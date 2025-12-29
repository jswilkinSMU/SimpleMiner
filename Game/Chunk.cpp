#include "Game/Chunk.hpp"
#include "Game/World.hpp"
#include "Game/Block.hpp"
#include "Game/BlockDefinition.hpp"
#include "Game/BlockIterator.hpp"
#include "Game/Game.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Core/EngineCommon.h"
#include "Engine/Math/SmoothNoise.hpp"
#include "Engine/Math/RawNoise.hpp"
#include "Engine/Math/MathUtils.h"
#include "Engine/Math/Splines.hpp"

Chunk::Chunk(IntVec2 const& chunkCoords)
	:m_chunkCoords(chunkCoords)
{
	// Getting spritesheet texture
	m_spriteImage = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/SpriteSheet_Classic_Faithful_32x.png", 5);
	m_spriteSheet = new SpriteSheet(*m_spriteImage, IntVec2::GRID8X8);

	// Populate blocks in chunk
	m_blocks = new Block[CHUNK_BLOCK_TOTAL];
}

Chunk::~Chunk()
{
	DeleteBuffers();

	delete[] m_blocks;
	m_blocks = nullptr;

	delete m_spriteSheet;
	m_spriteSheet = nullptr;
}

void Chunk::Update(float deltaseconds)
{
	UNUSED(deltaseconds);
}

void Chunk::Render() const
{
	if (m_vertexBuffer == nullptr || m_indexBuffer == nullptr)
	{
		return;
	}

	if (m_indices.empty())
	{
		return;
	}

	g_theRenderer->SetModelConstants();
	g_theGame->SetWorldConstants();
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_theRenderer->BindShader(g_theGame->m_worldShader);
	g_theRenderer->BindTexture(m_spriteImage);
	g_theRenderer->DrawIndexedVertexBuffer(m_vertexBuffer, m_indexBuffer, static_cast<unsigned int>(m_indices.size()));
}

