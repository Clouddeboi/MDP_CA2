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
		TileTypeInfo platformInfo(TileType::kPlatform, "Platform", 18.f, 18.f);

		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 0,   0 }, { 18, 18 }), "P1_S");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 18,  0 }, { 18, 18 }), "P1_L");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 36,  0 }, { 18, 18 }), "P1_M");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 54,  0 }, { 18, 18 }), "P1_R");

		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 0,  18 }, { 18, 18 }), "P2_S");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 18, 18 }, { 18, 18 }), "P2_L");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 36, 18 }, { 18, 18 }), "P2_M");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 54, 18 }, { 18, 18 }), "P2_R");

		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 0,  36 }, { 18, 18 }), "P3_S");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 18, 36 }, { 18, 18 }), "P3_L");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 36, 36 }, { 18, 18 }), "P3_M");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 54, 36 }, { 18, 18 }), "P3_R");

		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 0,  54 }, { 18, 18 }), "P4_S");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 18, 54 }, { 18, 18 }), "P4_L");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 36, 54 }, { 18, 18 }), "P4_M");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 54, 54 }, { 18, 18 }), "P4_R");

		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 0,  72 }, { 18, 18 }), "P5_S");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 18, 72 }, { 18, 18 }), "P5_L");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 36, 72 }, { 18, 18 }), "P5_M");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 54, 72 }, { 18, 18 }), "P5_R");

		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 0,  90 }, { 18, 18 }), "P6_S");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 18, 90 }, { 18, 18 }), "P6_L");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 36, 90 }, { 18, 18 }), "P6_M");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 54, 90 }, { 18, 18 }), "P6_R");

		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 0, 108 }, { 18, 18 }), "P7_S");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 18,108 }, { 18, 18 }), "P7_L");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 36,108 }, { 18, 18 }), "P7_M");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 54,108 }, { 18, 18 }), "P7_R");

		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 0, 126 }, { 18, 18 }), "P8_S");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 18,126 }, { 18, 18 }), "P8_L");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 36,126 }, { 18, 18 }), "P8_M");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 54,126 }, { 18, 18 }), "P8_R");

		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 72,0 }, { 18, 18 }), "P9_L");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 90,0 }, { 18, 18 }), "P9_R");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 72,18 }, { 18, 18 }), "P10_L");
		platformInfo.m_variants.emplace_back(TextureID::kPlatform, sf::IntRect({ 90,18 }, { 18, 18 }), "P10_R");

		m_tile_types.push_back(platformInfo);
	}

	//Box tile type (just keep is as the original for now)
	{
		TileTypeInfo boxInfo(TileType::kBox, "Box", 18.f, 18.f);

		boxInfo.m_variants.emplace_back(TextureID::kBox, sf::IntRect({ 108, 18 }, { 18, 18 }), "C1");

		m_tile_types.push_back(boxInfo);
	}

	//Register Player Spawn tile type
	{
		TileTypeInfo spawnInfo(TileType::kPlayerSpawn, "Player Spawn", 18.f, 18.f);

		//Player spawns don't need texture variants, they're just markers
		spawnInfo.m_variants.emplace_back(TextureID::kEntities, sf::IntRect({ 0, 0 }, { 18, 18 }), "Spawn Point");

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
	//types.push_back(TileType::kPlayerSpawn);
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
	if (type == TileType::kPlayerSpawn) return "Player Spawn";
	if (type == TileType::kNone) return "Eraser/None";

	const TileTypeInfo* info = GetTileTypeInfo(type);
	return info ? info->m_type_name : "Unknown Tool";
}