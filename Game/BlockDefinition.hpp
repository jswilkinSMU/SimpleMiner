#pragma once
#include "Engine/Core/XmlUtils.hpp"
// -----------------------------------------------------------------------------
struct BlockDefinition
{
	BlockDefinition(XmlElement const& blockElement);
	static std::vector<BlockDefinition*> s_blockDefs;
	static void InitializeBlockDefinitions();
	static void ClearBlockDefinitions();
	static BlockDefinition* GetBlockByName(std::string const& blockName);
// -----------------------------------------------------------------------------
	std::string m_blockName = "";
	bool		m_isVisible = false;
	bool		m_isSolid   = false;
	bool		m_isOpaque  = false;
	IntVec2		m_topSpriteCoords	 = IntVec2::ZERO;
	IntVec2		m_bottomSpriteCoords = IntVec2::ZERO;
	IntVec2		m_sideSpriteCoords	 = IntVec2::ZERO;
	int         m_indoorLight = 0;
	int         m_outdoorLight = 0;
};