void Chunk::PopulateWithDensityNoise()
{
	const float densityBiasPerBlock = 2.f / static_cast<float>(CHUNK_SIZE_Z);
	const int numXY = CHUNK_SIZE_X * CHUNK_SIZE_Y;

	// Noise maps for this chunk
	std::vector<float> continentNoiseMap(numXY);
	std::vector<float> continentalnessNoiseMap(numXY);
	std::vector<float> erosionMap(numXY);
	std::vector<float> peaksValleysMap(numXY);
	std::vector<float> temperatureMap(numXY);
	std::vector<float> humidityMap(numXY);
	std::vector<float> treeNoiseMap(numXY);
	std::vector<float> treeVariantNoiseMap(numXY);

	// Precomputation caching
	for (int chunkY = 0; chunkY < CHUNK_SIZE_Y; ++chunkY)
	{
		for (int chunkX = 0; chunkX < CHUNK_SIZE_X; ++chunkX)
		{
			int chunkIndex = chunkY * CHUNK_SIZE_X + chunkX;
			int globalX = m_chunkCoords.x * CHUNK_SIZE_X + chunkX;
			int globalY = m_chunkCoords.y * CHUNK_SIZE_Y + chunkY;


			continentNoiseMap[chunkIndex] = Compute2dPerlinNoise((float)globalX, (float)globalY, CONTINENT_SCALE, CONTINENT_OCTAVES,
				DEFAULT_OCTAVE_PERSISTANCE, DEFAULT_NOISE_OCTAVE_SCALE, true, GAME_SEED + 100);
			continentalnessNoiseMap[chunkIndex] = Compute2dPerlinNoise((float)globalX, (float)globalY, CONTINENTALNESS_SCALE, BIOME_OCTAVES,
				DEFAULT_OCTAVE_PERSISTANCE, DEFAULT_NOISE_OCTAVE_SCALE, true, GAME_SEED + 100);
			erosionMap[chunkIndex] = Compute2dPerlinNoise((float)globalX, (float)globalY, EROSIION_SCALE, BIOME_OCTAVES,
				DEFAULT_OCTAVE_PERSISTANCE, DEFAULT_NOISE_OCTAVE_SCALE, true, GAME_SEED + 200);
			peaksValleysMap[chunkIndex] = Compute2dPerlinNoise((float)globalX, (float)globalY, PEAKVALLEY_SCALE, BIOME_OCTAVES,
				DEFAULT_OCTAVE_PERSISTANCE, DEFAULT_NOISE_OCTAVE_SCALE, true, GAME_SEED + 300);
			temperatureMap[chunkIndex] = Compute2dPerlinNoise((float)globalX, (float)globalY, TEMPERATURE_SCALE, BIOME_OCTAVES,
				DEFAULT_OCTAVE_PERSISTANCE, DEFAULT_NOISE_OCTAVE_SCALE, true, GAME_SEED + 400);
			humidityMap[chunkIndex] = Compute2dPerlinNoise((float)globalX, (float)globalY, HUMIDITY_SCALE, BIOME_OCTAVES,
				DEFAULT_OCTAVE_PERSISTANCE, DEFAULT_NOISE_OCTAVE_SCALE, true, GAME_SEED + 500);

			treeNoiseMap[chunkIndex] = Get2dNoiseZeroToOne(globalX, globalY, GAME_SEED + 42);
			treeVariantNoiseMap[chunkIndex] = Get2dNoiseZeroToOne(globalX, globalY, GAME_SEED + 1337);
		}
	}

	// Using cached maps during block population
	for (int chunkY = 0; chunkY < CHUNK_SIZE_Y; ++chunkY)
	{
		for (int chunkX = 0; chunkX < CHUNK_SIZE_X; ++chunkX)
		{
			int chunkIndex = chunkY * CHUNK_SIZE_X + chunkX;
			int globalX = m_chunkCoords.x * CHUNK_SIZE_X + chunkX;
			int globalY = m_chunkCoords.y * CHUNK_SIZE_Y + chunkY;

			// Grabbing noise maps
			float continentNoise = continentNoiseMap[chunkIndex];
			float continentalness = continentalnessNoiseMap[chunkIndex];
			float erosion = erosionMap[chunkIndex];
			float peaksValleys = peaksValleysMap[chunkIndex];
			float temperature = temperatureMap[chunkIndex];
			float humidity = humidityMap[chunkIndex];

			// Computing biomes once per column
			BiomeParams biomeParams = { continentalness, erosion, peaksValleys, temperature, humidity };
			BiomeType biome = GetBiomeType(biomeParams);
			SurfaceBlocks surface = GetSurfaceBlocks(biome);

			// Continent shaping
			float normalizedNoise = (continentNoise + 1.0f) * 0.5f;
			float heightOffset = g_theGame->m_continentHeightOffsetCurve->EvaluateAtParametric(normalizedNoise).y;
			float squashingFactor = g_theGame->m_continentSquashCurve->EvaluateAtParametric(normalizedNoise).y;
			float baseHeight = DEFAULT_TERRAIN_HEIGHT + (heightOffset * (CHUNK_SIZE_Z / 8.5f));

			int surfaceDepthCounter = 0;
			int surfaceZ = -1;

			for (int chunkZ = CHUNK_SIZE_Z - 1; chunkZ >= 0; --chunkZ)
			{
				int blockIndex = GetBlockIndex(chunkX, chunkY, chunkZ);
				Block* block = &m_blocks[blockIndex];
				int globalZ = chunkZ;

				// Terrain density bias
				// -----------------------------------------------------------------------------
				float noiseValue = Compute3dPerlinNoise(static_cast<float>(globalX), static_cast<float>(globalY), static_cast<float>(globalZ),
					                                    DENSITY_NOISE_SCALE, DENSITY_OCTAVES, DEFAULT_OCTAVE_PERSISTANCE, DEFAULT_NOISE_OCTAVE_SCALE, true, GAME_SEED);

				float noiseBias = densityBiasPerBlock * (static_cast<float>(globalZ) - DEFAULT_TERRAIN_HEIGHT);
				float densityValue = noiseValue + noiseBias;
				// -----------------------------------------------------------------------------
				
				// Applying continental shaping
				// -----------------------------------------------------------------------------
				float offset = (static_cast<float>(globalZ) - baseHeight) / baseHeight;
				densityValue -= heightOffset;
				densityValue += (squashingFactor * SQUASH_MULT) * offset;
				// -----------------------------------------------------------------------------

				// Caves
				// -----------------------------------------------------------------------------
				bool isCave = false;
				bool chunkHasCaves = (Get2dNoiseZeroToOne(m_chunkCoords.x, m_chunkCoords.y, GAME_SEED + 9999) > 0.925f);
				bool belowSurface = (surfaceZ != -1 && chunkZ < surfaceZ);

				if (chunkHasCaves && belowSurface)
				{
					// Perlin Worm Tunnels
					{
						float wormNoise = Compute3dPerlinNoise(globalX * 0.05f, globalY * 0.05f, globalZ * 0.05f, 1.0f, 3, 0.5f, 2.0f, true, GAME_SEED + 777);
						if (fabsf(wormNoise) < 0.1f) 
						{
							densityValue = 1.0f;
							isCave = true;
						}
					}

					// Cheese Caves
					{
						float caveMask = Compute3dPerlinNoise(globalX * 0.03f, globalY * 0.03f, globalZ * 0.03f, 1.0f, 2, 0.5f, 2.0f, true, GAME_SEED + 900);
						if (caveMask > 0.55f)
						{
							densityValue = 1.0f;
							isCave = true;
						}
					}

					// Spherical Cavern
					{
						float roomChance = Get3dNoiseZeroToOne(globalX, globalY, globalZ, GAME_SEED + 1234);
						if (roomChance > 0.995f) 
						{
							Vec3 center = Vec3(floorf(globalX / 12.f) * 12, floorf(globalY / 12.f) * 12, floorf(globalZ / 12.f) * 12);
							Vec3 position = Vec3(static_cast<float>(globalX), static_cast<float>(globalY), static_cast<float>(globalZ));
							float dist = (position - center).GetLength();
							if (dist < 8.0f) 
							{
								densityValue = 1.0f;
								isCave = true;
							}
						}
					}
				}

				// -----------------------------------------------------------------------------

				// Block type assignemnt
				// -----------------------------------------------------------------------------
				if (densityValue < 0.f)
				{
					if (surfaceDepthCounter < SURFACE_LAYER_DEPTH)
					{
						if (globalZ >= SEA_LEVEL)
						{
							if (surfaceDepthCounter == 0)
							{
								block->SetBlockType(surface.top);
							}
							else
							{
								block->SetBlockType(surface.sub);
							}
						}
						else
						{
							block->SetBlockType(surface.underwater);
						}
						surfaceDepthCounter += 1;
					}
					else
					{
						if (globalZ == OBSIDIAN_Z)
						{
							block->SetBlockType(BLOCKTYPE_OBSIDIAN);
						}
						else if (globalZ == LAVA_Z)
						{
							block->SetBlockType(BLOCKTYPE_LAVA);
						}
						else
						{
							OreChance(globalX, globalY, globalZ, block);
						}
					}
					if (surfaceZ == -1)
					{
						surfaceZ = chunkZ;
					}
				}
				else
				{
					surfaceDepthCounter = 0;
					if (globalZ < SEA_LEVEL)
					{
						if (!isCave)
						{
							block->SetBlockType(BLOCKTYPE_WATER);
						}
						else if (block->m_blockType != BLOCKTYPE_WATER)
						{
							block->SetBlockType(BLOCKTYPE_AIR);
						}
					}
					else
					{
						block->SetBlockType(BLOCKTYPE_AIR);
					}
				}
				// -----------------------------------------------------------------------------

				// Trees
				// -----------------------------------------------------------------------------
				if (surfaceZ >= 0 && surfaceZ == chunkZ)
				{
					float treeNoise = treeNoiseMap[chunkIndex];
					if (treeNoise > 0.975f)
					{
						Block* surfaceBlock = &m_blocks[GetBlockIndex(chunkX, chunkY, surfaceZ)];
						uint8_t surfaceType = surfaceBlock->m_blockType;

						if (surfaceBlock->m_blockType != BLOCKTYPE_WATER)
						{
							TreeStamp const* stamp = nullptr;
							float treeVariantNoise = treeVariantNoiseMap[chunkIndex];

							if (biome == BIOME_FOREST)
							{
								if (surfaceType == BLOCKTYPE_GRASS || surfaceType == BLOCKTYPE_DIRT)
								{

									if (treeVariantNoise < 0.33f)
									{
										stamp = &g_treeStamps["oak_small"];
									}
									else if (treeVariantNoise < 0.66f)
									{
										stamp = &g_treeStamps["oak_large"];
									}
									else
									{
										stamp = &g_treeStamps["birch"];
									}
								}
							}
							else if (biome == BIOME_TAIGA)
							{
								if (surfaceType == BLOCKTYPE_GRASSLIGHT)
								{
									stamp = &g_treeStamps["spruce"];
								}
							}
							else if (biome == BIOME_SNOWY_TAIGA)
							{
								if (surfaceType == BLOCKTYPE_SNOW)
								{
									stamp = &g_treeStamps["snowy_spruce"];
								}
							}
							else if (biome == BIOME_DESERT)
							{
								if (surfaceType == BLOCKTYPE_SAND)
								{
									stamp = &g_treeStamps["cactus"];
								}
							}
							else if (biome == BIOME_SAVANNA)
							{
								if (surfaceType == BLOCKTYPE_GRASSYELLOW)
								{
									stamp = &g_treeStamps["acacia"];
								}
							}
							else if (biome == BIOME_JUNGLE)
							{
								if (surfaceType == BLOCKTYPE_GRASSDARK)
								{
									stamp = &g_treeStamps["jungle"];
								}
							}

							if (stamp != nullptr)
							{
								int aboveSurfaceIndex = GetBlockIndex(chunkX, chunkY, surfaceZ + 1);
								Block* aboveSurfaceBlock = &m_blocks[aboveSurfaceIndex];

								if (aboveSurfaceBlock->m_blockType == BLOCKTYPE_WATER)
								{
									continue;
								}

								if (chunkX >= stamp->radius && chunkX < CHUNK_SIZE_X - stamp->radius &&
									chunkY >= stamp->radius && chunkY < CHUNK_SIZE_Y - stamp->radius)
								{
									TryToPlaceTreeStamp(*stamp, chunkX, chunkY, surfaceZ + 1);
								}
							}
						}
					}
				}
			}
		}
	}
}

