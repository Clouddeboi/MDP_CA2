#include "TileRegistry.hpp"

TileRegistry& TileRegistry::GetInstance()
{
	static TileRegistry instance;
	return instance;
}

void TileRegistry::Initialize()
{
	m_tile_types.clear();

	//Register Platform tile type
	{
		TileTypeInfo platformInfo(TileType::kPlatform, "Platform", 128.f, 32.f);

		//Basic Platform parts
		platformInfo.m_variants.emplace_back(
			TextureID::kPlatform,
			sf::IntRect({ 0, 0 }, { 18, 18 }),
			"Platform Left"
		);

		platformInfo.m_variants.emplace_back(
			TextureID::kPlatform,
			sf::IntRect({ 64, 0 }, { 64, 32 }),
			"Platform Middle"
		);

		platformInfo.m_variants.emplace_back(
			TextureID::kPlatform,
			sf::IntRect({ 128, 0 }, { 64, 32 }),
			"Platform Right"
		);

		platformInfo.m_variants.emplace_back(
			TextureID::kPlatform,
			sf::IntRect({ 192, 0 }, { 64, 32 }),
			"Platform Single"
		);


		m_tile_types.push_back(platformInfo);
	}

	//Box tile type
	{
		TileTypeInfo boxInfo(TileType::kBox, "Box", 64.f, 64.f);

		boxInfo.m_variants.emplace_back(
			TextureID::kBox,
			sf::IntRect({ 0, 0 }, { 64, 64 }),
			"Wooden Crate"
		);

		boxInfo.m_variants.emplace_back(
			TextureID::kBox,
			sf::IntRect({ 64, 0 }, { 64, 64 }),
			"Metal Crate"
		);

		boxInfo.m_variants.emplace_back(
			TextureID::kBox,
			sf::IntRect({ 128, 0 }, { 64, 64 }),
			"Stone Crate"
		);

		m_tile_types.push_back(boxInfo);
	}

	//Register Player Spawn tile type
	{
		TileTypeInfo spawnInfo(TileType::kPlayerSpawn, "Player Spawn", 32.f, 32.f);

		//Player spawns don't need texture variants, they're just markers
		spawnInfo.m_variants.emplace_back(
			TextureID::kEntities,
			sf::IntRect({ 0, 0 }, { 16, 16 }),
			"Spawn Point"
		);

		m_tile_types.push_back(spawnInfo);
	}
}

const TileTypeInfo* TileRegistry::GetTileTypeInfo(TileType type) const
{
	for (const auto& info : m_tile_types)
	{
		if (info.m_type == type)
			return &info;
	}
	return nullptr;
}

const TileVariantInfo* TileRegistry::GetVariant(TileType type, int variantIndex) const
{
	const TileTypeInfo* typeInfo = GetTileTypeInfo(type);
	if (!typeInfo)
		return nullptr;

	if (variantIndex < 0 || variantIndex >= static_cast<int>(typeInfo->m_variants.size()))
		return nullptr;

	return &typeInfo->m_variants[variantIndex];
}

int TileRegistry::GetVariantCount(TileType type) const
{
	const TileTypeInfo* typeInfo = GetTileTypeInfo(type);
	if (!typeInfo)
		return 0;

	return static_cast<int>(typeInfo->m_variants.size());
}

std::vector<TileType> TileRegistry::GetAllTileTypes() const
{
	std::vector<TileType> types;
	for (const auto& info : m_tile_types)
	{
		types.push_back(info.m_type);
	}
	return types;
}

std::string TileRegistry::TileTypeToString(TileType type)
{
	switch (type)
	{
	case TileType::kNone:
		return "None";
	case TileType::kPlatform:
		return "Platform";
	case TileType::kBox:
		return "Box";
	case TileType::kPlayerSpawn:
		return "PlayerSpawn";
	default:
		return "Unknown";
	}
}

std::string TileRegistry::GetTileTypeName(TileType type) const
{
	const TileTypeInfo* info = GetTileTypeInfo(type);
	if (info)
		return info->m_type_name;

	return "Unknown";
}