#include "PlayerNameReader.hpp"
#include <fstream>
#include <iostream>

namespace PlayerNameReader
{
    std::string GetName(int playerIndex, const std::string& filename)
    {
        std::ifstream file(filename);

        if (!file.is_open())
        {
            std::cerr << "[ERROR] Could not open name file: " << filename
                << " - Using fallback name.\n";
        }
        else
        {
            std::cout << "[DEBUG] Successfully opened " << filename << "\n";
        }

        std::string line;
        int current = 0;

        while (std::getline(file, line))
        {
            if (current == playerIndex)
            {
                std::string name = line.length() > 7 ? line.substr(0, 7) : line;
                std::cout << "[DEBUG] Found name for index " << playerIndex << ": " << name << "\n";
                return name;
            }
            current++;
        }

        std::string fallback = "Player" + std::to_string(playerIndex + 1);
        return fallback.length() > 7 ? fallback.substr(0, 7) : fallback;
    }
}