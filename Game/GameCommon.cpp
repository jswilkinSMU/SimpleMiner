#include "Game/GameCommon.h"
#include "Engine/Math/Vec3.h"
#include "Engine/Math/MathUtils.h"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Core/Vertex_PCU.h"
#include "Engine/Renderer/Renderer.h"

std::unordered_map<std::string, TreeStamp> g_treeStamps;

void BuildTreeStamps()
{
	g_treeStamps["oak_small"] = MakeSmallOakTree();
	g_treeStamps["oak_large"] = MakeLargeOakTree();
	g_treeStamps["spruce"] = MakeSpruceTree();
	g_treeStamps["birch"] = MakeBirchTree();
	g_treeStamps["acacia"] = MakeAcaciaTree();
	g_treeStamps["jungle"] = MakeJungleTree();
	g_treeStamps["cactus"] = MakeCactus();
	g_treeStamps["snowy_spruce"] = MakeSnowySpruceTree();
}

TreeStamp MakeSmallOakTree()
{
	TreeStamp stamp;
	int trunkHeight = 4;

	// Trunk
	for (int trunkZ = 0; trunkZ < trunkHeight; ++trunkZ)
	{
		stamp.blocks.emplace_back(IntVec3(0, 0, trunkZ), BLOCKTYPE_OAKLOG);
	}

	// Leaves
	for (int leafZ = trunkHeight - 2; leafZ <= trunkHeight + 2; ++leafZ)
	{
		int radius = 2 - abs(leafZ - trunkHeight);
		for (int leafY = -radius; leafY <= radius; ++leafY)
		{
			for (int leafX = -radius; leafX <= radius; ++leafX)
			{
				if (leafX * leafX + leafY * leafY + (leafZ - trunkHeight) * (leafZ - trunkHeight) < 3)
				{
					stamp.blocks.emplace_back(IntVec3(leafX, leafY, leafZ), BLOCKTYPE_OAKLEAVES);
				}
			}
		}
	}

	stamp.radius = 2;
	return stamp;
}

TreeStamp MakeLargeOakTree()
{
	TreeStamp stamp;
	int trunkHeight = 7;

	// Trunk
	for (int trunkZ = 0; trunkZ < trunkHeight; ++trunkZ)
	{
		stamp.blocks.emplace_back(IntVec3(0, 0, trunkZ), BLOCKTYPE_OAKLOG);
	}

	// Leaves
	for (int leafZ = trunkHeight - 2; leafZ <= trunkHeight + 2; ++leafZ)
	{
		int radius = 2 - abs(leafZ - trunkHeight);
		for (int leafY = -radius; leafY <= radius; ++leafY)
		{
			for (int leafX = -radius; leafX <= radius; ++leafX)
			{
				if (leafX * leafX + leafY * leafY + (leafZ - trunkHeight) * (leafZ - trunkHeight) < 27.5)
				{
					stamp.blocks.emplace_back(IntVec3(leafX, leafY, leafZ), BLOCKTYPE_OAKLEAVES);
				}
			}
		}
	}

	stamp.radius = 2;
	return stamp;
}

TreeStamp MakeSpruceTree()
{
	TreeStamp stamp;
	int trunkHeight = 7;

	// Trunk
	for (int trunkZ = 0; trunkZ < trunkHeight; ++trunkZ)
	{
		stamp.blocks.emplace_back(IntVec3(0, 0, trunkZ), BLOCKTYPE_SPRUCELOG);
	}

	// Leaves
	for (int leafZ = trunkHeight - 2; leafZ <= trunkHeight + 2; ++leafZ)
	{
		int radius = 2 - abs(leafZ - trunkHeight);
		for (int leafY = -radius; leafY <= radius; ++leafY)
		{
			for (int leafX = -radius; leafX <= radius; ++leafX)
			{
				if (leafX * leafX + leafY * leafY + (leafZ - trunkHeight) * (leafZ - trunkHeight) < 5)
				{
					stamp.blocks.emplace_back(IntVec3(leafX, leafY, leafZ), BLOCKTYPE_SPRUCELEAVES);
				}
			}
		}
	}

	stamp.radius = 2;
	return stamp;
}

TreeStamp MakeAcaciaTree()
{
	TreeStamp stamp;
	int trunkHeight = 7;

	// Trunk
	for (int trunkZ = 0; trunkZ < trunkHeight; ++trunkZ)
	{
		stamp.blocks.emplace_back(IntVec3(0, 0, trunkZ), BLOCKTYPE_ACACIALOG);
	}

	// Leaves
	for (int leafZ = trunkHeight; leafZ <= trunkHeight + 2; ++leafZ)
	{
		int radius = 2 - abs(leafZ - trunkHeight);
		for (int leafY = -radius; leafY <= radius; ++leafY)
		{
			for (int leafX = -radius; leafX <= radius; ++leafX)
			{
				if (leafX * leafX + leafY * leafY + (leafZ - trunkHeight) * (leafZ - trunkHeight) < 6)
				{
					stamp.blocks.emplace_back(IntVec3(leafX, leafY, leafZ), BLOCKTYPE_ACACIALEAVES);
				}
			}
		}
	}

	stamp.radius = 2;
	return stamp;
}

