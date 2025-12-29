#include "Game/World.hpp"
#include "Game/Chunk.hpp"
#include "Game/Game.h"
#include "Game/GameCommon.h"
#include "Game/Player.hpp"
#include "Game/BlockDefinition.hpp"
#include "Engine/Core/EngineCommon.h"
#include "Engine/Core/DebugRender.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Engine/Input/InputSystem.h"
#include "Engine/Math/MathUtils.h"

World::World(Game* owner)
	:m_theGame(owner)
{
}

World::~World()
{
	for (auto foundChunk = m_activeChunks.begin(); foundChunk != m_activeChunks.end(); ++foundChunk)
	{
		Chunk* chunk = foundChunk->second;
		if (chunk->m_needsSaving)
		{
			SaveChunkToFile(chunk);
		}
		if (chunk != nullptr)
		{
			delete chunk;
			chunk = nullptr;
		}
	}
	m_activeChunks.clear();
}

void World::Update(float deltaSeconds)
{
	UNUSED(deltaSeconds)
	Vec2 cameraPosXY = m_theGame->m_gameCamera->GetRenderCamera().GetPosition().GetXY();

	HandleDebugInput();

	UpdateMeshBuildQueue(cameraPosXY);
	BuildMeshesThisFrame();

	DeactivateFurthestChunk(cameraPosXY);
	QueueClosestMissingChunk(cameraPosXY);

	DispatchGenerateJobs();
	DispatchLoadAndSaveJobs();

	ProcessCompletedJobs();
}

void World::HandleDebugInput()
{
	XboxController const& controller = g_theInput->GetController(0);

	if (g_theInput->WasKeyJustPressed(KEYCODE_F2) || controller.WasButtonJustPressed(XBOX_BUTTON_A))
	{
		m_debugDrawMode = !m_debugDrawMode;
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_F4))
	{
		m_debugRaycast = !m_debugRaycast;
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_F3))
	{
		m_debugJobText = !m_debugJobText;
	}

	if (g_theInput->WasKeyJustPressed('K'))
	{
		m_lightingEnabled = !m_lightingEnabled;
	}
}

void World::Render() const
{
	for (auto foundChunk = m_activeChunks.begin(); foundChunk != m_activeChunks.end(); ++foundChunk)
	{
		Chunk* chunk = foundChunk->second;
		if (chunk != nullptr)
		{
			chunk->Render();
		}
	}

	RenderDebugModes();
}

void World::RenderDebugModes() const
{
	AABB2 gameSceneBounds = AABB2(Vec2::ZERO, Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y));

	if (m_debugDrawMode)
	{
		int totalVertices = 0;
		int totalIndices = 0;

		for (auto foundChunk = m_activeChunks.begin(); foundChunk != m_activeChunks.end(); ++foundChunk)
		{
			Chunk* chunk = foundChunk->second;
			if (chunk != nullptr)
			{
				DebugAddWorldWireAABB3(chunk->GetWorldBounds(), 0.0f);
				totalVertices += chunk->GetVertexCount();
				totalIndices += chunk->GetIndexCount();
			}
		}

		std::string chunkText = Stringf("Chunks: %d Vertices: %d Indices: %d ",
			static_cast<int>(m_activeChunks.size()), totalVertices, totalIndices);
		DebugAddScreenText(chunkText, gameSceneBounds, 15.f, Vec2(0.f, 0.97f), 0.f);
	}

	if (m_debugJobText)
	{
		std::string activeChunkText = Stringf("Chunks: %d", static_cast<int>(m_activeChunks.size()));
		std::string pendingJobsText = Stringf("Pending Jobs: %d", static_cast<int>(g_theJobSystem->m_pendingJobs.size()));
		std::string executingJobsText = Stringf("Executing Jobs: %d", static_cast<int>(g_theJobSystem->m_executingJobs.size()));
		std::string completedJobsText = Stringf("Completed Jobs: %d", static_cast<int>(g_theJobSystem->m_completedJobs.size()));

		DebugAddScreenText(activeChunkText, gameSceneBounds, 15.f, Vec2(0.f, 0.275f), 0.f);
		DebugAddScreenText(Stringf("Chunks pending save: %d", m_chunksQueuedForSave.size()), gameSceneBounds, 15.f, Vec2(0.f, 0.25f), 0.f);
		DebugAddScreenText(Stringf("Chunks saving: %d", m_outstandingSaveJobs), gameSceneBounds, 15.f, Vec2(0.f, 0.225f), 0.f);
		DebugAddScreenText(Stringf("Chunks pending load: %d", m_chunksQueuedForLoad.size()), gameSceneBounds, 15.f, Vec2(0.f, 0.2f), 0.f);
		DebugAddScreenText(Stringf("Chunks loading: %d", m_outstandingLoadJobs), gameSceneBounds, 15.f, Vec2(0.f, 0.175f), 0.f);
		DebugAddScreenText(Stringf("Chunks pending generation: %d", m_chunksQueuedForGeneration.size()), gameSceneBounds, 15.f, Vec2(0.f, 0.15f), 0.f);
		DebugAddScreenText(Stringf("Chunks generating: %d", m_outstandingGenerateJobs), gameSceneBounds, 15.f, Vec2(0.f, 0.125f), 0.f);
		DebugAddScreenText("JobSystem: ", gameSceneBounds, 15.f, Vec2(0.f, 0.1f), 0.f);
		DebugAddScreenText(pendingJobsText, gameSceneBounds, 15.f, Vec2(0.f, 0.075f), 0.f);
		DebugAddScreenText(executingJobsText, gameSceneBounds, 15.f, Vec2(0.f, 0.05f), 0.f);
		DebugAddScreenText(completedJobsText, gameSceneBounds, 15.f, Vec2(0.f, 0.025f), 0.f);
	}
}