void Chunk::OreChance(int globalX, int globalY, int globalZ, Block* block)
{
	// Diamond veins
	float diamondNoise = Compute3dPerlinNoise(globalX * 0.09f, globalY * 0.09f, globalZ * 0.09f,
		1.0f, 3, 0.5f, 2.0f, true,
		GAME_SEED + 20100);

	if (diamondNoise > 0.75f && globalZ < 20)
	{
		block->SetBlockType(BLOCKTYPE_DIAMOND);
		return;
	}

	// Gold veins
	float goldNoise = Compute3dPerlinNoise(globalX * 0.09f, globalY * 0.09f, globalZ * 0.09f,
		1.0f, 3, 0.5f, 2.0f, true,
		GAME_SEED + 20200);

	if (goldNoise > 0.65f && globalZ < 40)
	{
		block->SetBlockType(BLOCKTYPE_GOLD);
		return;
	}

	// Iron veins
	float ironNoise = Compute3dPerlinNoise(globalX * 0.08f, globalY * 0.08f, globalZ * 0.08f,
		1.0f, 4, 0.5f, 2.0f, true,
		GAME_SEED + 20300);

	if (ironNoise > 0.5f)
	{
		block->SetBlockType(BLOCKTYPE_IRON);
		return;
	}

	// Coal veins
	float coalNoise = Compute3dPerlinNoise(globalX * 0.075f, globalY * 0.075f, globalZ * 0.075f,
		1.0f, 3, 0.5f, 2.0f, true,
		GAME_SEED + 20400);

	if (coalNoise > 0.45f)
	{
		block->SetBlockType(BLOCKTYPE_COAL);
		return;
	}

	block->SetBlockType(BLOCKTYPE_STONE);
}

void Chunk::TryToPlaceTreeStamp(TreeStamp const& treeStamp, int localX, int localY, int localZ)
{
	for (auto const& [offset, blocktype] : treeStamp.blocks)
	{
		IntVec3 treePosition = IntVec3(localX + offset.x, localY + offset.y, localZ + offset.z);
		IntVec2 neighborChunkOffset = IntVec2::ZERO;
		IntVec3 blockPositionInTarget = treePosition;

		// X
		if (treePosition.x < 0)
		{
			neighborChunkOffset.x = -1;
			blockPositionInTarget.x += CHUNK_SIZE_X;
		}
		else if (treePosition.x >= CHUNK_SIZE_X)
		{
			neighborChunkOffset.x = 1;
			blockPositionInTarget.x -= CHUNK_SIZE_X;
		}

		// Y
		if (treePosition.y < 0)
		{
			neighborChunkOffset.y = -1;
			blockPositionInTarget.y += CHUNK_SIZE_Y;
		}
		else if (treePosition.y >= CHUNK_SIZE_Y)
		{
			neighborChunkOffset.y = 1;
			blockPositionInTarget.y -= CHUNK_SIZE_Y;
		}

		// Skipping blocks below or above world height
		if (blockPositionInTarget.z < 0 || blockPositionInTarget.z >= CHUNK_SIZE_Z)
		{
			continue;
		}

		// Finding the target chunk
		Chunk* targetChunk = g_theGame->m_currentWorld->GetWorldChunk(m_chunkCoords + neighborChunkOffset);
		if (!targetChunk)
		{
			continue;
		}

		int index = targetChunk->GetBlockIndex(blockPositionInTarget);
		Block& block = targetChunk->m_blocks[index];

		if (block.m_blockType == BLOCKTYPE_AIR)
		{
			block.SetBlockType(blocktype);
		}
	}
}

int Chunk::GetTemperatureBand(float v)
{
	if (v < -0.45f) return 0;
	if (v < -0.15f) return 1;
	if (v < 0.20f) return 2; 
	if (v < 0.55f) return 3;
	return 4;
}

int Chunk::GetHumidityBand(float v)
{
	if (v < -0.35f) return 0; 
	if (v < -0.10f) return 1; 
	if (v < 0.10f) return 2;
	if (v < 0.30f) return 3; 
	return 4;
}

int Chunk::GetContinentalnessBand(float v)
{
	if (v < -1.05f) return 0;      // Deep Ocean
	if (v < -0.455f) return 1;    // Deep Ocean
	if (v < -0.19f) return 2;    // Ocean
	if (v < -0.11f) return 3;   // Coast
	if (v < 0.03f) return 4;   // Near-Inland
	if (v < 0.30f) return 5;  // Mid-Inland
	return 6;                // Far-Inland
}

