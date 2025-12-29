#pragma once
#include "Game/GameCommon.h"
#include "Engine/Math/IntVec2.h"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Math/Vec3.h"
#include "Engine/Math/AABB2.h"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Core/Vertex_PCUTBN.hpp"
#include <vector>
#include <atomic>
// -----------------------------------------------------------------------------
class Block;
class VertexBuffer;
class IndexBuffer;
class Texture;
class SpriteSheet;
// -----------------------------------------------------------------------------
struct BiomeParams
{
	float continentalness;
	float erosion;
	float peaksValleys;
	float temperature;
	float humidity;
};
// -----------------------------------------------------------------------------
enum BiomeType : uint8_t
{
	BIOME_OCEAN,
	BIOME_DEEP_OCEAN,
	BIOME_FROZEN_OCEAN,
	BIOME_COAST,
	BIOME_BEACH,
	BIOME_SNOWY_BEACH,
	BIOME_DESERT,
	BIOME_PLAINS,
	BIOME_SAVANNA,
	BIOME_FOREST,
	BIOME_TAIGA,
	BIOME_SNOWY_TAIGA,
	BIOME_JUNGLE,
	BIOME_SNOWY_PLAINS,
	BIOME_BADLANDS,
	BIOME_STONY_PEAKS,
	BIOME_SNOWY_PEAKS,
	NUM_BIOME_TYPES
};
// -----------------------------------------------------------------------------
struct SurfaceBlocks 
{
	BlockType top;
	BlockType sub;
	BlockType underwater;
};
// -----------------------------------------------------------------------------
struct ChunkFileHeader
{
	char m_g = 'G';
	char m_c = 'C';
	char m_h = 'H';
	char m_k = 'K';
	uint8_t m_version = 1;
	uint8_t m_bitsX = 4;
	uint8_t m_bitsY = 4;
	uint8_t m_bitsZ = 7;
};
// -----------------------------------------------------------------------------
struct Run
{
	uint8_t m_blockType = 0;
	uint8_t m_runLength = 0;
};
// -----------------------------------------------------------------------------
class Chunk
{
public:

	// Add constructor after creating World data structure
	Chunk(IntVec2 const& chunkCoords);
	~Chunk();

	void Update(float deltaseconds);
	void Render() const;

	// Terrain Gen w/Density (NEW)
	void PopulateWithDensityNoise();

	// Structure/cool stuff
	void OreChance(int globalX, int globalY, int globalZ, Block* block);
	void TryToPlaceTreeStamp(TreeStamp const& treeStamp, int localX, int localY, int localZ);

	// Biomes
	int  GetTemperatureBand(float v);
	int  GetHumidityBand(float v);
	int  GetContinentalnessBand(float v);
	BiomeType GetBiomeType(BiomeParams const& biome);
	SurfaceBlocks GetSurfaceBlocks(BiomeType biome);

	// Terrain Generation A02 (OLD)
	void PopulateChunksWithNoise();
	void GenerateNoiseMaps(unsigned int humiditySeed, std::vector<float>& humidityMapXY, unsigned int temperatureSeed, std::vector<float>& tempMapXY, 
		                   unsigned int hillSeed, unsigned int oceanSeed, unsigned int terrainSeed, unsigned int dirtSeed, std::vector<int>& dirtDepthXY, std::vector<int>& heightMapXY);
	void PopulateTerrainBlocks(std::vector<int> heightMapXY, std::vector<int> dirtDepthXY, std::vector<float> humidityMapXY, std::vector<float> tempMapXY, unsigned int terrainSeed);
	void PopulateTrees(std::vector<int> heightMapXY, std::vector<float> humidityMapXY, std::vector<float> tempMapXY);
	
	void GenerateChunkMesh();

	void CreateBuffers();
	void DeleteBuffers();

	// Coord/Index utils
	int		GetBlockIndex(int x, int y, int z) const;
	int		GetBlockIndex(IntVec3 const& coords) const;
	int		IndexToLocalX(int blockIndex) const;
	int		IndexToLocalY(int blockIndex) const;
	int		IndexToLocalZ(int blockIndex) const;
	IntVec3 IndexToLocalCoords(int blockIndex) const;
	int		GlobalCoordsToIndex(IntVec3 const& globalCoords) const;
	int		GlobalCoordsToIndex(int x, int y, int z) const;
	IntVec3 IndexToGlobalCoords(int blockIndex) const;
	IntVec3 GlobalCoordsToLocalCoords(IntVec3 const& globalCoords) const;
	IntVec2 GetChunkCoords(IntVec3 const& globalCoords) const;
	IntVec2 GetChunkCoords(Vec3 const& position) const;
	IntVec2 GetChunkCenter(IntVec2 const& chunkCoords) const;
	IntVec3 GetBlockCoordsFromWorldPos(Vec3 const& worldPos) const;
	Block*  GetBlockAtLocalCoords(int blockX, int blockY, int blockZ) const;

	// Helpers
	AABB3	GetWorldBounds() const;
	int		GetVertexCount() const;
	int		GetIndexCount()  const;
	void	AddVertsForBlockFace(Vec3 const& blockPos, int blockFace, Rgba8 const& blockTint, AABB2 const& blockUVs);
	void	SetBlockType(int x, int y, int z, uint8_t newBlockType);

public:
	bool m_isMeshDirty = false;
	bool m_needsSaving = false;
	IntVec2 m_chunkCoords = IntVec2::ZERO;

	// Neighbor pointers
	Chunk* m_northNeighbor = nullptr;
	Chunk* m_southNeighbor = nullptr;
	Chunk* m_eastNeighbor = nullptr;
	Chunk* m_westNeighbor = nullptr;

	// Single 1D array of blocks
	Block* m_blocks = nullptr;

	// Atomic chunk state type
	std::atomic<ChunkState> m_chunkState = ChunkState::CONSTRUCTING;
private:
	// Each chunk has its own chunk coordinates and bounds
	AABB3   m_chunkWorldBounds = AABB3(Vec3::ZERO, Vec3::ZERO);

	// Each chunk owns a vertexbuffer and vertex array
	VertexBuffer* m_vertexBuffer = nullptr;
	IndexBuffer* m_indexBuffer = nullptr;
	std::vector<Vertex_PCUTBN> m_vertexes;
	std::vector<unsigned int> m_indices;

	// All chunk blocks are textured with the same single spritesheet
	Texture* m_spriteImage = nullptr;
	SpriteSheet* m_spriteSheet = nullptr;
};