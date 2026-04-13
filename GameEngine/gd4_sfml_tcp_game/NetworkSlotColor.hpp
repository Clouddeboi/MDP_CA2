#pragma once
#include <SFML/Graphics/Color.hpp>
#include <cstdint>
#include <cstddef>

inline sf::Color NetworkSlotColor(std::uint8_t networkId) {
	static const sf::Color kPalette[] = {
		sf::Color(220, 50, 50), // 0 — red (host) 
		sf::Color(50, 120, 220), // 1 — blue (client) etc.
		sf::Color(50,200,80),
		sf::Color(230,160,20),
		sf::Color(180,60,220),
		sf::Color(20,200,200),
		sf::Color(255,80,120),
		sf::Color(120,80,255),
		sf::Color(255,215,0),
		sf::Color(0,180,120),
		sf::Color(255,140,0),
		sf::Color(70,70,70),
		sf::Color(160,82,45),
		sf::Color(0,150,255),
		sf::Color(255,20,147),
		sf::Color(154,205,50),
		sf::Color(75,0,130),
		sf::Color(255,105,180),
		sf::Color(64,224,208),
		sf::Color(210,105,30)
	};
	constexpr std::size_t kCount = sizeof(kPalette) / sizeof(kPalette[0]);
	return kPalette[networkId % kCount];
}