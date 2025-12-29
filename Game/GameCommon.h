#pragma once
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Math/Vec3.h"
#include "Engine/Math/RandomNumberGenerator.h"
#include "Engine/Math/Vec4.hpp"
#include "Engine/Math/Vec2.hpp"
#include <vector>
#include <unordered_map>
#include <string>
// -----------------------------------------------------------------------------
class App;
class Game;
class Renderer;
class InputSystem;
class AudioSystem;
class JobSystem;
class Window;
struct Vec2;
struct Rgba8;
// -----------------------------------------------------------------------------
constexpr float SCREEN_SIZE_X = 1600.f;
constexpr float SCREEN_SIZE_Y = 800.f;
constexpr float SCREEN_CENTER_X = SCREEN_SIZE_X / 2.f;
constexpr float SCREEN_CENTER_Y = SCREEN_SIZE_Y / 2.f;
// -----------------------------------------------------------------------------
enum BlockType : uint8_t 
{
	BLOCKTYPE_AIR,
	BLOCKTYPE_WATER,
	BLOCKTYPE_SAND,
	BLOCKTYPE_SNOW,
	BLOCKTYPE_ICE,
	BLOCKTYPE_DIRT,
	BLOCKTYPE_STONE,
	BLOCKTYPE_COAL,
	BLOCKTYPE_IRON,
	BLOCKTYPE_GOLD,
	BLOCKTYPE_DIAMOND,
	BLOCKTYPE_OBSIDIAN,
	BLOCKTYPE_LAVA,
	BLOCKTYPE_GLOWSTONE,
	BLOCKTYPE_COBBLESTONE,
	BLOCKTYPE_CHISELEDBRICK,
	BLOCKTYPE_GRASS,
	BLOCKTYPE_GRASSLIGHT,
	BLOCKTYPE_GRASSDARK,
	BLOCKTYPE_GRASSYELLOW,
	BLOCKTYPE_ACACIALOG,
	BLOCKTYPE_ACACIAPLANKS,
	BLOCKTYPE_ACACIALEAVES,
	BLOCKTYPE_CACTUS,
	BLOCKTYPE_OAKLOG,
	BLOCKTYPE_OAKPLANKS,
	BLOCKTYPE_OAKLEAVES,
	BLOCKTYPE_BIRCHLOG,
	BLOCKTYPE_BIRCHPLANKS,
	BLOCKTYPE_BIRCHLEAVES,
	BLOCKTYPE_JUNGLELOG,
	BLOCKTYPE_JUNGLEPLANKS,
	BLOCKTYPE_JUNGLELEAVES,
	BLOCKTYPE_SPRUCELOG,
	BLOCKTYPE_SPRUCEPLANKS,
	BLOCKTYPE_SPRUCELEAVES,
	BLOCKTYPE_SPRUCELEAVESSNOW,
	NUM_BLOCK_TYPES
};
// -----------------------------------------------------------------------------
enum BlockFace
{
	BLOCK_FACE_EAST,
	BLOCK_FACE_WEST,
	BLOCK_FACE_NORTH,
	BLOCK_FACE_SOUTH,
	BLOCK_FACE_TOP,
	BLOCK_FACE_BOTTOM,
	NUM_BLOCKFACES
};
static const IntVec3 BLOCKFACE[6] =
{
	IntVec3::EAST,
	IntVec3::WEST,
	IntVec3::NORTH,
	IntVec3::SOUTH,
	IntVec3::TOP,
	IntVec3::BOTTOM
};
// -----------------------------------------------------------------------------
struct TreeStamp
{
	std::vector <std::pair<IntVec3, BlockType>> blocks;
	int radius = 0;
};
extern std::unordered_map<std::string, TreeStamp> g_treeStamps;
// -----------------------------------------------------------------------------
enum class ChunkState
{
	MISSING = -1,                 // Optional; Used only in cases where we want to say a chunk doesn't exist at all
	ON_DISK,                      // Optional; Used only in cases where we want to say a chunk is missing, but it exists on disk
	CONSTRUCTING,                 // [set by main thread] Initial Chunk::m_state value during early construction

	ACTIVATING_QUEUED_LOAD,       // [set by main thread] Chunk has been added to the loading queue
	ACTIVATING_LOADING,           // [set by disk thread] Chunk is being loaded and populated by disk i/o thread
	ACTIVATING_LOAD_COMPLETE,     // [set by disk thread] Chunk is done loading and ready for main thread to claim

