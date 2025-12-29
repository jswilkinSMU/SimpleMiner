#include "Game/BlockDefinition.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

std::vector<BlockDefinition*> BlockDefinition::s_blockDefs;

BlockDefinition::BlockDefinition(XmlElement const& blockElement)
{
	// Parse block name
	m_blockName = ParseXmlAttribute(blockElement, "name", m_blockName);

	// Parse block bools
	m_isVisible = ParseXmlAttribute(blockElement, "isVisible", m_isVisible);
	m_isSolid   = ParseXmlAttribute(blockElement, "isSolid", m_isSolid);
	m_isOpaque  = ParseXmlAttribute(blockElement, "isOpaque", m_isOpaque);

	// Parse block coords
	m_topSpriteCoords	 = ParseXmlAttribute(blockElement, "topSpriteCoords", m_topSpriteCoords);
	m_bottomSpriteCoords = ParseXmlAttribute(blockElement, "bottomSpriteCoords", m_bottomSpriteCoords);
	m_sideSpriteCoords	 = ParseXmlAttribute(blockElement, "sideSpriteCoords", m_sideSpriteCoords);

	// Parse block lighting
	m_indoorLight = ParseXmlAttribute(blockElement, "indoorLighting", m_indoorLight);
	m_outdoorLight = ParseXmlAttribute(blockElement, "outdoorLighting", m_outdoorLight);
}

void BlockDefinition::InitializeBlockDefinitions()
{
	XmlDocument blockDefsXml;
	char const* filePath = "Data/Definitions/BlockSpriteSheet_BlockDefinitions.xml";
	XmlError result = blockDefsXml.LoadFile(filePath);
	GUARANTEE_OR_DIE(result == tinyxml2::XML_SUCCESS, Stringf("Failed to open required block definitions file \"%s\"", filePath));

	XmlElement* rootElement = blockDefsXml.RootElement();
	GUARANTEE_OR_DIE(rootElement, "RootElement not found!");

	XmlElement* blockDefElement = rootElement->FirstChildElement();
	while (blockDefElement)
	{
		std::string elementName = blockDefElement->Name();
		GUARANTEE_OR_DIE(elementName == "BlockDefinition", Stringf("Root child element in %s was <%s>, must be <BlockDefinition>!", filePath, elementName.c_str()));
		BlockDefinition* newLevelDef = new BlockDefinition(*blockDefElement);
		s_blockDefs.push_back(newLevelDef);
		blockDefElement = blockDefElement->NextSiblingElement();
	}
}

void BlockDefinition::ClearBlockDefinitions()
{
	s_blockDefs.clear();
}

BlockDefinition* BlockDefinition::GetBlockByName(std::string const& blockName)
{
	for (int blockIndex = 0; blockIndex < static_cast<int>(s_blockDefs.size()); ++blockIndex)
	{
		if (s_blockDefs[blockIndex]->m_blockName == blockName)
		{
			return s_blockDefs[blockIndex];
		}
	}
	return nullptr;
}
