#pragma once
#include "TileType.hpp"
#include "TextureID.hpp"
#include <SFML/Graphics/Rect.hpp>
#include <vector>
#include <string>

//Defines texture information for a tile variant
struct TileVariantInfo
{
	TileVariantInfo(TextureID texture, const sf::IntRect& rect, const std::string& name)
		: m_texture_id(texture)
		, m_texture_rect(rect)
		, m_variant_name(name)
	{
	}

	TextureID m_texture_id;
	sf::IntRect m_texture_rect;
	std::string m_variant_name;
};

//Information about a tile type
struct TileTypeInfo
{
	TileTypeInfo(TileType type, const std::string& name, float defaultWidth, float defaultHeight)
		: m_type(type)
		, m_type_name(name)
		, m_default_width(defaultWidth)
		, m_default_height(defaultHeight)
	{
	}

	TileType m_type;
	std::string m_type_name;
	float m_default_width;
	float m_default_height;
	std::vector<TileVariantInfo> m_variants;
};

class TileRegistry
{
public:
	static TileRegistry& GetInstance();

	//Initialize the registry with all tile types and variants
	void Initialize();

	//Get tile type info
	const TileTypeInfo* GetTileTypeInfo(TileType type) const;

	//Get variant info for a specific tile type
	const TileVariantInfo* GetVariant(TileType type, int variantIndex) const;

	//Get number of variants for a tile type
	int GetVariantCount(TileType type) const;

	//Get all tile types
	std::vector<TileType> GetAllTileTypes() const;

	//Convert tile type to string
	static std::string TileTypeToString(TileType type);

	//Get display name for tile type
	std::string GetTileTypeName(TileType type) const;

private:
	TileRegistry() = default;
	~TileRegistry() = default;
	TileRegistry(const TileRegistry&) = delete;
	TileRegistry& operator=(const TileRegistry&) = delete;

	std::vector<TileTypeInfo> m_tile_types;
};