BiomeType Chunk::GetBiomeType(BiomeParams const& biome)
{
	int temp = GetTemperatureBand(biome.temperature);
	int humidity = GetHumidityBand(biome.humidity);
	int continent = GetContinentalnessBand(biome.continentalness);

	// Oceanic regions
	if (continent <= 2) 
	{
		if (temp == 0) return BIOME_FROZEN_OCEAN;
		if (continent <= 1) return BIOME_DEEP_OCEAN;
		return BIOME_OCEAN;
	}

	// Coasts / beaches
	if (continent == 3) 
	{
		if (temp == 0) return BIOME_SNOWY_BEACH;
		if (temp < 4) return BIOME_BEACH;
		return BIOME_DESERT;
	}

	// Inland regions
	// Temperature + Humidity table
	if (temp == 0) 
	{
		if (humidity <= 1)
		{
			return BIOME_SNOWY_PLAINS;
		}
		if (humidity == 3)
		{
			return BIOME_SNOWY_TAIGA;
		}
		else
		{
			return BIOME_TAIGA;
		}
	}
	else if (temp == 1) 
	{
		if (humidity <= 1)
		{
			return BIOME_PLAINS;
		}
		if (humidity >= 3)
		{
			return BIOME_FOREST;
		}
		else
		{
			return BIOME_TAIGA;
		}
	}
	else if (temp == 2) 
	{
		if (humidity <= 1)
		{
			return BIOME_PLAINS;
		}
		if (humidity >= 3)
		{
			return BIOME_FOREST;
		}
		else
		{
			return BIOME_FOREST;
		}
	}
	else if (temp == 3) 
	{
		if (humidity <= 1)
		{
			return BIOME_SAVANNA;
		}
		if (humidity >= 3)
		{
			return BIOME_JUNGLE;
		}
		else
		{
			return BIOME_FOREST;
		}
	}
	else 
	{
		if (humidity <= 2)
		{
			return BIOME_DESERT;
		}
		else
		{
			return BIOME_BADLANDS;
		}
	}
}


SurfaceBlocks Chunk::GetSurfaceBlocks(BiomeType biome)
{
	switch (biome)
	{
	// Oceans & Beaches
		case BIOME_OCEAN:
		case BIOME_DEEP_OCEAN:
			return { BLOCKTYPE_SAND, BLOCKTYPE_ICE, BLOCKTYPE_SAND };
		case BIOME_FROZEN_OCEAN:
			return { BLOCKTYPE_SNOW, BLOCKTYPE_ICE, BLOCKTYPE_SAND };
		case BIOME_BEACH:
			return { BLOCKTYPE_SAND, BLOCKTYPE_SAND, BLOCKTYPE_SAND };
		case BIOME_SNOWY_BEACH:
			return { BLOCKTYPE_SNOW, BLOCKTYPE_SNOW, BLOCKTYPE_SAND };

	// Hot desert areas
		case BIOME_DESERT:
			return { BLOCKTYPE_SAND, BLOCKTYPE_SAND, BLOCKTYPE_SAND };
		case BIOME_BADLANDS:
			return { BLOCKTYPE_GRASSLIGHT, BLOCKTYPE_DIRT, BLOCKTYPE_STONE };

	// Forests
		case BIOME_PLAINS:
			return { BLOCKTYPE_GRASS, BLOCKTYPE_DIRT, BLOCKTYPE_SAND };
		case BIOME_FOREST:
			return { BLOCKTYPE_GRASS, BLOCKTYPE_DIRT, BLOCKTYPE_STONE };
		case BIOME_TAIGA:
			return { BLOCKTYPE_GRASSLIGHT, BLOCKTYPE_DIRT, BLOCKTYPE_STONE };
		case BIOME_SNOWY_TAIGA:
			return { BLOCKTYPE_SNOW, BLOCKTYPE_ICE, BLOCKTYPE_STONE };
		case BIOME_SNOWY_PLAINS:
			return { BLOCKTYPE_SNOW, BLOCKTYPE_DIRT, BLOCKTYPE_SAND };

	// Jungle / Savanna
		case BIOME_JUNGLE:
			return { BLOCKTYPE_GRASSDARK, BLOCKTYPE_DIRT, BLOCKTYPE_SAND };
		case BIOME_SAVANNA:
			return { BLOCKTYPE_GRASSYELLOW, BLOCKTYPE_DIRT, BLOCKTYPE_STONE };

		default:
			return { BLOCKTYPE_STONE, BLOCKTYPE_STONE, BLOCKTYPE_STONE };
	}
}

void Chunk::PopulateChunksWithNoise()
{
	unsigned int terrainSeed = GAME_SEED;
	unsigned int humiditySeed = terrainSeed + 1;
	unsigned int temperatureSeed = humiditySeed + 1;
	unsigned int hillSeed = temperatureSeed + 1;
	unsigned int oceanSeed = hillSeed + 1;
	unsigned int dirtSeed = oceanSeed + 1;

	std::vector<int> heightMapXY(CHUNK_SIZE_X * CHUNK_SIZE_Y);
	std::vector<int> dirtDepthXY(CHUNK_SIZE_X * CHUNK_SIZE_Y);
	std::vector<float> humidityMapXY(CHUNK_SIZE_X * CHUNK_SIZE_Y);
	std::vector<float> tempMapXY(CHUNK_SIZE_X * CHUNK_SIZE_Y);

	// Generating the noise maps
	GenerateNoiseMaps(humiditySeed, humidityMapXY, temperatureSeed, tempMapXY, hillSeed, oceanSeed, terrainSeed, dirtSeed, dirtDepthXY, heightMapXY);

	// Fill the blocks based on noise maps
	PopulateTerrainBlocks(heightMapXY, dirtDepthXY, humidityMapXY, tempMapXY, terrainSeed);

	// Generate vegetation
	PopulateTrees(heightMapXY, humidityMapXY, tempMapXY);
}