void World::DeactivateFurthestChunk(Vec2 const& cameraPosXY)
{
	Chunk* farthestChunk = nullptr;
	float farthestDistSq = 0.f;

	for (auto const& pair : m_activeChunks)
	{
		Chunk* chunk = pair.second;
		IntVec2 chunkCenter = IntVec2(chunk->GetChunkCenter(chunk->m_chunkCoords));
		Vec2 chunkCenterAsVec2 = Vec2(static_cast<float>(chunkCenter.x), static_cast<float>(chunkCenter.y));
		float distSq = GetDistanceSquared2D(cameraPosXY, chunkCenterAsVec2);

		if (distSq > CHUNK_DEACTIVATION_RANGE * CHUNK_DEACTIVATION_RANGE && distSq > farthestDistSq)
		{
			farthestDistSq = distSq;
			farthestChunk = chunk;
		}
	}

	if (farthestChunk)
	{
		DeActivateChunk(farthestChunk);
	}
}

void World::QueueClosestMissingChunk(Vec2 const& cameraPosXY)
{
	if (static_cast<int>(m_activeChunks.size()) >= MAX_ACTIVE_CHUNKS)
	{
		return;
	}

	IntVec2 cameraChunkCoords = GetChunkCoordsFromWorldPos(cameraPosXY);
	IntVec2 closestCoords = IntVec2::ZERO;
	float   closestDistSq = FLT_MAX;
	int     chunksToProcess = 25;

	for (int chunkX = -CHUNK_ACTIVATION_RADIUS_X; chunkX <= CHUNK_ACTIVATION_RADIUS_X; ++chunkX)
	{
		for (int chunkY = -CHUNK_ACTIVATION_RADIUS_Y; chunkY <= CHUNK_ACTIVATION_RADIUS_Y; ++chunkY)
		{
			if (chunksToProcess <= 0)
			{
				return;
			}

			IntVec2 coords = cameraChunkCoords + IntVec2(chunkX, chunkY);
			if (m_activeChunks.find(coords) != m_activeChunks.end())
			{
				continue;
			}

			Vec2 chunkCenter = Vec2(coords.x * CHUNK_SIZE_X + CHUNK_SIZE_X / 2.f, coords.y * CHUNK_SIZE_Y + CHUNK_SIZE_Y / 2.f);
			float distSq = GetDistanceSquared2D(cameraPosXY, chunkCenter);

			if (distSq <= CHUNK_ACTIVATION_RANGE * CHUNK_ACTIVATION_RANGE && distSq < closestDistSq)
			{
				closestDistSq = distSq;
				closestCoords = coords;

				Chunk* newChunk = new Chunk(closestCoords);
				ActivateChunk(newChunk);
				chunksToProcess -= 1;
			}
		}
	}
}

void World::UpdateMeshBuildQueue(Vec2 const& cameraPosXY)
{
	m_meshBuildQueue.clear();

	// Gather dirty chunks that within build range
	for (auto& pair : m_activeChunks)
	{
		Chunk* chunk = pair.second;
		if (chunk && chunk->m_isMeshDirty)
		{
			IntVec2 chunkCenter = chunk->GetChunkCenter(chunk->m_chunkCoords);
			Vec2 chunkCenterVec2(static_cast<float>(chunkCenter.x), static_cast<float>(chunkCenter.y));
			float chunkDistSquared = GetDistanceSquared2D(cameraPosXY, chunkCenterVec2);

			if (chunkDistSquared <= CHUNK_MESH_BUILD_RANGE)
			{
				m_meshBuildQueue.push_back(chunk);
			}
		}
	}

	// Sort mesh build queue by distance to camera
	for (int chunkMeshIndex = 0; chunkMeshIndex < static_cast<int>(m_meshBuildQueue.size()); ++chunkMeshIndex)
	{
		int closestChunkIndex = chunkMeshIndex;
		float closestChunkDistSquared = GetChunkDistSquaredToCamera(m_meshBuildQueue[chunkMeshIndex], cameraPosXY);

		for (int nextChunkMeshIndex = chunkMeshIndex + 1; nextChunkMeshIndex < static_cast<int>(m_meshBuildQueue.size()); ++nextChunkMeshIndex)
		{
			float nextChunkDistSquared = GetChunkDistSquaredToCamera(m_meshBuildQueue[nextChunkMeshIndex], cameraPosXY);
			if (nextChunkDistSquared < closestChunkDistSquared)
			{
				closestChunkDistSquared = nextChunkDistSquared;
				closestChunkIndex = nextChunkMeshIndex;
			}
		}

		if (closestChunkIndex != chunkMeshIndex)
		{
			std::swap(m_meshBuildQueue[chunkMeshIndex], m_meshBuildQueue[closestChunkIndex]);
		}
	}
}

