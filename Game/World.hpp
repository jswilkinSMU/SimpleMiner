#pragma once
#include "Game/GameCommon.h"
#include "Game/BlockIterator.hpp"
#include "Engine/Math/IntVec2.h"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/RaycastUtils.hpp"
#include "Engine/Core/JobSystem.hpp"
#include <unordered_map>
// -----------------------------------------------------------------------------
class Game;
class Chunk;
// -----------------------------------------------------------------------------
namespace std
{
	template <>
	struct hash<IntVec2>
	{
		std::size_t operator()(IntVec2 const& coords) const noexcept
		{
			return (std::hash<int>()(coords.x) * 73856093) ^ (std::hash<int>()(coords.y) * 19349663);
		}
	};
}
// -----------------------------------------------------------------------------
class GenerateChunkJob : public Job
{
public:
	GenerateChunkJob(Chunk* chunk) : m_chunk(chunk) {}
	virtual void Execute() override;

public:
	Chunk* m_chunk = nullptr;
};
// -----------------------------------------------------------------------------
class SaveChunkJob : public Job
{
public:
	SaveChunkJob(Chunk* chunk) : m_chunk(chunk) {}
	virtual void Execute() override;

public:
	Chunk* m_chunk = nullptr;
};
// -----------------------------------------------------------------------------
class LoadChunkJob : public Job
{
public:
	LoadChunkJob(Chunk* chunk) : m_chunk(chunk) {}
	virtual void Execute() override;

public:
	Chunk* m_chunk = nullptr;
};
// -----------------------------------------------------------------------------
struct GameRaycastResult3D : public RaycastResult3D
{
	BlockIterator m_impactedBlockIterator = BlockIterator(nullptr, -1);
};
// -----------------------------------------------------------------------------
class World
{
public:
	World(Game* owner);
	~World();

	// Updating
	void Update(float deltaSeconds);
	void HandleDebugInput();

	// Rendering
	void Render() const;
	void RenderDebugModes() const;

	// Processing
	void DeactivateFurthestChunk(Vec2 const& cameraPosXY);
	void QueueClosestMissingChunk(Vec2 const& cameraPosXY);

	// Mesh
	void UpdateMeshBuildQueue(Vec2 const& cameraPosXY);
	void BuildMeshesThisFrame();
	void CleanUpMeshBuildQueue();
	
	// Jobs
	void DispatchGenerateJobs();
	void DispatchLoadAndSaveJobs();
	void ProcessCompletedJobs();

	// Chunk Activation
	void ActivateChunk(Chunk* chunkToActivate);
	void FinalizeActivatedChunk(Chunk* chunkToActivate);
	void HookUpNeighbors(Chunk* chunkToActivate);
	
	// Chunk Deactivation
	void DeActivateChunk(Chunk* chunkToDeActivate);
	void RemoveFromNeighbors(Chunk* chunkToDeActivate);

	// Saving and Loading
	void SaveChunkToFile(Chunk* chunkToSave);
	void LoadChunkFromFile(Chunk* chunkToLoad);

	// Lighting
	void ProcessDirtyLighting();
	void ProcessNextDirtyLightBlock();
	void MarkLightingDirty(BlockIterator const& blockIterator);
	void MarkLightingDirtyIfNotOpaque(BlockIterator const& blockIterator);
	void InitializeChunkLighting(Chunk* chunk);
	void MarkBoundaryBlocksDirty(Chunk* chunk);
	void MarkSkyAndOutdoorLight(Chunk* chunk);
	void MarkEmissiveBlocksDirty(Chunk* chunk);
	void MarkChunkMeshesDirtyAround(BlockIterator const& blockIterator);
	void ComputeCorrectLightInfluence(BlockIterator const& blockIter, uint8_t& outIndoor, uint8_t& outOutdoor);

	// World utilities
	IntVec2 GetChunkCoordsFromWorldPos(Vec2 const& worldPos) const;
	IntVec2 GetGlobalChunkCoords(IntVec3 const& globalCoords) const;
	IntVec3 GetChunkCoordsFromPosition(Vec3 const& position) const;
	float   GetChunkDistSquaredToCamera(Chunk* chunk, Vec2 const& cameraPosXY);
	Chunk*  GetWorldChunk(IntVec2 chunkCoords) const;
	Chunk*  GetChunkForWorldPos(Vec3 const& worldPos) const;
	bool	SetBlockTypeAtCoords(IntVec3 const& globalCoords, uint8_t blockTypeIndex);
	uint8_t GetBlockTypeAtCoords(IntVec3 const& globalCoords);
	void    PropagateSkyDown(BlockIterator start);
	void    ClearSkyDown(BlockIterator start);

	// Raycasting
	GameRaycastResult3D RaycastVsBlocks(Vec3 const& rayStartPos, Vec3 const& rayDir, float maxDist) const;

	bool m_lightingEnabled = true;
private:
	Game* m_theGame = nullptr;
	std::unordered_map<IntVec2, Chunk*> m_activeChunks;
	std::vector<Chunk*> m_meshBuildQueue;
	std::deque<BlockIterator> m_dirtyLightBlocks;

	// Debugging
	bool m_debugDrawMode = false;
	bool m_debugJobText = false;
	bool m_debugRaycast = false;

	// Job queues
	std::deque<Chunk*> m_chunksQueuedForGeneration;
	std::deque<Chunk*> m_chunksQueuedForLoad;
	std::deque<Chunk*> m_chunksQueuedForSave;

	// Current outstanding jobs
	int m_outstandingGenerateJobs = 0;
	int m_outstandingLoadJobs     = 0;
	int m_outstandingSaveJobs     = 0;
};