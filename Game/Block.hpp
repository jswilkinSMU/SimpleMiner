#pragma once
#include <cstdint>
// -----------------------------------------------------------------------------
// Block flag bit masks
constexpr uint8_t BLOCK_BIT_MASK_IS_SKY = (1 << 0);            // I am non-opaque and no opaque blocks are above me
constexpr uint8_t BLOCK_BIT_MASK_IS_LIGHT_DIRTY = (1 << 1);   // A block iterator for me is currently in the dirty light queue.
constexpr uint8_t BLOCK_BIT_MASK_IS_FULL_OPAQUE = (1 << 2);  // I block light, visibility, and hide my neighbors faces.
constexpr uint8_t BLOCK_BIT_MASK_IS_SOLID = (1 << 3);       // Physical objects and physics raycasts collide with me.
constexpr uint8_t BLOCK_BIT_MASK_IS_VISIBLE = (1 << 4);    // I cannot be skipped during chunk mesh rebuilding.
// -----------------------------------------------------------------------------
class Block
{
public:
	// Block Type
	void	SetBlockType(uint8_t blockType);
	uint8_t GetBlockType() const;

	// Light Influence
	void    SetOutdoorLight(uint8_t outdoorLight);
	uint8_t GetOutdoorLight() const;
	void    SetIndoorLight(uint8_t indoorLight);
	uint8_t GetIndoorLight() const;

	// Bit Flags
	bool IsSky() const;
	void SetIsSky(bool isSky);

	bool IsLightDirty() const;
	void SetIsLightDirty(bool isLightDirty);

	bool IsFullOpaque() const;
	void SetIsFullOpaque(bool isFullOpaque);

	bool IsSolid() const;
	void SetIsSolid(bool isSolid);

	bool IsVisible() const;
	void SetIsVisible(bool isVisible);

public:
	uint8_t m_blockType = 0;
	uint8_t m_lightInfluenceData = 0;
	uint8_t m_flags = 0;
};