void World::BuildMeshesThisFrame()
{
	ProcessDirtyLighting();
	int chunkBuildCount = 0;

	for (int chunkMeshIndex = 0; chunkMeshIndex < static_cast<int>(m_meshBuildQueue.size()); ++chunkMeshIndex)
	{
		if (chunkBuildCount >= MAX_MESHES_PER_FRAME)
		{
			break;
		}

		Chunk* chunk = m_meshBuildQueue[chunkMeshIndex];

		if (chunk && chunk->m_isMeshDirty)
		{
			// Wait to generate until all neighbors are active
			if (!chunk->m_northNeighbor || !chunk->m_southNeighbor || !chunk->m_eastNeighbor || !chunk->m_westNeighbor)
			{
				continue;
			}

			chunk->GenerateChunkMesh();
			chunk->m_isMeshDirty = false;
			chunkBuildCount += 1;
		}
	}

	CleanUpMeshBuildQueue();
}

void World::CleanUpMeshBuildQueue()
{
	for (int meshIndex = 0; meshIndex < static_cast<int>(m_meshBuildQueue.size());)
	{
		if (!m_meshBuildQueue[meshIndex]->m_isMeshDirty)
		{
			m_meshBuildQueue.erase(m_meshBuildQueue.begin() + meshIndex);
		}
		else
		{
			++meshIndex;
		}
	}
}

void World::DispatchGenerateJobs()
{
	while (!m_chunksQueuedForGeneration.empty() && m_outstandingGenerateJobs < MAX_GENERATION_JOBS)
	{
		Chunk* chunk = m_chunksQueuedForGeneration.front();
		m_chunksQueuedForGeneration.pop_front();

		chunk->m_chunkState.store(ChunkState::ACTIVATING_GENERATING);

		GenerateChunkJob* job = new GenerateChunkJob(chunk);
		g_theJobSystem->AddJobToSystem(job);
		m_outstandingGenerateJobs += 1;

		if (m_outstandingGenerateJobs >= MAX_GENERATION_JOBS)
		{
			break;
		}
	}
}

void World::DispatchLoadAndSaveJobs()
{
	// Load jobs
	while (!m_chunksQueuedForLoad.empty() && m_outstandingLoadJobs < MAX_LOAD_JOBS)
	{
		Chunk* chunk = m_chunksQueuedForLoad.front();
		m_chunksQueuedForLoad.pop_front();

		chunk->m_chunkState.store(ChunkState::ACTIVATING_LOADING);

		LoadChunkJob* job = new LoadChunkJob(chunk);
		g_theJobSystem->AddJobToSystem(job);
		m_outstandingLoadJobs += 1;
	}

	// Save jobs
	while (!m_chunksQueuedForSave.empty() && m_outstandingSaveJobs < MAX_SAVE_JOBS)
	{
		Chunk* chunk = m_chunksQueuedForSave.front();
		m_chunksQueuedForSave.pop_front();

		chunk->m_chunkState.store(ChunkState::DEACTIVATING_SAVING);

		SaveChunkJob* job = new SaveChunkJob(chunk);
		g_theJobSystem->AddJobToSystem(job);
		m_outstandingSaveJobs += 1;
	}
}

void World::ProcessCompletedJobs()
{
	Job* completedJob = nullptr;
	while ((completedJob = g_theJobSystem->RetreiveCompletedJob()) != nullptr)
	{
		if (GenerateChunkJob* genJob = dynamic_cast<GenerateChunkJob*>(completedJob))
		{
			Chunk* chunk = genJob->m_chunk;
			if (chunk->m_chunkState.load() == ChunkState::ACTIVATING_GENERATE_COMPLETE)
			{
				FinalizeActivatedChunk(chunk);
			}
			delete genJob;
			m_outstandingGenerateJobs -= 1;
		}
		else if (LoadChunkJob* loadJob = dynamic_cast<LoadChunkJob*>(completedJob))
		{
			Chunk* chunk = loadJob->m_chunk;
			if (chunk->m_chunkState.load() == ChunkState::ACTIVATING_LOAD_COMPLETE)
			{
				FinalizeActivatedChunk(chunk);
			}
			delete loadJob;
			m_outstandingLoadJobs -= 1;
		}
		else if (SaveChunkJob* saveJob = dynamic_cast<SaveChunkJob*>(completedJob))
		{
			Chunk* chunk = saveJob->m_chunk;
			if (chunk->m_chunkState.load() == ChunkState::DEACTIVATING_SAVE_COMPLETE)
			{
				chunk->DeleteBuffers();
				delete chunk;
			}
			delete saveJob;
			m_outstandingSaveJobs -= 1;
		}
		else
		{
			delete completedJob;
		}
	}
}