void Chunk::GenerateNoiseMaps(unsigned int humiditySeed, std::vector<float>& humidityMapXY, unsigned int temperatureSeed, std::vector<float>& tempMapXY, unsigned int hillSeed, unsigned int oceanSeed, unsigned int terrainSeed, unsigned int dirtSeed, std::vector<int>& dirtDepthXY, std::vector<int>& heightMapXY)
{
	for (int chunkY = 0; chunkY < CHUNK_SIZE_Y; ++chunkY)
	{
		for (int chunkX = 0; chunkX < CHUNK_SIZE_X; ++chunkX)
		{
			int chunkGlobalX = m_chunkCoords.x * CHUNK_SIZE_X + chunkX;
			int chunkGlobalY = m_chunkCoords.y * CHUNK_SIZE_Y + chunkY;
			int blockIndexXY = chunkY * CHUNK_SIZE_X + chunkX;

			// Humidity
			float humidityNoise = Compute2dPerlinNoise(static_cast<float>(chunkGlobalX), static_cast<float>(chunkGlobalY), HUMIDITY_NOISE_SCALE, HUMIDITY_NOISE_OCTAVES,
				DEFAULT_OCTAVE_PERSISTANCE, DEFAULT_NOISE_OCTAVE_SCALE, true, humiditySeed);
			float humidity = 0.5f + 0.5f * humidityNoise;
			humidityMapXY[blockIndexXY] = humidity;

			// Temperature
			float temperatureRaw = Get2dNoiseNegOneToOne(chunkGlobalX, chunkGlobalY, temperatureSeed) * TEMPERATURE_RAW_NOISE_SCALE;
			float temperatureNoise = Compute2dPerlinNoise(static_cast<float>(chunkGlobalX), static_cast<float>(chunkGlobalY), TEMPERATURE_NOISE_SCALE, TEMPERATURE_NOISE_OCTAVES,
				DEFAULT_OCTAVE_PERSISTANCE, DEFAULT_NOISE_OCTAVE_SCALE, true, temperatureSeed);
			float temperature = temperatureRaw + 0.5f + 0.5f * temperatureNoise;
			tempMapXY[blockIndexXY] = temperature;

			// Hills
			float hillNoise = Compute2dPerlinNoise(static_cast<float>(chunkGlobalX), static_cast<float>(chunkGlobalY), HILLINESS_NOISE_SCALE, HILLINESS_NOISE_OCTAVES,
				DEFAULT_OCTAVE_PERSISTANCE, DEFAULT_NOISE_OCTAVE_SCALE, true, hillSeed);
			float hillNormalized = (hillNoise + 1.f) * 0.5f;
			float hill = SmoothStep3(hillNormalized);

			// Ocean
			float oceanNoise = Compute2dPerlinNoise(static_cast<float>(chunkGlobalX), static_cast<float>(chunkGlobalY), OCEANESS_NOISE_SCALE, OCEANESS_NOISE_OCTAVES,
				DEFAULT_OCTAVE_PERSISTANCE, DEFAULT_NOISE_OCTAVE_SCALE, true, oceanSeed);

			// Terrain
			float terrainNoise = Compute2dPerlinNoise(static_cast<float>(chunkGlobalX), static_cast<float>(chunkGlobalY), TERRAIN_NOISE_SCALE, TERRAIN_NOISE_OCTAVES,
				DEFAULT_OCTAVE_PERSISTANCE, DEFAULT_NOISE_OCTAVE_SCALE, true, terrainSeed);
			float riverValleyTerrain = fabsf(terrainNoise);
			float baseTerrainHeight = DEFAULT_TERRAIN_HEIGHT + hill * RangeMap(riverValleyTerrain, 0.f, 1.f, -RIVER_DEPTH, DEFAULT_TERRAIN_HEIGHT);

			// Ocean dips
			if (oceanNoise > OCEAN_START_THRESHOLD)
			{
				float oceanBlend = (oceanNoise - OCEAN_START_THRESHOLD) / (OCEAN_END_THRESHOLD - OCEAN_START_THRESHOLD);
				oceanBlend = GetClamped(oceanBlend, 0.f, 1.f);
				baseTerrainHeight -= Interpolate(0.f, OCEAN_DEPTH, oceanBlend);
			}

			int terrainHeight = static_cast<int>(floorf(baseTerrainHeight));

			// Dirt depth
			float dirtDepthNoise = Get2dNoiseZeroToOne(chunkGlobalX, chunkGlobalY, dirtSeed);
			int dirtSubZ = MIN_DIRT_OFFSET_Z + RoundDownToInt(dirtDepthNoise * (MAX_DIRT_OFFSET_Z - MIN_DIRT_OFFSET_Z));
			dirtDepthXY[blockIndexXY] = dirtSubZ;

			heightMapXY[blockIndexXY] = terrainHeight;
		}
	}
}

void Chunk::PopulateTerrainBlocks(std::vector<int> heightMapXY, std::vector<int> dirtDepthXY, std::vector<float> humidityMapXY, std::vector<float> tempMapXY, unsigned int terrainSeed)
{
	for (int chunkZ = 0; chunkZ < CHUNK_SIZE_Z; ++chunkZ)
	{
		for (int chunkY = 0; chunkY < CHUNK_SIZE_Y; ++chunkY)
		{
			for (int chunkX = 0; chunkX < CHUNK_SIZE_X; ++chunkX)
			{
				int blockIndex = GetBlockIndex(chunkX, chunkY, chunkZ);
				Block* block = &m_blocks[blockIndex];
				block->m_blockType = BLOCKTYPE_AIR;

				int chunkGlobalX = m_chunkCoords.x * CHUNK_SIZE_X + chunkX;
				int chunkGlobalY = m_chunkCoords.y * CHUNK_SIZE_Y + chunkY;
				int chunkGlobalZ = chunkZ;

				int blockIndexXY = chunkY * CHUNK_SIZE_X + chunkX;
				int terrainHeight = heightMapXY[blockIndexXY];
				int dirtDepth = dirtDepthXY[blockIndexXY];
				float humidity = humidityMapXY[blockIndexXY];
				float temperature = tempMapXY[blockIndexXY];

				// Ice
				float iceDepthFloat = DEFAULT_TERRAIN_HEIGHT - floorf(RangeMapClamped(temperature, ICE_TEMPERATURE_MAX, ICE_TEMPERATURE_MIN, ICE_DEPTH_MIN, ICE_DEPTH_MAX));
				int iceDepth = static_cast<int>(iceDepthFloat);

				// Water
				if (chunkGlobalZ > terrainHeight && chunkGlobalZ < SEA_LEVEL_Z)
				{
					if (temperature < 0.38f && chunkGlobalZ > iceDepth)
					{
						block->m_blockType = BLOCKTYPE_ICE;
					}
					else
					{
						block->m_blockType = BLOCKTYPE_WATER;
					}
				}

				// Surface
				if (chunkGlobalZ == terrainHeight)
				{
					BlockType blockType = BLOCKTYPE_GRASS;
					if (humidity < MIN_SAND_HUMIDITY)
					{
						blockType = BLOCKTYPE_SAND;
					}
					if (humidity < MAX_SAND_HUMIDITY && terrainHeight <= static_cast<int>(DEFAULT_TERRAIN_HEIGHT))
					{
						blockType = BLOCKTYPE_SAND;
					}
					block->m_blockType = blockType;
				}

				int dirtTopZ = terrainHeight - dirtDepth;
				int sandTopZ = terrainHeight - static_cast<int>(RoundDownToInt(RangeMapClamped(humidity, MIN_SAND_DEPTH_HUMIDITY, MAX_SAND_DEPTH_HUMIDITY, SAND_DEPTH_MIN, SAND_DEPTH_MAX)));

				if (chunkGlobalZ < terrainHeight && chunkGlobalZ >= dirtTopZ)
				{
					BlockType blockType = BLOCKTYPE_DIRT;
					if (chunkGlobalZ >= sandTopZ)
					{
						blockType = BLOCKTYPE_SAND;
					}
					block->m_blockType = blockType;
				}

				// Underground
				if (chunkGlobalZ < dirtTopZ)
				{
					if (chunkGlobalZ == OBSIDIAN_Z)
					{
						block->m_blockType = BLOCKTYPE_OBSIDIAN;
					}
					else if (chunkGlobalZ == LAVA_Z)
					{
						block->m_blockType = BLOCKTYPE_LAVA;
					}
					else
					{
						float oreNoise = Get3dNoiseZeroToOne(chunkGlobalX, chunkGlobalY, chunkZ, terrainSeed + 100);

						if (oreNoise < DIAMOND_CHANCE)
						{
							block->m_blockType = BLOCKTYPE_DIAMOND;
						}
						else if (oreNoise < GOLD_CHANCE)
						{
							block->m_blockType = BLOCKTYPE_GOLD;
						}
						else if (oreNoise < IRON_CHANCE)
						{
							block->m_blockType = BLOCKTYPE_IRON;
						}
						else if (oreNoise < COAL_CHANCE)
						{
							block->m_blockType = BLOCKTYPE_COAL;
						}
						else
						{
							block->m_blockType = BLOCKTYPE_STONE;
						}
					}
				}
			}
		}
	}
}

