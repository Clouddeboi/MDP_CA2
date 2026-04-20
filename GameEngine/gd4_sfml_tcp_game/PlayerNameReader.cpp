#include "PlayerNameReader.hpp"
#include <fstream>

namespace PlayerNameReader
{
    std::string GetName(int playerIndex, const std::string& filename)
    {
        std::ifstream file(filename);
        std::string line;
        int current = 0;

        while (std::getline(file, line))
        {
            if (current == playerIndex)
            {
                //7 characters max
                return line.length() > 7 ? line.substr(0, 7) : line;
            }
            current++;
        }

        //Fallback name if the file doesn't exist or doesn't have enough lines
        std::string fallback = "Player" + std::to_string(playerIndex + 1);
        return fallback.length() > 7 ? fallback.substr(0, 7) : fallback;
    }
}