void World::ActivateChunk(Chunk* chunkToActivate)
{
	if (m_activeChunks.find(chunkToActivate->m_chunkCoords) != m_activeChunks.end())
	{
		return;
	}

	std::string filename = Stringf("Saves/Chunk(%i,%i).chunk", chunkToActivate->m_chunkCoords.x, chunkToActivate->m_chunkCoords.y);
	if (DoesFileExist(filename))
	{
		chunkToActivate->m_chunkState.store(ChunkState::ACTIVATING_QUEUED_LOAD);
		m_chunksQueuedForLoad.push_back(chunkToActivate);
	}
	else
	{
		chunkToActivate->m_chunkState.store(ChunkState::ACTIVATING_QUEUED_GENERATE);
		m_chunksQueuedForGeneration.push_back(chunkToActivate);
	}
}

void World::FinalizeActivatedChunk(Chunk* chunkToActivate)
{
	if (m_activeChunks.find(chunkToActivate->m_chunkCoords) != m_activeChunks.end())
	{
		return;
	}

	// Add to active chunks
	m_activeChunks[chunkToActivate->m_chunkCoords] = chunkToActivate;

	// Mark mesh dirty so it'll be processed
	chunkToActivate->m_isMeshDirty = true;

	// This chunk has just been activated from a clean state
	chunkToActivate->m_needsSaving = false;

	// Hook up neighbors
	HookUpNeighbors(chunkToActivate);

	// Initialize lighting
	InitializeChunkLighting(chunkToActivate);
}

void World::HookUpNeighbors(Chunk* chunkToActivate)
{
	// Check and set North
	IntVec2 northCoords = chunkToActivate->m_chunkCoords + IntVec2::NORTH;
	if (m_activeChunks.find(northCoords) != m_activeChunks.end())
	{
		chunkToActivate->m_northNeighbor = m_activeChunks[northCoords];
		m_activeChunks[northCoords]->m_southNeighbor = chunkToActivate;
	}

	// Check and set South
	IntVec2 southCoords = chunkToActivate->m_chunkCoords + IntVec2::SOUTH;
	if (m_activeChunks.find(southCoords) != m_activeChunks.end())
	{
		chunkToActivate->m_southNeighbor = m_activeChunks[southCoords];
		m_activeChunks[southCoords]->m_northNeighbor = chunkToActivate;
	}

	// Check and set East
	IntVec2 eastCoords = chunkToActivate->m_chunkCoords + IntVec2::EAST;
	if (m_activeChunks.find(eastCoords) != m_activeChunks.end())
	{
		chunkToActivate->m_eastNeighbor = m_activeChunks[eastCoords];
		m_activeChunks[eastCoords]->m_westNeighbor = chunkToActivate;
	}

	// Check and set West
	IntVec2 westCoords = chunkToActivate->m_chunkCoords + IntVec2::WEST;
	if (m_activeChunks.find(westCoords) != m_activeChunks.end())
	{
		chunkToActivate->m_westNeighbor = m_activeChunks[westCoords];
		m_activeChunks[westCoords]->m_eastNeighbor = chunkToActivate;
	}
}

void World::DeActivateChunk(Chunk* chunkToDeActivate)
{
	if (m_activeChunks.find(chunkToDeActivate->m_chunkCoords) == m_activeChunks.end())
	{
		return;
	}

	// Remove from neighbors
	RemoveFromNeighbors(chunkToDeActivate);

	// Remove from active map
	m_activeChunks.erase(chunkToDeActivate->m_chunkCoords);

	if (chunkToDeActivate->m_needsSaving)
	{
		chunkToDeActivate->m_chunkState.store(ChunkState::DEACTIVATING_QUEUED_SAVE);
		m_chunksQueuedForSave.push_back(chunkToDeActivate);
	}
	else
	{
		chunkToDeActivate->DeleteBuffers();
		delete chunkToDeActivate;
	}
}

void World::RemoveFromNeighbors(Chunk* chunkToDeActivate)
{
	if (chunkToDeActivate->m_northNeighbor != nullptr)
	{
		chunkToDeActivate->m_northNeighbor->m_southNeighbor = nullptr;
	}
	if (chunkToDeActivate->m_southNeighbor != nullptr)
	{
		chunkToDeActivate->m_southNeighbor->m_northNeighbor = nullptr;
	}
	if (chunkToDeActivate->m_eastNeighbor != nullptr)
	{
		chunkToDeActivate->m_eastNeighbor->m_westNeighbor = nullptr;
	}
	if (chunkToDeActivate->m_westNeighbor != nullptr)
	{
		chunkToDeActivate->m_westNeighbor->m_eastNeighbor = nullptr;
	}
}