	ACTIVATING_QUEUED_GENERATE,   // [set by main thread] Chunk has been added to the generating queue
	ACTIVATING_GENERATING,        // [set by generator thread] Chunk is being generated & populated by a generator thread
	ACTIVATING_GENERATE_COMPLETE, // [set by generator thread] Chunk is done generating and ready for main thread to claim

	ACTIVE,                       // [set by main thread] Chunk is in m_activeChunks; only main thread can touch it. Lighting and mesh building allowed

	DEACTIVATING_QUEUED_SAVE,     // [set by main thread] Chunk has been deactivated, and is being queued for save
	DEACTIVATING_SAVING,          // [set by disk thread] Chunk is being compressed and saved by disk i/o thread
	DEACTIVATING_SAVE_COMPLETE,   // [set by disk thread] Chunk has been saved and is ready for main thread to claim
	DECONSTRUCTING,               // [set by main thread] Chunk is being destroyed by main thread

	NUM_CHUNK_STATES
};
// -----------------------------------------------------------------------------
struct WorldConstants
{
	Vec4 CameraPosition    = Vec4(0.f, 0.f, 0.f, 0.f);
	Vec4 IndoorLightColor  = Vec4(0.f, 0.f, 0.f, 0.f);
	Vec4 OutdoorLightColor = Vec4(0.f, 0.f, 0.f, 0.f);
	Vec4 SkyColor          = Vec4(0.f, 0.f, 0.f, 0.f);
	float FogNearDistance  = 0.f;
	float FogFarDistance   = 0.f;
	Vec2 WorldPadding      = Vec2::ZERO;
};
// -----------------------------------------------------------------------------
// Block constants
constexpr int BLOCK_SIZE = 1;

// Chunk bit constants
constexpr int CHUNK_BITS_X = 5;
constexpr int CHUNK_BITS_Y = 5;
constexpr int CHUNK_BITS_Z = 7;

// Chunk size constants
constexpr int CHUNK_SIZE_X = 1 << CHUNK_BITS_X;
constexpr int CHUNK_SIZE_Y = 1 << CHUNK_BITS_Y;
constexpr int CHUNK_SIZE_Z = 1 << CHUNK_BITS_Z;
constexpr int CHUNK_BLOCK_TOTAL = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;

// Chunk mask constants
constexpr int CHUNK_MASK_X = CHUNK_SIZE_X - 1;
constexpr int CHUNK_MASK_Y = CHUNK_SIZE_Y - 1;
constexpr int CHUNK_MASK_Z = CHUNK_SIZE_Z - 1;

// Chunk activation constants
constexpr int CHUNK_ACTIVATION_RANGE = 360;
constexpr int CHUNK_DEACTIVATION_RANGE = CHUNK_ACTIVATION_RANGE + CHUNK_SIZE_X + CHUNK_SIZE_Y;
constexpr int CHUNK_ACTIVATION_RADIUS_X = 1 + CHUNK_ACTIVATION_RANGE / CHUNK_SIZE_X;
constexpr int CHUNK_ACTIVATION_RADIUS_Y = 1 + CHUNK_ACTIVATION_RANGE / CHUNK_SIZE_Y;
constexpr int MAX_ACTIVE_CHUNKS = (2 * CHUNK_ACTIVATION_RADIUS_X) * (2 * CHUNK_ACTIVATION_RADIUS_Y);
constexpr int MAX_MESHES_PER_FRAME = 2;
constexpr int CHUNK_MESH_BUILD_RANGE = CHUNK_ACTIVATION_RANGE * CHUNK_ACTIVATION_RANGE;

// Job constants
constexpr int MAX_GENERATION_JOBS = 3000;
constexpr int MAX_LOAD_JOBS = 2;
constexpr int MAX_SAVE_JOBS = 2;

// Noise constants
constexpr unsigned int GAME_SEED = 0u;

constexpr float        DEFAULT_OCTAVE_PERSISTANCE = 0.5f;
constexpr float        DEFAULT_NOISE_OCTAVE_SCALE = 2.0f;
constexpr float        DEFAULT_TERRAIN_HEIGHT = 64.0f;

constexpr float		   RIVER_DEPTH = 8.0f;
constexpr float		   TERRAIN_NOISE_SCALE = 200.0f;
constexpr unsigned int TERRAIN_NOISE_OCTAVES = 5u;

constexpr float		   HUMIDITY_NOISE_SCALE = 800.0f;
constexpr unsigned int HUMIDITY_NOISE_OCTAVES = 4u;

constexpr float		   TEMPERATURE_RAW_NOISE_SCALE = 0.0075f;
constexpr float        TEMPERATURE_NOISE_SCALE = 400.0f;
constexpr unsigned int TEMPERATURE_NOISE_OCTAVES = 4u;

