#pragma once
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <vector>
#include <string>
#include "TileType.hpp"

//Represents a single tile in the level
struct TileData
{
	TileData()
		: m_type(TileType::kNone)
		, m_position(0.f, 0.f)
		, m_texture_variant(0)
		, m_width(0.f)
		, m_height(0.f)
	{
	}

	TileData(TileType type, const sf::Vector2f& position, int variant = 0, float width = 64.f, float height = 64.f)
		: m_type(type)
		, m_position(position)
		, m_texture_variant(variant)
		, m_width(width)
		, m_height(height)
	{
	}

	TileType m_type;
	sf::Vector2f m_position;
	int m_texture_variant;  //Texture variant to use
	float m_width;          //For platforms that can have custom widths
	float m_height;         //For platforms that can have custom heights
};

//Represents a player spawn point
struct PlayerSpawnData
{
	PlayerSpawnData()
		: m_spawn_index(0)
		, m_position(0.f, 0.f)
	{
	}

	PlayerSpawnData(int index, const sf::Vector2f& position)
		: m_spawn_index(index)
		, m_position(position)
	{
	}

	int m_spawn_index;
	sf::Vector2f m_position;
};

//Metadata about the level
struct LevelMetadata
{
	LevelMetadata()
		: m_level_name("Untitled Level")
		, m_creation_date("")
		, m_author("Unknown")
		, m_grid_size(32)
	{
	}

	std::string m_level_name;
	std::string m_creation_date;
	std::string m_author;
	int m_grid_size;//Size of grid cells in pixels
};

//Complete level data structure
struct LevelData
{
	LevelData()
		: m_world_bounds({ 0.f, 0.f }, { 1600.f, 900.f })
		, m_metadata()
	{
		//Reserve space for common level sizes
		m_tiles.reserve(100);
		m_player_spawns.reserve(20);
	}

	LevelMetadata m_metadata;
	sf::FloatRect m_world_bounds;//Playing area boundaries
	std::vector<TileData> m_tiles;//All placed tiles
	std::vector<PlayerSpawnData> m_player_spawns;//Player spawn points

	//Validation methods
	bool IsValid() const
	{
		//Must have at least 2 spawn points
		if (m_player_spawns.size() < 2)
			return false;

		//Cannot have more than 20 spawn points
		if (m_player_spawns.size() > 20)
			return false;

		//World bounds must be valid
		if (m_world_bounds.size.x <= 0.f || m_world_bounds.size.y <= 0.f)
			return false;

		return true;
	}

	int GetTileCount() const
	{
		return static_cast<int>(m_tiles.size());
	}

	int GetSpawnCount() const
	{
		return static_cast<int>(m_player_spawns.size());
	}

	void Clear()
	{
		m_tiles.clear();
		m_player_spawns.clear();
	}

	//Helper to check if a spawn index already exists
	bool HasSpawn(int index) const
	{
		for (const auto& spawn : m_player_spawns)
		{
			if (spawn.m_spawn_index == index)
				return true;
		}
		return false;
	}

	//Helper to get spawn by index
	PlayerSpawnData* GetSpawn(int index)
	{
		for (auto& spawn : m_player_spawns)
		{
			if (spawn.m_spawn_index == index)
				return &spawn;
		}
		return nullptr;
	}
};