#include "Game/BlockIterator.hpp"
#include "Game/Chunk.hpp"
#include "Engine/Math/IntVec3.hpp"

BlockIterator::BlockIterator(Chunk* chunk, int blockIndex)
	:m_chunk(chunk),
	 m_blockIndex(blockIndex)
{
}

BlockIterator::~BlockIterator()
{
}

bool BlockIterator::IsValid() const
{
	return m_chunk != nullptr && m_blockIndex >= 0;
}

Chunk* BlockIterator::GetChunk() const
{
	return m_chunk;
}

IntVec3 BlockIterator::GetBlockCoords() const
{
	if (!m_chunk || m_blockIndex < 0)
	{
		return IntVec3::INVALID;
	}
	return m_chunk->IndexToGlobalCoords(m_blockIndex);
}

Vec3 BlockIterator::GetBlockCenter() const
{
	IntVec3 global = GetBlockCoords();

	if (global == IntVec3::INVALID)
	{
		return Vec3::ZERO;
	}

	Vec3 blockCenter = Vec3(static_cast<float>(global.x) + 0.5f, static_cast<float>(global.y) + 0.5f, static_cast<float>(global.z) + 0.5f);
	return blockCenter;
}

Block* BlockIterator::GetBlock() const
{
	if (!IsValid())
	{
		return nullptr;
	}

	int blockX = m_chunk->IndexToLocalX(m_blockIndex);
	int blockY = m_chunk->IndexToLocalY(m_blockIndex);
	int blockZ = m_chunk->IndexToLocalZ(m_blockIndex);

	return m_chunk->GetBlockAtLocalCoords(blockX, blockY, blockZ);
}

BlockIterator BlockIterator::GetNorthNeighbor() const
{
	if (!IsValid()) 
	{
		return BlockIterator(nullptr, -1);
	}

	int blockX = m_chunk->IndexToLocalX(m_blockIndex);
	int blockY = m_chunk->IndexToLocalY(m_blockIndex);
	int blockZ = m_chunk->IndexToLocalZ(m_blockIndex);

	blockY += 1;

	// Cross into neighbor
	if (blockY >= CHUNK_SIZE_Y)
	{
		if (m_chunk->m_northNeighbor == nullptr)
		{
			return BlockIterator(nullptr, -1);
		}

		blockY = 0;
		int neighborBlockIndex = m_chunk->m_northNeighbor->GetBlockIndex(blockX, blockY, blockZ);
		return BlockIterator(m_chunk->m_northNeighbor, neighborBlockIndex);
	}

	// My chunk
	int neighborBlockIndex = m_chunk->GetBlockIndex(blockX, blockY, blockZ);
	return BlockIterator(m_chunk, neighborBlockIndex);
}

BlockIterator BlockIterator::GetSouthNeighbor() const
{
	if (!IsValid())
	{
		return BlockIterator(nullptr, -1);
	}

	int blockX = m_chunk->IndexToLocalX(m_blockIndex);
	int blockY = m_chunk->IndexToLocalY(m_blockIndex);
	int blockZ = m_chunk->IndexToLocalZ(m_blockIndex);

	blockY -= 1;

	// Cross into neighbor
	if (blockY < 0)
	{
		if (m_chunk->m_southNeighbor == nullptr)
		{
			return BlockIterator(nullptr, -1);
		}

		blockY = CHUNK_MASK_Y;
		int neighborBlockIndex = m_chunk->m_southNeighbor->GetBlockIndex(blockX, blockY, blockZ);
		return BlockIterator(m_chunk->m_southNeighbor, neighborBlockIndex);
	}

	// My chunk
	int neighborBlockIndex = m_chunk->GetBlockIndex(blockX, blockY, blockZ);
	return BlockIterator(m_chunk, neighborBlockIndex);
}