constexpr float        HILLINESS_NOISE_SCALE = 250.0f;
constexpr unsigned int HILLINESS_NOISE_OCTAVES = 4u;

constexpr float        OCEAN_START_THRESHOLD = 0.0f;
constexpr float        OCEAN_END_THRESHOLD = 0.5f;
constexpr float        OCEAN_DEPTH = 30.0f;

constexpr float        OCEANESS_NOISE_SCALE = 600.0f;
constexpr unsigned int OCEANESS_NOISE_OCTAVES = 3u;

constexpr int   MIN_DIRT_OFFSET_Z = 3;
constexpr int   MAX_DIRT_OFFSET_Z = 4;
constexpr float MIN_SAND_HUMIDITY = 0.4f;
constexpr float MAX_SAND_HUMIDITY = 0.7f;
constexpr int   SEA_LEVEL_Z = CHUNK_SIZE_Z / 2;

constexpr float ICE_TEMPERATURE_MAX = 0.37f;
constexpr float ICE_TEMPERATURE_MIN = 0.0f;
constexpr float ICE_DEPTH_MIN = 0.0f;
constexpr float ICE_DEPTH_MAX = 8.0f;

constexpr float MIN_SAND_DEPTH_HUMIDITY = 0.4f;
constexpr float MAX_SAND_DEPTH_HUMIDITY = 0.0f;
constexpr float SAND_DEPTH_MIN = 0.0f;
constexpr float SAND_DEPTH_MAX = 6.0f;

// A04 world gen
constexpr float SQUASH_MULT = 3.5f;
constexpr int   SURFACE_LAYER_DEPTH = 3;
constexpr float DENSITY_NOISE_SCALE = 128.f;
constexpr int   DENSITY_OCTAVES = 8;
constexpr float SEA_LEVEL = 60.f;

// Continent shaping
constexpr float CONTINENT_SCALE = 256.f;
constexpr int   CONTINENT_OCTAVES = 2;

// Biome noise
constexpr int BIOME_OCTAVES = 4;
constexpr float CONTINENTALNESS_SCALE = 512.f;
constexpr float EROSIION_SCALE = 256.f;
constexpr float PEAKVALLEY_SCALE = 256.f;
constexpr float TEMPERATURE_SCALE = 512.f;
constexpr float HUMIDITY_SCALE = 512.f;

// Ore chance constants
constexpr float COAL_CHANCE = 0.05f;
constexpr float IRON_CHANCE = 0.02f;
constexpr float GOLD_CHANCE = 0.005f;
constexpr float DIAMOND_CHANCE = 0.001f;
constexpr int   OBSIDIAN_Z = 1;
constexpr int   LAVA_Z = 0;

// Effect noise
constexpr float LIGHTNING_SCALE = 0.001f;
constexpr int   LIGHTNING_OCTAVES = 9;
constexpr float GLOWSTONE_SCALE = 0.001f;
constexpr int   GLOWSTONE_OCTAVES = 9;

constexpr float FIXED_DELTASECONDS = 1.f / 60.f;
constexpr float CAMERA_SMOOTHING = 1.f;
constexpr int   INVENTORY_SIZE = 10;
constexpr int   MAX_STACK_IN_SLOT = 64;
// -----------------------------------------------------------------------------
extern App* g_theApp;
extern Game* g_theGame;
extern Renderer* g_theRenderer;
extern RandomNumberGenerator* g_rng;
extern InputSystem* g_theInput;
extern AudioSystem* g_theAudio;
extern JobSystem* g_theJobSystem;
extern Window* g_theWindow;
// -----------------------------------------------------------------------------
// Tree stamps
void      BuildTreeStamps();
TreeStamp MakeSmallOakTree();
TreeStamp MakeLargeOakTree();
TreeStamp MakeSpruceTree();
TreeStamp MakeAcaciaTree();
TreeStamp MakeBirchTree();
TreeStamp MakeJungleTree();
TreeStamp MakeCactus();
TreeStamp MakeSnowySpruceTree();
// -----------------------------------------------------------------------------
// Raycast voxel helpers
constexpr float MAX_STEP = 99999.f;
// Computes how far along the ray to move per block in each axis
Vec3 ComputeRayStep(Vec3 const& direction);

// Determines the direction (+1 or -1) of ray travel per axis
IntVec3 ComputeStepDir(Vec3 const& direction);

// Computes the initial rayLength (distance to first voxel boundary on each axis)
Vec3 ComputeInitialRayLength(const Vec3& start, const Vec3& dir, const IntVec3& block, const Vec3& rayStep);
// -----------------------------------------------------------------------------