void Chunk::PopulateTrees(std::vector<int> heightMapXY, std::vector<float> humidityMapXY, std::vector<float> tempMapXY)
{
	for (int chunkY = 0; chunkY < CHUNK_SIZE_Y; ++chunkY)
	{
		for (int chunkX = 0; chunkX < CHUNK_SIZE_X; ++chunkX)
		{
			int chunkGlobalX = m_chunkCoords.x * CHUNK_SIZE_X + chunkX;
			int chunkGlobalY = m_chunkCoords.y * CHUNK_SIZE_Y + chunkY;
			int blockIndexXY = chunkY * CHUNK_SIZE_X + chunkX;

			int terrainHeight = heightMapXY[blockIndexXY];
			float humidity = humidityMapXY[blockIndexXY];
			float temperature = tempMapXY[blockIndexXY];

			// Check to not spawn trees underwater
			if (terrainHeight < SEA_LEVEL_Z)
			{
				continue;
			}

			int blockIndex = GetBlockIndex(chunkX, chunkY, terrainHeight);
			uint8_t surfaceBlockType = m_blocks[blockIndex].m_blockType;

			// Check so that our trees spawn on grass
			if (surfaceBlockType != BLOCKTYPE_GRASS)
			{
				continue;
			}

			float treeNoise = Get2dNoiseZeroToOne(chunkGlobalX, chunkGlobalY, GAME_SEED);
			if (treeNoise > 0.0005f)
			{
				continue;
			}

			// Choosing a tree type by climate
			BlockType blockLog = BLOCKTYPE_OAKLOG;
			BlockType blockLeaves = BLOCKTYPE_OAKLEAVES;

			if (temperature < 0.4f)
			{
				blockLog = BLOCKTYPE_SPRUCELOG;
				blockLeaves = BLOCKTYPE_SPRUCELEAVES;
			}
			else if (humidity > 0.7f)
			{
				blockLog = BLOCKTYPE_JUNGLELOG;
				blockLeaves = BLOCKTYPE_JUNGLELEAVES;
			}
			else if (humidity < 0.3f)
			{
				blockLog = BLOCKTYPE_ACACIALOG;
				blockLeaves = BLOCKTYPE_ACACIALEAVES;
			}
			else if (temperature > 0.8f)
			{
				blockLog = BLOCKTYPE_BIRCHLOG;
				blockLeaves = BLOCKTYPE_BIRCHLEAVES;
			}

			// Setting a random trunk height
			int trunkHeight = 6 + static_cast<int>((Get2dNoiseZeroToOne(chunkGlobalX, chunkGlobalY, GAME_SEED) * 4));

			// Trunk
			for (int chunkZ = 1; chunkZ <= trunkHeight; ++chunkZ)
			{
				int trunkZUp = terrainHeight + chunkZ;

				if (trunkZUp >= CHUNK_SIZE_Z)
				{
					break;
				}

				int treeBlockIndex = GetBlockIndex(chunkX, chunkY, trunkZUp);
				m_blocks[treeBlockIndex].m_blockType = blockLog;
			}

			// Leaves
			for (int treeZ = -2; treeZ <= 2; ++treeZ)
			{
				int treeZUp = terrainHeight + trunkHeight + treeZ;

				if (treeZUp < 0 || treeZUp >= CHUNK_SIZE_Z)
				{
					continue;
				}

				for (int treeY = -2; treeY <= 2; ++treeY)
				{
					int treeYAcross = chunkY + treeY;

					if (treeYAcross < 0 || treeYAcross >= CHUNK_SIZE_Y)
					{
						continue;
					}

					for (int treeX = -2; treeX <= 2; ++treeX)
					{
						int treeXAcross = chunkX + treeX;

						if (treeXAcross < 0 || treeXAcross >= CHUNK_SIZE_X)
						{
							continue;
						}

						float treeSpread = sqrtf(static_cast<float>((treeX * treeX) + (treeY * treeY) + (treeZ * treeZ)));
						if (treeSpread > 2.5f)
						{
							continue;
						}

						int treeLeafIndex = GetBlockIndex(treeXAcross, treeYAcross, treeZUp);
						if (m_blocks[treeLeafIndex].m_blockType == BLOCKTYPE_AIR)
						{
							m_blocks[treeLeafIndex].m_blockType = blockLeaves;
						}
					}
				}
			}
		}
	}
}

