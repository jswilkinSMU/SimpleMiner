#pragma once
#include "Game/GameCommon.h"
// -----------------------------------------------------------------------------
class Chunk;
class Block;
// -----------------------------------------------------------------------------
class BlockIterator
{
public:
	BlockIterator(Chunk* chunk, int blockIndex);
	~BlockIterator();

	bool    IsValid() const; 
	Chunk*  GetChunk() const;
	Block*  GetBlock() const;
	IntVec3 GetBlockCoords() const;
	Vec3    GetBlockCenter() const;

	BlockIterator GetNorthNeighbor() const;
	BlockIterator GetSouthNeighbor() const;
	BlockIterator GetEastNeighbor() const;
	BlockIterator GetWestNeighbor() const;
	BlockIterator GetUpNeighbor() const;
	BlockIterator GetDownNeighbor() const;
	BlockIterator GetNeighbor(BlockFace blockFace) const;

public:
	Chunk* m_chunk = nullptr;
	int	   m_blockIndex = -1;
};