BlockIterator BlockIterator::GetEastNeighbor() const
{
	if (!IsValid())
	{
		return BlockIterator(nullptr, -1);
	}

	int blockX = m_chunk->IndexToLocalX(m_blockIndex);
	int blockY = m_chunk->IndexToLocalY(m_blockIndex);
	int blockZ = m_chunk->IndexToLocalZ(m_blockIndex);

	blockX += 1;

	// Cross into neighbor
	if (blockX >= CHUNK_SIZE_X)
	{
		if (m_chunk->m_eastNeighbor == nullptr)
		{
			return BlockIterator(nullptr, -1);
		}

		blockX = 0;
		int neighborBlockIndex = m_chunk->m_eastNeighbor->GetBlockIndex(blockX, blockY, blockZ);
		return BlockIterator(m_chunk->m_eastNeighbor, neighborBlockIndex);
	}

	// My chunk
	int neighborBlockIndex = m_chunk->GetBlockIndex(blockX, blockY, blockZ);
	return BlockIterator(m_chunk, neighborBlockIndex);
}

BlockIterator BlockIterator::GetWestNeighbor() const
{
	if (!IsValid())
	{
		return BlockIterator(nullptr, -1);
	}

	int blockX = m_chunk->IndexToLocalX(m_blockIndex);
	int blockY = m_chunk->IndexToLocalY(m_blockIndex);
	int blockZ = m_chunk->IndexToLocalZ(m_blockIndex);

	blockX -= 1;

	// Cross into neighbor
	if (blockX < 0)
	{
		if (m_chunk->m_westNeighbor == nullptr)
		{
			return BlockIterator(nullptr, -1);
		}

		blockX = CHUNK_MASK_X;
		int neighborBlockIndex = m_chunk->m_westNeighbor->GetBlockIndex(blockX, blockY, blockZ);
		return BlockIterator(m_chunk->m_westNeighbor, neighborBlockIndex);
	}

	// My chunk
	int neighborBlockIndex = m_chunk->GetBlockIndex(blockX, blockY, blockZ);
	return BlockIterator(m_chunk, neighborBlockIndex);
}

BlockIterator BlockIterator::GetUpNeighbor() const
{
	if (!IsValid())
	{
		return BlockIterator(nullptr, -1);
	}

	int blockX = m_chunk->IndexToLocalX(m_blockIndex);
	int blockY = m_chunk->IndexToLocalY(m_blockIndex);
	int blockZ = m_chunk->IndexToLocalZ(m_blockIndex);

	blockZ += 1;
	if (blockZ >= CHUNK_SIZE_Z)
	{
		return BlockIterator(m_chunk, -1);
	}

	int neighborBlockIndex = m_chunk->GetBlockIndex(blockX, blockY, blockZ);
	return BlockIterator(m_chunk, neighborBlockIndex);
}

BlockIterator BlockIterator::GetDownNeighbor() const
{
	if (!IsValid())
	{
		return BlockIterator(nullptr, -1);
	}

	int blockX = m_chunk->IndexToLocalX(m_blockIndex);
	int blockY = m_chunk->IndexToLocalY(m_blockIndex);
	int blockZ = m_chunk->IndexToLocalZ(m_blockIndex);

	blockZ -= 1;
	if (blockZ < 0)
	{
		return BlockIterator(m_chunk, -1);
	}

	int neighborBlockIndex = m_chunk->GetBlockIndex(blockX, blockY, blockZ);
	return BlockIterator(m_chunk, neighborBlockIndex);
}

BlockIterator BlockIterator::GetNeighbor(BlockFace blockFace) const
{
	switch (blockFace)
	{
		case BLOCK_FACE_EAST:   return GetEastNeighbor();
		case BLOCK_FACE_WEST:   return GetWestNeighbor();
		case BLOCK_FACE_NORTH:  return GetNorthNeighbor();
		case BLOCK_FACE_SOUTH:  return GetSouthNeighbor();
		case BLOCK_FACE_TOP:    return GetUpNeighbor();
		case BLOCK_FACE_BOTTOM: return GetDownNeighbor();
		default: return BlockIterator(nullptr, -1);
	}
}