void Chunk::GenerateChunkMesh()
{
	m_vertexes.clear();
	m_indices.clear();

	for (int chunkZ = 0; chunkZ < CHUNK_SIZE_Z; ++chunkZ)
	{
		for (int chunkY = 0; chunkY < CHUNK_SIZE_Y; ++chunkY)
		{
			for (int chunkX = 0; chunkX < CHUNK_SIZE_X; ++chunkX)
			{
				int blockIndex = GetBlockIndex(chunkX, chunkY, chunkZ);
				BlockIterator blockIterator(this, blockIndex);
				Block* block = &m_blocks[blockIterator.m_blockIndex];
				BlockDefinition* blockDef = BlockDefinition::s_blockDefs[block->m_blockType];

				if (!blockDef || !blockDef->m_isVisible)
				{
					continue;
				}

				float blockPosX = static_cast<float>(m_chunkCoords.x * CHUNK_SIZE_X + chunkX);
				float blockPosY = static_cast<float>(m_chunkCoords.y * CHUNK_SIZE_Y + chunkY);
				float blockPosZ = static_cast<float>(chunkZ);
				Vec3  blockPos = Vec3(blockPosX, blockPosY, blockPosZ);

				for (int blockFace = 0; blockFace < NUM_BLOCKFACES; ++blockFace)
				{
					BlockIterator neighbor = blockIterator.GetNeighbor(static_cast<BlockFace>(blockFace));
					bool drawFace = true;

					if (neighbor.IsValid()) 
					{
						Block* neighborBlock = &neighbor.m_chunk->m_blocks[neighbor.m_blockIndex];
						BlockDefinition* neighborDef = BlockDefinition::s_blockDefs[neighborBlock->m_blockType];
						if (neighborDef && neighborDef->m_isOpaque) 
						{
							drawFace = false;
						}
					}

					if (!drawFace)
					{
						continue;
					}

					IntVec2 spriteCoords = IntVec2::ZERO;
					Rgba8	colorTint = Rgba8::WHITE;

					switch (blockFace)
					{
						case BLOCK_FACE_TOP:
						{
							spriteCoords = blockDef->m_topSpriteCoords;
							colorTint = Rgba8::WHITE;
							break;
						}
						case BLOCK_FACE_BOTTOM:
						{
							spriteCoords = blockDef->m_bottomSpriteCoords;
							colorTint = Rgba8::WHITE;
							break;
						}
						case BLOCK_FACE_EAST:
						{
							spriteCoords = blockDef->m_sideSpriteCoords;
							colorTint = Rgba8(230, 230, 230);
							break;
						}
						case BLOCK_FACE_WEST:
						{
							spriteCoords = blockDef->m_sideSpriteCoords;
							colorTint = Rgba8(230, 230, 230);
							break;
						}
						case BLOCK_FACE_NORTH:
						{
							spriteCoords = blockDef->m_sideSpriteCoords;
							colorTint = Rgba8(200, 200, 200);
							break;
						}
						case BLOCK_FACE_SOUTH:
						{
							spriteCoords = blockDef->m_sideSpriteCoords;
							colorTint = Rgba8(200, 200, 200);
							break;
						}
					}
					BlockIterator neighborBlockIterator = blockIterator.GetNeighbor(static_cast<BlockFace>(blockFace));
					Block* neighborBlock = neighborBlockIterator.GetBlock();
					uint8_t outdoorLight = 0;
					uint8_t indoorLight = 0;

					if (neighborBlock)
					{
						outdoorLight = neighborBlock->GetOutdoorLight();
						indoorLight = neighborBlock->GetIndoorLight();
					}

					uint8_t redOutdoorChannel = (outdoorLight * 255) / 15;
					uint8_t greenIndoorChannel = (indoorLight * 255) / 15;
					Rgba8 vertexColor(redOutdoorChannel, greenIndoorChannel, colorTint.b, 255);
					if (!g_theGame->m_currentWorld->m_lightingEnabled)
					{
						vertexColor = colorTint;
					}
					AABB2 uv = m_spriteSheet->GetSpriteUVCoords(spriteCoords);
					AddVertsForBlockFace(blockPos, blockFace, vertexColor, uv);
				}
			}
		}
	}
	CreateBuffers();
}

void Chunk::CreateBuffers()
{
	if (m_vertexes.empty())
	{
		return;
	}

	if (m_indices.empty())
	{
		return;
	}

	DeleteBuffers();

	m_vertexBuffer = g_theRenderer->CreateVertexBuffer(static_cast<unsigned int>(m_vertexes.size()) * sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN));
	m_indexBuffer = g_theRenderer->CreateIndexBuffer(static_cast<unsigned int>(m_indices.size()) * sizeof(unsigned int), sizeof(unsigned int));
	g_theRenderer->CopyCPUToGPU(m_vertexes.data(), m_vertexBuffer->GetSize(), m_vertexBuffer);
	g_theRenderer->CopyCPUToGPU(m_indices.data(), m_indexBuffer->GetSize(), m_indexBuffer);
}

void Chunk::DeleteBuffers()
{
	delete m_vertexBuffer;
	m_vertexBuffer = nullptr;

	delete m_indexBuffer;
	m_indexBuffer = nullptr;
}

int Chunk::GetBlockIndex(int x, int y, int z) const
{
	int blockX = x;
	int blockY = y << CHUNK_BITS_X;
	int blockZ = z << (CHUNK_BITS_X + CHUNK_BITS_Y);
	return blockX + blockY + blockZ;
}

int Chunk::GetBlockIndex(IntVec3 const& coords) const
{
	return GetBlockIndex(coords.x, coords.y, coords.z);
}

int Chunk::IndexToLocalX(int blockIndex) const
{
	return blockIndex & CHUNK_MASK_X;
}

int Chunk::IndexToLocalY(int blockIndex) const
{
	return (blockIndex >> CHUNK_BITS_X) & CHUNK_MASK_Y;
}

int Chunk::IndexToLocalZ(int blockIndex) const
{
	return blockIndex >> (CHUNK_BITS_X + CHUNK_BITS_Y);
}

IntVec3 Chunk::IndexToLocalCoords(int blockIndex) const
{
	int x = blockIndex & CHUNK_MASK_X;         
	int y = (blockIndex >> CHUNK_BITS_X) & CHUNK_MASK_Y;
	int z = blockIndex >> (CHUNK_BITS_X + CHUNK_BITS_Y);

	return IntVec3(x, y, z);
}

int Chunk::GlobalCoordsToIndex(IntVec3 const& globalCoords) const
{
	int localX = globalCoords.x & CHUNK_MASK_X;
	int localY = globalCoords.y & CHUNK_MASK_Y;
	int localZ = globalCoords.z & CHUNK_MASK_Z;
	return (localZ << (CHUNK_BITS_X + CHUNK_BITS_Y)) | (localY << CHUNK_BITS_X) | localX;
}

int Chunk::GlobalCoordsToIndex(int x, int y, int z) const
{
	return GlobalCoordsToIndex(IntVec3(x, y, z));
}

IntVec3 Chunk::IndexToGlobalCoords(int blockIndex) const
{
	int localX = blockIndex & CHUNK_MASK_X;
	int localY = (blockIndex >> CHUNK_BITS_X) & CHUNK_MASK_Y;
	int localZ = blockIndex >> (CHUNK_BITS_X + CHUNK_BITS_Y);

	int globalX = m_chunkCoords.x * CHUNK_SIZE_X + localX;
	int globalY = m_chunkCoords.y * CHUNK_SIZE_Y + localY;
	int globalZ = localZ;

	return IntVec3(globalX, globalY, globalZ);
}

IntVec3 Chunk::GlobalCoordsToLocalCoords(IntVec3 const& globalCoords) const
{
	int localX = globalCoords.x & CHUNK_MASK_X;
	int localY = globalCoords.y & CHUNK_MASK_Y;
	int localZ = globalCoords.z & CHUNK_MASK_Z;

	return IntVec3(localX, localY, localZ);
}

IntVec2 Chunk::GetChunkCoords(IntVec3 const& globalCoords) const
{
	int chunkX = globalCoords.x >> CHUNK_BITS_X;
	int chunkY = globalCoords.y >> CHUNK_BITS_Y;
	return IntVec2(chunkX, chunkY);
}

