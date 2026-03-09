#include "LevelManager.hpp"
#include "LevelSerializer.hpp"
#include <filesystem>
#include <iostream>
#include <chrono>

LevelManager::LevelManager()
	: m_random_generator(static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count()))
{
	RefreshLevelList();
}

void LevelManager::RefreshLevelList()
{
	m_available_levels.clear();

	std::string levelsDir = LevelSerializer::GetLevelsDirectory();

	//Create directory if it doesn't exist
	if (!std::filesystem::exists(levelsDir))
	{
		std::filesystem::create_directories(levelsDir);
		return;
	}

	//Scan for .lvl files
	try
	{
		for (const auto& entry : std::filesystem::directory_iterator(levelsDir))
		{
			if (entry.is_regular_file())
			{
				std::string path = entry.path().string();
				if (path.ends_with(".lvl"))
				{
					m_available_levels.push_back(path);
					std::cout << "Found level: " << path << std::endl;
				}
			}
		}
	}
	//Catch errors
	catch (const std::filesystem::filesystem_error& e)
	{
		std::cerr << "Error scanning levels directory: " << e.what() << std::endl;
	}

	std::cout << "Total levels found: " << m_available_levels.size() << std::endl;
}

std::string LevelManager::GetRandomLevelPath() const
{
	if (m_available_levels.empty())
		return "";

	std::uniform_int_distribution<size_t> dist(0, m_available_levels.size() - 1);
	return m_available_levels[dist(m_random_generator)];
}

const std::vector<std::string>& LevelManager::GetAvailableLevels() const
{
	return m_available_levels;
}

int LevelManager::GetLevelCount() const
{
	return static_cast<int>(m_available_levels.size());
}

bool LevelManager::HasLevels() const
{
	return !m_available_levels.empty();
}

//Extracts a user friendly level name from the file path (reverse of CreateFilename)
std::string LevelManager::GetLevelNameFromPath(const std::string& path)
{
	//Extract filename from path
	size_t lastSlash = path.find_last_of("/\\");
	std::string filename = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;

	//Remove extension
	size_t lastDot = filename.find_last_of('.');
	if (lastDot != std::string::npos)
		filename = filename.substr(0, lastDot);

	//Remove "level_" prefix if present
	if (filename.find("level_") == 0)
		filename = filename.substr(6);

	//Replace underscores with spaces
	for (char& c : filename)
	{
		if (c == '_')
			c = ' ';
	}

	return filename;
}