void World::SaveChunkToFile(Chunk* chunkToSave)
{
	std::vector<uint8_t> byteInBuffer;

	// Write the header
	ChunkFileHeader header;
	byteInBuffer.push_back(header.m_g);
	byteInBuffer.push_back(header.m_c);
	byteInBuffer.push_back(header.m_h);
	byteInBuffer.push_back(header.m_k);
	byteInBuffer.push_back(header.m_version);
	byteInBuffer.push_back(header.m_bitsX);
	byteInBuffer.push_back(header.m_bitsY);
	byteInBuffer.push_back(header.m_bitsZ);

	// Encode the RLE
	uint8_t currentBlockType = chunkToSave->m_blocks[0].GetBlockType();
	uint8_t runLength = 1;

	for (int blockIndex = 1; blockIndex < CHUNK_BLOCK_TOTAL; ++blockIndex)
	{
		uint8_t blockType = chunkToSave->m_blocks[blockIndex].GetBlockType();
		if (blockType == currentBlockType && runLength < 255)
		{
			runLength += 1;
		}
		else
		{
			// Storing the run
			byteInBuffer.push_back(currentBlockType);
			byteInBuffer.push_back(runLength);

			// Starting a new run
			currentBlockType = blockType;
			runLength = 1;
		}
	}

	// Storing final run
	byteInBuffer.push_back(currentBlockType);
	byteInBuffer.push_back(runLength);

	// Writing the buffer to file
	std::string filename = Stringf("Saves/Chunk(%d,%d).chunk", chunkToSave->m_chunkCoords.x, chunkToSave->m_chunkCoords.y);
	WriteBufferToFile(byteInBuffer, filename);
}

void World::LoadChunkFromFile(Chunk* chunkToLoad)
{
	std::vector<uint8_t> outByteBuffer;

	// Read the file
	std::string filename = Stringf("Saves/Chunk(%d,%d).chunk", chunkToLoad->m_chunkCoords.x, chunkToLoad->m_chunkCoords.y);
	FileReadToBuffer(outByteBuffer, filename);

	// Read the header
	uint8_t* outBufferPointer = outByteBuffer.data();
	ChunkFileHeader* header = reinterpret_cast<ChunkFileHeader*>(outBufferPointer);

	if (header->m_g != 'G' || header->m_c != 'C' || header->m_h != 'H' || header->m_k != 'K')
	{
		ERROR_AND_DIE("File header does not match ChunkFileHeader!");
		return;
	}

	// Increment the current buffer data pointer by the size of a header
	outBufferPointer += sizeof(ChunkFileHeader);

	// Decode the RLE
	int blockIndex = 0;
	while (outBufferPointer < outByteBuffer.data() + outByteBuffer.size())
	{
		uint8_t blockType = *outBufferPointer++;
		uint8_t runLength = *outBufferPointer++;

		// Set the blocks
		for (int setBlockIndex = 0; setBlockIndex < runLength; ++setBlockIndex)
		{
			chunkToLoad->m_blocks[blockIndex++].SetBlockType(blockType);
		}
	}
}

void World::ProcessDirtyLighting()
{
	// Processing light blocks until none remain
	while (!m_dirtyLightBlocks.empty())
	{
		ProcessNextDirtyLightBlock();
	}
}

void World::ProcessNextDirtyLightBlock()
{
	if (m_dirtyLightBlocks.empty())
	{
		return;
	}

	// Popping the first block in the queue
	BlockIterator blockIterator = m_dirtyLightBlocks.front();
	m_dirtyLightBlocks.pop_front();

	Block* block = blockIterator.GetBlock();
	if (block == nullptr)
	{
		return;
	}

	block->SetIsLightDirty(false);

	uint8_t correctIndoor, correctOutdoor;
	ComputeCorrectLightInfluence(blockIterator, correctIndoor, correctOutdoor);

	if (correctIndoor != block->GetIndoorLight() || correctOutdoor != block->GetOutdoorLight())
	{
		block->SetIndoorLight(correctIndoor);
		block->SetOutdoorLight(correctOutdoor);

		// Mark all surrounding chunk meshes dirty
		MarkChunkMeshesDirtyAround(blockIterator);

		// Propagate
		BlockIterator neighbors[6] = 
		{
			blockIterator.GetNorthNeighbor(),
			blockIterator.GetSouthNeighbor(),
			blockIterator.GetEastNeighbor(),
			blockIterator.GetWestNeighbor(),
			blockIterator.GetUpNeighbor(),
			blockIterator.GetDownNeighbor()
		};


		for (BlockIterator const& neighborIterator : neighbors)
		{
			Block* neighborBlock = neighborIterator.GetBlock();
			if (!neighborBlock)
			{
				continue;
			}

			if (!neighborBlock->IsFullOpaque())
			{
				MarkLightingDirty(neighborIterator);
			}
		}
	}
}

void World::MarkLightingDirty(BlockIterator const& blockIterator)
{
	Block* block = blockIterator.GetBlock();

	// Check to make sure we have a block
	if (block == nullptr)
	{
		return;
	}

	// Check if it is already marked dirty
	if (block->IsLightDirty())
	{
		return;
	}

	// Mark and add to queue
	block->SetIsLightDirty(true);
	m_dirtyLightBlocks.push_back(blockIterator);
}

void World::MarkLightingDirtyIfNotOpaque(BlockIterator const& blockIterator)
{
	Block* block = blockIterator.GetBlock();
	if (block == nullptr)
	{
		return;
	}

	if (!block->IsFullOpaque())
	{
		MarkLightingDirty(blockIterator);
	}
}

