#pragma once

#include <string>

namespace PlayerNameReader
{
    std::string GetName(int playerIndex, const std::string& filename = "Media/Textures/PlayerName.txt");
}