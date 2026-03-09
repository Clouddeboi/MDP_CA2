#pragma once
#include "LevelData.hpp"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

class LevelSerializer
{
public:
	//Save and load level data to file
	static bool Save(const LevelData& level, const std::string& filename);
	static bool Load(const std::string& filename, LevelData& level);

	//Get the levels directory path
	static std::string GetLevelsDirectory();

	//Helper to create a safe filename from level name
	static std::string CreateFilename(const std::string& levelName);

private:
	//Helper functions for serialization
	static void WriteTileType(std::ofstream& file, TileType type);
	static TileType ReadTileType(const std::string& typeStr);

	static std::string TileTypeToString(TileType type);
	static TileType StringToTileType(const std::string& str);
};