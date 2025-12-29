#include "Game/Block.hpp"
#include "Game/BlockDefinition.hpp"

void Block::SetBlockType(uint8_t blockType)
{
	m_blockType = blockType;

	BlockDefinition const& def = *BlockDefinition::s_blockDefs[blockType];
	SetIsSolid(def.m_isSolid);
	SetIsVisible(def.m_isVisible);
	SetIsFullOpaque(def.m_isOpaque);
}

uint8_t Block::GetBlockType() const
{
	return m_blockType;
}

void Block::SetOutdoorLight(uint8_t outdoorLight)
{
	outdoorLight &= 0x0F;
	m_lightInfluenceData = (m_lightInfluenceData & 0x0F) | (outdoorLight << 4);
}

uint8_t Block::GetOutdoorLight() const
{
	return (m_lightInfluenceData >> 4) & 0x0F;
}

void Block::SetIndoorLight(uint8_t indoorLight)
{
	indoorLight &= 0x0F;
	m_lightInfluenceData = (m_lightInfluenceData & 0xF0) | indoorLight;
}

uint8_t Block::GetIndoorLight() const
{
	return m_lightInfluenceData & 0x0F;
}

bool Block::IsSky() const
{
	return (m_flags & BLOCK_BIT_MASK_IS_SKY) != 0;
}

void Block::SetIsSky(bool isSky)
{
	if (isSky)
	{
		m_flags |= BLOCK_BIT_MASK_IS_SKY;
	}
	else
	{
		m_flags &= ~BLOCK_BIT_MASK_IS_SKY;
	}
}

bool Block::IsLightDirty() const
{
	return (m_flags & BLOCK_BIT_MASK_IS_LIGHT_DIRTY) != 0;
}

void Block::SetIsLightDirty(bool isLightDirty)
{
	if (isLightDirty)
	{
		m_flags |= BLOCK_BIT_MASK_IS_LIGHT_DIRTY;
	}
	else
	{
		m_flags &= ~BLOCK_BIT_MASK_IS_LIGHT_DIRTY;
	}
}

bool Block::IsFullOpaque() const
{
	return (m_flags & BLOCK_BIT_MASK_IS_FULL_OPAQUE) != 0;
}

void Block::SetIsFullOpaque(bool isFullOpaque)
{
	if (isFullOpaque)
	{
		m_flags |= BLOCK_BIT_MASK_IS_FULL_OPAQUE;
	}
	else
	{
		m_flags &= ~BLOCK_BIT_MASK_IS_FULL_OPAQUE;
	}
}

bool Block::IsSolid() const
{
	return (m_flags & BLOCK_BIT_MASK_IS_SOLID) != 0;
}

void Block::SetIsSolid(bool isSolid)
{
	if (isSolid)
	{
		m_flags |= BLOCK_BIT_MASK_IS_SOLID;
	}
	else
	{
		m_flags &= ~BLOCK_BIT_MASK_IS_SOLID;
	}
}

bool Block::IsVisible() const
{
	return (m_flags & BLOCK_BIT_MASK_IS_VISIBLE) != 0;
}

void Block::SetIsVisible(bool isVisible)
{
	if (isVisible)
	{
		m_flags |= BLOCK_BIT_MASK_IS_VISIBLE;
	}
	else
	{
		m_flags &= ~BLOCK_BIT_MASK_IS_VISIBLE;
	}
}
