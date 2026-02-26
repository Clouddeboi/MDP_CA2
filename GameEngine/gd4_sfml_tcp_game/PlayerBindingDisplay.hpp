#pragma once
#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include "ResourceIdentifiers.hpp"
#include "InputDevice.hpp"
#include "Animation.hpp"
#include <optional>

/*
 * Grid-based player slot display for binding screen
 * Shows player number, device, colored sprite preview, and ready status
 */
class PlayerBindingDisplay : public sf::Drawable
{
public:
	PlayerBindingDisplay(FontHolder& fonts, TextureHolder& textures);

	void SetPosition(const sf::Vector2f& position);
	void SetSize(const sf::Vector2f& size);

	// Player slot management
	void SetPlayerInfo(int playerNumber, const InputDeviceInfo& device);
	void SetPlayerColor(const sf::Color& color);
	void SetReady(bool ready);
	void Clear();

	bool IsOccupied() const;
	sf::FloatRect GetBounds() const;

	void Update(sf::Time dt);

private:
	void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
	void UpdateLayout();

private:
	FontHolder& m_fonts;

	TextureHolder& m_textures;
	Animation m_player_idle_animation;
	sf::Time m_animation_time;

	sf::Vector2f m_position;
	sf::Vector2f m_size;

	sf::RectangleShape m_background;
	sf::RectangleShape m_sprite_preview_bg;

	std::optional<sf::Text> m_player_number_text;
	std::optional<sf::Text> m_device_text;
	std::optional<sf::Text> m_ready_indicator;

	int m_player_number;
	sf::Color m_player_color;
	bool m_is_ready;
	bool m_is_occupied;
};