void World::InitializeChunkLighting(Chunk* chunk)
{
	if (!chunk)
	{
		return;
	}

	for (int blockIndex = 0; blockIndex < CHUNK_BLOCK_TOTAL; ++blockIndex)
	{
		Block& block = chunk->m_blocks[blockIndex];
		block.SetIndoorLight(0);
		block.SetOutdoorLight(0);
		block.SetIsLightDirty(false);
		block.SetIsSky(false);
	}

	MarkBoundaryBlocksDirty(chunk);
	MarkSkyAndOutdoorLight(chunk);
	MarkEmissiveBlocksDirty(chunk);
}

void World::MarkBoundaryBlocksDirty(Chunk* chunk)
{
	for (int chunkZ = 0; chunkZ < CHUNK_SIZE_Z; ++chunkZ)
	{
		for (int chunkX = 0; chunkX < CHUNK_SIZE_X; ++chunkX)
		{
			for (int chunkY = 0; chunkY < CHUNK_SIZE_Y; ++chunkY)
			{
				if (chunkX == 0 || chunkX == CHUNK_SIZE_X - 1 || chunkY == 0 || chunkY == CHUNK_SIZE_Y - 1)
				{
					BlockIterator blockIterator(chunk, chunk->GetBlockIndex(chunkX, chunkY, chunkZ));
					Chunk* neighbor = nullptr;

					if (chunkX == 0 && chunk->m_westNeighbor)
					{
						neighbor = chunk->m_westNeighbor;
					}
					if (chunkX == CHUNK_SIZE_X - 1 && chunk->m_eastNeighbor)
					{
						neighbor = chunk->m_eastNeighbor;
					}
					if (chunkY == 0 && chunk->m_southNeighbor)
					{
						neighbor = chunk->m_southNeighbor;
					}
					if (chunkY == CHUNK_SIZE_Y - 1 && chunk->m_northNeighbor)
					{
						neighbor = chunk->m_northNeighbor;
					}

					if (neighbor)
					{
						MarkLightingDirtyIfNotOpaque(blockIterator);
					}
				}
			}
		}
	}
}

void World::MarkSkyAndOutdoorLight(Chunk* chunk)
{
	for (int chunkX = 0; chunkX < CHUNK_SIZE_X; ++chunkX)
	{
		for (int chunkY = 0; chunkY < CHUNK_SIZE_Y; ++chunkY)
		{
			bool isSkyBlocked = false;

			for (int chunkZ = CHUNK_SIZE_Z - 1; chunkZ >= 0; --chunkZ)
			{
				BlockIterator blockIterator(chunk, chunk->GetBlockIndex(chunkX, chunkY, chunkZ));
				Block* block = blockIterator.GetBlock();
				if (!block)
				{
					continue;
				}

				if (!isSkyBlocked && !block->IsFullOpaque())
				{
					// This block sees the sky
					block->SetIsSky(true);
					block->SetOutdoorLight(15);

					MarkLightingDirty(blockIterator);

					// Marking horizontal air neighbors dirty
					MarkLightingDirtyIfNotOpaque(blockIterator.GetNorthNeighbor());
					MarkLightingDirtyIfNotOpaque(blockIterator.GetSouthNeighbor());
					MarkLightingDirtyIfNotOpaque(blockIterator.GetEastNeighbor());
					MarkLightingDirtyIfNotOpaque(blockIterator.GetWestNeighbor());
				}
				else if (block->IsFullOpaque())
				{
					isSkyBlocked = true;
				}
			}
		}
	}
}

void World::MarkEmissiveBlocksDirty(Chunk* chunk)
{
	for (int blockIndex = 0; blockIndex < CHUNK_BLOCK_TOTAL; ++blockIndex)
	{
		BlockIterator blockIterator(chunk, blockIndex);
		Block* block = blockIterator.GetBlock();
		if (!block)
		{
			continue;
		}

		BlockDefinition* blockDef = BlockDefinition::s_blockDefs[block->GetBlockType()];
		if (blockDef->m_indoorLight > 0 || blockDef->m_outdoorLight > 0)
		{
			MarkLightingDirty(blockIterator);
		}
	}
}

void World::MarkChunkMeshesDirtyAround(BlockIterator const& blockIterator)
{
	Chunk* centerChunk = blockIterator.GetChunk();
	if (!centerChunk)
	{
		return;
	}

	// Mark the chunk containing the block
	centerChunk->m_isMeshDirty = true;

	// Mark neighbor chunks that may need to rebuild mesh due to shared faces
	BlockIterator neighbors[6] = 
	{
		blockIterator.GetNorthNeighbor(),
		blockIterator.GetSouthNeighbor(),
		blockIterator.GetEastNeighbor(),
		blockIterator.GetWestNeighbor(),
		blockIterator.GetUpNeighbor(),
		blockIterator.GetDownNeighbor()
	};

	for (BlockIterator const& neighbor : neighbors)
	{
		if (!neighbor.IsValid())
		{
			continue;
		}

		Chunk* neighborChunk = neighbor.GetChunk();
		if (neighborChunk && neighborChunk != centerChunk)
		{
			neighborChunk->m_isMeshDirty = true;
		}
	}
}