IntVec2 Chunk::GetChunkCoords(Vec3 const& position) const
{
	int chunkX = static_cast<int>(position.x) >> CHUNK_BITS_X;
	int chunkY = static_cast<int>(position.y) >> CHUNK_BITS_Y;
	return IntVec2(chunkX, chunkY);
}

IntVec2 Chunk::GetChunkCenter(IntVec2 const& chunkCoords) const
{
	int chunkCenterX = (chunkCoords.x << CHUNK_BITS_X) + (CHUNK_SIZE_X >> 1);
	int chunkCenterY = (chunkCoords.y << CHUNK_BITS_Y) + (CHUNK_SIZE_Y >> 1);
	return IntVec2(chunkCenterX, chunkCenterY);
}

IntVec3 Chunk::GetBlockCoordsFromWorldPos(Vec3 const& worldPos) const
{
	Vec3 chunkOrigin = Vec3(static_cast<float>(m_chunkCoords.x * CHUNK_SIZE_X), static_cast<float>(m_chunkCoords.y * CHUNK_SIZE_Y), 0.f);
	Vec3 localPos = worldPos - chunkOrigin;

	IntVec3 localBlockCoords = IntVec3(RoundDownToInt(localPos.x), RoundDownToInt(localPos.y), RoundDownToInt(localPos.z));
	return localBlockCoords;
}

Block* Chunk::GetBlockAtLocalCoords(int blockX, int blockY, int blockZ) const
{
	int blockIndex = GetBlockIndex(blockX, blockY, blockZ);

	if (blockIndex >= 0 && blockIndex < CHUNK_BLOCK_TOTAL)
	{
		return &m_blocks[blockIndex];
	}

	return nullptr;
}

AABB3 Chunk::GetWorldBounds() const
{
	Vec3 worldMins = Vec3(static_cast<float>(m_chunkCoords.x * CHUNK_SIZE_X), static_cast<float>(m_chunkCoords.y * CHUNK_SIZE_Y), 0.f);
	Vec3 worldMaxs = worldMins + Vec3(static_cast<float>(CHUNK_SIZE_X), static_cast<float>(CHUNK_SIZE_Y), static_cast<float>(CHUNK_SIZE_Z));
	return AABB3(worldMins, worldMaxs);
}

int Chunk::GetVertexCount() const
{
	return static_cast<int>(m_vertexes.size());
}

int Chunk::GetIndexCount() const
{
	return static_cast<int>(m_indices.size());
}

void Chunk::AddVertsForBlockFace(Vec3 const& blockPos, int blockFace, Rgba8 const& blockTint, AABB2 const& blockUVs)
{
	Vec3 mins = blockPos;
	Vec3 maxs = blockPos + Vec3::ONE;
	Vec3 bl, br, tr, tl;

	switch (blockFace)
	{
		case BLOCK_FACE_EAST:
		{
			bl = Vec3(maxs.x, mins.y, mins.z);
			br = Vec3(maxs.x, maxs.y, mins.z);
			tr = Vec3(maxs.x, maxs.y, maxs.z);
			tl = Vec3(maxs.x, mins.y, maxs.z);
			break;
		}
		case BLOCK_FACE_WEST:
		{
			bl = Vec3(mins.x, maxs.y, mins.z);
			br = Vec3(mins.x, mins.y, mins.z);
			tr = Vec3(mins.x, mins.y, maxs.z);
			tl = Vec3(mins.x, maxs.y, maxs.z);
			break;
		}
		case BLOCK_FACE_NORTH:
		{
			bl = Vec3(maxs.x, maxs.y, mins.z);
			br = Vec3(mins.x, maxs.y, mins.z);
			tr = Vec3(mins.x, maxs.y, maxs.z);
			tl = Vec3(maxs.x, maxs.y, maxs.z);
			break;
		}
		case BLOCK_FACE_SOUTH:
		{
			bl = Vec3(mins.x, mins.y, mins.z);
			br = Vec3(maxs.x, mins.y, mins.z);
			tr = Vec3(maxs.x, mins.y, maxs.z);
			tl = Vec3(mins.x, mins.y, maxs.z);
			break;
		}
		case BLOCK_FACE_TOP:
		{
			bl = Vec3(mins.x, mins.y, maxs.z);
			br = Vec3(maxs.x, mins.y, maxs.z);
			tr = Vec3(maxs.x, maxs.y, maxs.z);
			tl = Vec3(mins.x, maxs.y, maxs.z);
			break;
		}
		case BLOCK_FACE_BOTTOM:
		{
			bl = Vec3(mins.x, maxs.y, mins.z);
			br = Vec3(maxs.x, maxs.y, mins.z);
			tr = Vec3(maxs.x, mins.y, mins.z);
			tl = Vec3(mins.x, mins.y, mins.z);
			break;
		}
	}

	AddVertsForQuad3D(m_vertexes, m_indices, bl, br, tr, tl, blockTint, blockUVs);
}

void Chunk::SetBlockType(int x, int y, int z, uint8_t newBlockType)
{
	if (x < 0 || x >= CHUNK_SIZE_X || y < 0 || y >= CHUNK_SIZE_Y || z < 0 || z >= CHUNK_SIZE_Z)
	{
		return;
	}

	int blockIndex = GetBlockIndex(x, y, z);
	Block* block = &m_blocks[blockIndex];
	uint8_t oldType = block->m_blockType;

	if (oldType == newBlockType)
	{
		return;
	}

	BlockDefinition* newDef = BlockDefinition::s_blockDefs[newBlockType];
	BlockIterator currentBlockIterator(this, blockIndex);
	BlockIterator aboveBlockIterator = currentBlockIterator.GetUpNeighbor();

	block->SetBlockType(newBlockType);
	m_isMeshDirty = true;
	m_needsSaving = true;

	g_theGame->m_currentWorld->MarkLightingDirty(currentBlockIterator);

	if (newDef->m_isOpaque == false)
	{
		Block* above = aboveBlockIterator.GetBlock();
		if (above && above->IsSky())
		{
			block->SetIsSky(true);
			g_theGame->m_currentWorld->MarkLightingDirty(currentBlockIterator);
			g_theGame->m_currentWorld->PropagateSkyDown(currentBlockIterator.GetDownNeighbor());
		}
	}
	else
	{
		if (block->IsSky())
		{
			block->SetIsSky(false);
		}

		g_theGame->m_currentWorld->MarkLightingDirty(currentBlockIterator);
		g_theGame->m_currentWorld->ClearSkyDown(currentBlockIterator.GetDownNeighbor());
	}
}
