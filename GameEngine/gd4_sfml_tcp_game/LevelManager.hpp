#pragma once
#include "LevelData.hpp"
#include <vector>
#include <string>
#include <random>

class LevelManager
{
public:
	LevelManager();

	//Scan the Levels directory and load all available levels
	void RefreshLevelList();

	//Get a random level
	std::string GetRandomLevelPath() const;

	//Get all available level paths
	const std::vector<std::string>& GetAvailableLevels() const;

	//Get number of available levels
	int GetLevelCount() const;

	//Check if any levels are available
	bool HasLevels() const;

	//Get level name from path
	static std::string GetLevelNameFromPath(const std::string& path);

private:
	std::vector<std::string> m_available_levels;

	//mutable allows GetRandomLevelPath to modify the generator state even though it's a const method
	//mt19937 is a high-quality random number generator that is part of the C++ standard library
	mutable std::mt19937 m_random_generator;
};