void World::ComputeCorrectLightInfluence(BlockIterator const& blockIter, uint8_t& outIndoor, uint8_t& outOutdoor)
{
	Block* block = blockIter.GetBlock();
	if (!block)
	{
		outIndoor = 0;
		outOutdoor = 0;
		return;
	}

	BlockDefinition* def = BlockDefinition::s_blockDefs[block->GetBlockType()];

	float indoor = 0;
	float outdoor = 0;

	// Sky blocks get full outdoor lighting
	if (block->IsSky() && !block->IsFullOpaque())
	{
		outdoor = 15;
	}

	// Getting the emissive block itself
	indoor = GetMax(indoor, static_cast<float>(def->m_indoorLight));
	outdoor = GetMax(outdoor, static_cast<float>(def->m_outdoorLight));

	// Non-opaque and propagate
	if (!block->IsFullOpaque())
	{
		BlockIterator neighbors[6] = 
		{
			blockIter.GetNorthNeighbor(),
			blockIter.GetSouthNeighbor(),
			blockIter.GetEastNeighbor(),
			blockIter.GetWestNeighbor(),
			blockIter.GetUpNeighbor(),
			blockIter.GetDownNeighbor()
		};

		for (BlockIterator const& neighboringIterator : neighbors)
		{
			Block* neighborBlock = neighboringIterator.GetBlock();
			if (!neighborBlock)
			{
				continue;
			}

			indoor = GetMax(indoor, static_cast<uint8_t>((neighborBlock->GetIndoorLight() > 0 ? neighborBlock->GetIndoorLight() - 1 : 0)));
			outdoor = GetMax(outdoor, static_cast<uint8_t>((neighborBlock->GetOutdoorLight() > 0 ? neighborBlock->GetOutdoorLight() - 1 : 0)));
		}
	}

	outIndoor = static_cast<uint8_t>(indoor);
	outOutdoor = static_cast<uint8_t>(outdoor);
}

IntVec2 World::GetChunkCoordsFromWorldPos(Vec2 const& worldPos) const
{
	int chunkX = RoundDownToInt(worldPos.x / static_cast<float>(CHUNK_SIZE_X));
	int chunkY = RoundDownToInt(worldPos.y / static_cast<float>(CHUNK_SIZE_Y));
	return IntVec2(chunkX, chunkY);
}

IntVec2 World::GetGlobalChunkCoords(IntVec3 const& globalCoords) const
{
	int chunkX = RoundDownToInt(static_cast<float>(globalCoords.x) / CHUNK_SIZE_X);
	int chunkY = RoundDownToInt(static_cast<float>(globalCoords.y) / CHUNK_SIZE_Y);
	return IntVec2(chunkX, chunkY);
}

IntVec3 World::GetChunkCoordsFromPosition(Vec3 const& position) const
{
	int positionX = RoundDownToInt(position.x);
	int positionY = RoundDownToInt(position.y);
	int positionZ = RoundDownToInt(position.z);
	return IntVec3(positionX, positionY, positionZ);
}

float World::GetChunkDistSquaredToCamera(Chunk* chunk, Vec2 const& cameraPosXY)
{
	IntVec2 chunkCenter = chunk->GetChunkCenter(chunk->m_chunkCoords);
	Vec2 centerAsVec2 = Vec2(static_cast<float>(chunkCenter.x), static_cast<float>(chunkCenter.y));
	return GetDistanceSquared2D(cameraPosXY, centerAsVec2);
}

Chunk* World::GetWorldChunk(IntVec2 chunkCoords) const
{
	auto foundChunk = m_activeChunks.find(chunkCoords);
	if (foundChunk != m_activeChunks.end())
	{
		return foundChunk->second;
	}
	return nullptr;
}

Chunk* World::GetChunkForWorldPos(Vec3 const& worldPos) const
{
	IntVec2 chunkCoords = GetChunkCoordsFromWorldPos(Vec2(worldPos.x, worldPos.y));
	auto foundChunk = m_activeChunks.find(chunkCoords);
	if (foundChunk != m_activeChunks.end())
	{
		return foundChunk->second;
	}
	return nullptr;
}

bool World::SetBlockTypeAtCoords(IntVec3 const& globalCoords, uint8_t blockTypeIndex)
{
	IntVec2 chunkCoords = GetGlobalChunkCoords(globalCoords);
	Chunk* chunk = GetWorldChunk(chunkCoords);

	// Checking if we have a chunk
	if (chunk == nullptr)
	{
		return false;
	}

	IntVec3 localCoords = chunk->GlobalCoordsToLocalCoords(globalCoords);
	chunk->SetBlockType(localCoords.x, localCoords.y, localCoords.z, blockTypeIndex);
	return true;
}

uint8_t World::GetBlockTypeAtCoords(IntVec3 const& globalCoords)
{
	IntVec2 chunkCoords = GetGlobalChunkCoords(globalCoords);
	Chunk* chunk = GetWorldChunk(chunkCoords);

	// Checking if we have a chunk
	if (chunk == nullptr)
	{
		return BLOCKTYPE_AIR;
	}

	IntVec3 blockLocal = chunk->GlobalCoordsToLocalCoords(globalCoords);
	Block* block = chunk->GetBlockAtLocalCoords(blockLocal.x, blockLocal.y, blockLocal.z);

	// Checking if we have a block
	if (block == nullptr)
	{
		return BLOCKTYPE_AIR;
	}

	// Found a valid block!
	return block->m_blockType;
}

