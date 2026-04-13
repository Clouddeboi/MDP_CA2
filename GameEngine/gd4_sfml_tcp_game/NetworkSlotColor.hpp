#pragma once
#include <SFML/Graphics/Color.hpp>
#include <cstdint>
#include <cstddef>

// Returns a distinct, visually-strong color for a given network slot.
// Both host and client call this with the same ID → always get the same color.
// inline (not static) so including this in multiple .cpp files won't cause
// "multiply defined symbol" linker errors.
inline sf::Color NetworkSlotColor(std::uint8_t networkId)
{
	static const sf::Color kPalette[] = {
		sf::Color(220,  50,  50),   // 0 — red    (host)
		sf::Color(50, 120, 220),   // 1 — blue   (client)
		sf::Color(50, 200,  80),   // 2 — green
		sf::Color(230, 160,  20),   // 3 — orange
		sf::Color(180,  60, 220),   // 4 — purple
		sf::Color(20, 200, 200),   // 5 — teal
	};
	constexpr std::size_t kCount = sizeof(kPalette) / sizeof(kPalette[0]);
	return kPalette[networkId % kCount];
}