TreeStamp MakeBirchTree()
{
	TreeStamp stamp;
	int trunkHeight = 7;

	// Trunk
	for (int trunkZ = 0; trunkZ < trunkHeight; ++trunkZ)
	{
		stamp.blocks.emplace_back(IntVec3(0, 0, trunkZ), BLOCKTYPE_BIRCHLOG);
	}

	// Leaves
	for (int leafZ = trunkHeight - 2; leafZ <= trunkHeight + 2; ++leafZ)
	{
		int radius = 2 - abs(leafZ - trunkHeight);
		for (int leafY = -radius; leafY <= radius; ++leafY)
		{
			for (int leafX = -radius; leafX <= radius; ++leafX)
			{
				if (leafX * leafX + leafY * leafY + (leafZ - trunkHeight) * (leafZ - trunkHeight) < 6)
				{
					stamp.blocks.emplace_back(IntVec3(leafX, leafY, leafZ), BLOCKTYPE_BIRCHLEAVES);
				}
			}
		}
	}

	stamp.radius = 2;
	return stamp;
}

TreeStamp MakeJungleTree()
{
	TreeStamp stamp;
	int trunkHeight = 7;

	// Trunk
	for (int trunkZ = 0; trunkZ < trunkHeight; ++trunkZ)
	{
		stamp.blocks.emplace_back(IntVec3(0, 0, trunkZ), BLOCKTYPE_JUNGLELOG);
	}

	// Leaves
	for (int leafZ = trunkHeight - 2; leafZ <= trunkHeight + 2; ++leafZ)
	{
		int radius = 2 - abs(leafZ - trunkHeight);
		for (int leafY = -radius; leafY <= radius; ++leafY)
		{
			for (int leafX = -radius; leafX <= radius; ++leafX)
			{
				if (leafX * leafX + leafY * leafY + (leafZ - trunkHeight) * (leafZ - trunkHeight) < 6)
				{
					stamp.blocks.emplace_back(IntVec3(leafX, leafY, leafZ), BLOCKTYPE_JUNGLELEAVES);
				}
			}
		}
	}

	stamp.radius = 2;
	return stamp;
}

TreeStamp MakeCactus()
{
	TreeStamp stamp;
	int trunkHeight = 3;

	// Trunk
	for (int trunkZ = 0; trunkZ < trunkHeight; ++trunkZ)
	{
		stamp.blocks.emplace_back(IntVec3(0, 0, trunkZ), BLOCKTYPE_CACTUS);
	}

	stamp.radius = 1;
	return stamp;
}

TreeStamp MakeSnowySpruceTree()
{
	TreeStamp stamp;
	int trunkHeight = 7;

	// Trunk
	for (int trunkZ = 0; trunkZ < trunkHeight; ++trunkZ)
	{
		stamp.blocks.emplace_back(IntVec3(0, 0, trunkZ), BLOCKTYPE_SPRUCELOG);
	}

	// Leaves
	for (int leafZ = trunkHeight - 2; leafZ <= trunkHeight + 2; ++leafZ)
	{
		int radius = 2 - abs(leafZ - trunkHeight);
		for (int leafY = -radius; leafY <= radius; ++leafY)
		{
			for (int leafX = -radius; leafX <= radius; ++leafX)
			{
				if (leafX * leafX + leafY * leafY + (leafZ - trunkHeight) * (leafZ - trunkHeight) < 8)
				{
					stamp.blocks.emplace_back(IntVec3(leafX, leafY, leafZ), BLOCKTYPE_SPRUCELEAVESSNOW);
				}
			}
		}
	}

	stamp.radius = 2;
	return stamp;
}

Vec3 ComputeRayStep(Vec3 const& direction)
{
	return Vec3(
		direction.x != 0.f ? 1.f / fabsf(direction.x) : MAX_STEP,
		direction.y != 0.f ? 1.f / fabsf(direction.y) : MAX_STEP,
		direction.z != 0.f ? 1.f / fabsf(direction.z) : MAX_STEP
	);
}

IntVec3 ComputeStepDir(Vec3 const& direction)
{
	return IntVec3((direction.x < 0.f) ? -1 : 1, (direction.y < 0.f) ? -1 : 1, (direction.z < 0.f) ? -1 : 1);
}

Vec3 ComputeInitialRayLength(Vec3 const& startPos, Vec3 const& rayDir, IntVec3 const& block, Vec3 const& rayStep)
{
	Vec3 rayLength;

	auto distToBoundary = [&](float startCoord, float blockCoord, float step, float dirComponent)
	{
		return (dirComponent < 0.f) ? (startCoord - blockCoord) * step : ((blockCoord + 1.f) - startCoord) * step;
	};

	rayLength.x = distToBoundary(startPos.x, static_cast<float>(block.x), rayStep.x, rayDir.x);
	rayLength.y = distToBoundary(startPos.y, static_cast<float>(block.y), rayStep.y, rayDir.y);
	rayLength.z = distToBoundary(startPos.z, static_cast<float>(block.z), rayStep.z, rayDir.z);

	return rayLength;
}