void World::PropagateSkyDown(BlockIterator start)
{
	BlockIterator blockIterator = start;

	while (blockIterator.IsValid())
	{
		Block* block = blockIterator.GetBlock();
		if (!block)
		{
			break;
		}

		if (block->IsFullOpaque())
		{
			break;
		}

		block->SetIsSky(true);
		MarkLightingDirty(blockIterator);
		blockIterator = blockIterator.GetDownNeighbor();
	}
}

void World::ClearSkyDown(BlockIterator start)
{
	BlockIterator blockIterator = start;

	while (blockIterator.IsValid())
	{
		Block* block = blockIterator.GetBlock();
		if (!block)
		{
			break;
		}

		if (block->IsFullOpaque())
		{
			break;
		}


		block->SetIsSky(false);
		MarkLightingDirty(blockIterator);
		blockIterator = blockIterator.GetDownNeighbor();
	}
}

GameRaycastResult3D  World::RaycastVsBlocks(Vec3 const& rayStartPos, Vec3 const& rayDir, float maxDist) const
{
	GameRaycastResult3D result;

	// Early exit to check if we are outside a chunk
	if (rayStartPos.z < 0.f || rayStartPos.z > static_cast<float>(CHUNK_SIZE_Z))
	{
		return result;
	}

	Chunk* startChunk = GetChunkForWorldPos(rayStartPos);

	// Check to see if we have a chunk
	if (!startChunk)
	{
		return result;
	}

	// Initializing voxel stepping
	IntVec3 currentBlock = IntVec3(
		RoundDownToInt(rayStartPos.x),
		RoundDownToInt(rayStartPos.y),
		RoundDownToInt(rayStartPos.z)
	);

	int blockIndex = startChunk->GetBlockIndex(startChunk->GetBlockCoordsFromWorldPos(rayStartPos));
	BlockIterator it(startChunk, blockIndex);

	Vec3 rayStep = ComputeRayStep(rayDir);
	Vec3 rayLength = ComputeInitialRayLength(rayStartPos, rayDir, currentBlock, rayStep);
	IntVec3 stepDir = ComputeStepDir(rayDir);

	float traveled = 0.f;

	while (traveled < maxDist)
	{
		Block* block = it.GetBlock();

		// Check if we have a block
		if (!block)
		{
			return result;
		}

		// Check if our impacted block is solid, if so we have an impact
		if (block->IsSolid())
		{
			result.m_didImpact = true;
			result.m_impactDist = traveled;
			result.m_impactPos = rayStartPos + rayDir * traveled;
			result.m_impactedBlockIterator = it;
			return result;
		}

		// Step to next voxel
		if (rayLength.x < rayLength.y && rayLength.x < rayLength.z)
		{
			if (stepDir.x == 1)
			{
				it = it.GetEastNeighbor();
			}
			else
			{
				it = it.GetWestNeighbor();
			}
			traveled = rayLength.x;
			rayLength.x += rayStep.x;
			result.m_impactNormal = -Vec3::XAXE * static_cast<float>(stepDir.x);
		}
		else if (rayLength.y < rayLength.z)
		{
			if (stepDir.y == 1)
			{
				it = it.GetNorthNeighbor();
			}
			else
			{
				it = it.GetSouthNeighbor();
			}
			traveled = rayLength.y;
			rayLength.y += rayStep.y;
			result.m_impactNormal = -Vec3::YAXE * static_cast<float>(stepDir.y);
		}
		else
		{
			if (stepDir.z == 1)
			{
				it = it.GetUpNeighbor();
			}
			else
			{
				it = it.GetDownNeighbor();
			}
			traveled = rayLength.z;
			rayLength.z += rayStep.z;
			result.m_impactNormal = -Vec3::ZAXE * static_cast<float>(stepDir.z);
		}
	}

	return result;
}
// -----------------------------------------------------------------------------
void GenerateChunkJob::Execute()
{
	if (!m_chunk)
	{
		return;
	}

	// Generates the chunks mesh
	m_chunk->PopulateWithDensityNoise();

	// Mark chunk as complete
	m_chunk->m_chunkState.store(ChunkState::ACTIVATING_GENERATE_COMPLETE);
}
// -----------------------------------------------------------------------------
void SaveChunkJob::Execute()
{
	if (!m_chunk)
	{
		return;
	}

	// Save the chunk to file
	g_theGame->m_currentWorld->SaveChunkToFile(m_chunk);

	// Mark chunk as complete
	m_chunk->m_chunkState.store(ChunkState::DEACTIVATING_SAVE_COMPLETE);
}
// -----------------------------------------------------------------------------
void LoadChunkJob::Execute()
{
	if (!m_chunk)
	{
		return;
	}

	// Load chunk from file
	g_theGame->m_currentWorld->LoadChunkFromFile(m_chunk);

	// Mark chunk as complete
	m_chunk->m_chunkState.store(ChunkState::ACTIVATING_LOAD_COMPLETE);
}
