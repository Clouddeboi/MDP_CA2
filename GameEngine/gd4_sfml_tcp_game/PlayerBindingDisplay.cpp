#include "PlayerBindingDisplay.hpp"
#include "Utility.hpp"
#include "ResourceHolder.hpp"
#include "Animation.hpp"
#include <SFML/Graphics/RenderTarget.hpp>

PlayerBindingDisplay::PlayerBindingDisplay(FontHolder& fonts, TextureHolder& textures)
	: m_fonts(fonts)
	, m_textures(textures)
	, m_position(0.f, 0.f)
	, m_size(200.f, 150.f)
	, m_player_number(-1)
	, m_player_color(sf::Color::White)
	, m_is_ready(false)
	, m_is_occupied(false)
	, m_player_idle_animation(textures.Get(TextureID::kPlayer1Animations))
{
	m_background.setFillColor(sf::Color(50, 50, 50, 200));
	m_background.setOutlineColor(sf::Color(100, 100, 100));
	m_background.setOutlineThickness(2.f);

	//Configure idle animation
	m_player_idle_animation.SetFrameSize(sf::Vector2i(16, 21));
	m_player_idle_animation.SetNumFrames(4);
	m_player_idle_animation.SetDuration(sf::seconds(0.5f));
	m_player_idle_animation.SetRepeating(true);
	m_player_idle_animation.setScale({ 1.4f, 1.4f });
	Utility::CentreOrigin(m_player_idle_animation);

	UpdateLayout();
}

void PlayerBindingDisplay::Update(sf::Time dt)
{
	if (m_is_occupied)
	{
		m_player_idle_animation.Update(dt);
	}
}

void PlayerBindingDisplay::SetPosition(const sf::Vector2f& position)
{
	m_position = position;
	UpdateLayout();
}

void PlayerBindingDisplay::SetSize(const sf::Vector2f& size)
{
	m_size = size;
	UpdateLayout();
}

void PlayerBindingDisplay::SetPlayerInfo(int playerNumber, const InputDeviceInfo& device)
{
	m_player_number = playerNumber;
	m_is_occupied = true;

	m_player_number_text.emplace(m_fonts.Get(Font::kMain), "P" + std::to_string(playerNumber), 24);
	m_player_number_text->setFillColor(sf::Color::White);
	m_player_number_text->setOutlineColor(sf::Color::Black);
	m_player_number_text->setOutlineThickness(2.f);

	std::string deviceDesc = InputDeviceDetector::GetDeviceDescription(device);
	//Shorten long device names
	if (deviceDesc.length() > 15)
		deviceDesc = deviceDesc.substr(0, 12) + "...";

	m_device_text.emplace(m_fonts.Get(Font::kMain), deviceDesc, 16);
	m_device_text->setFillColor(sf::Color(200, 200, 200));
	m_device_text->setOutlineColor(sf::Color::Black);
	m_device_text->setOutlineThickness(1.f);

	UpdateLayout();
}

void PlayerBindingDisplay::SetPlayerColor(const sf::Color& color)
{
	m_player_color = color;
	m_player_idle_animation.SetColor(color);
}

void PlayerBindingDisplay::SetReady(bool ready)
{
	m_is_ready = ready;

	if (ready)
	{
		m_ready_indicator.emplace(m_fonts.Get(Font::kMain), "READY");
		m_ready_indicator->setCharacterSize(16);
		m_ready_indicator->setFillColor(sf::Color::Green);
		m_ready_indicator->setOutlineColor(sf::Color::Black);
		m_ready_indicator->setOutlineThickness(2.f);
		m_background.setOutlineColor(sf::Color::Green);
		m_background.setOutlineThickness(3.f);

		//Add a green box indicator under device text
		m_sprite_preview_bg.setFillColor(sf::Color::Green);
		m_sprite_preview_bg.setSize({ 80.f, 20.f });
	}
	else
	{
		m_ready_indicator.reset();
		m_background.setOutlineColor(sf::Color(100, 100, 100));
		m_background.setOutlineThickness(2.f);

		//Hide the green box
		m_sprite_preview_bg.setFillColor(sf::Color::Transparent);
		m_sprite_preview_bg.setSize({ 64.f, 64.f });
	}

	UpdateLayout();
}

void PlayerBindingDisplay::Clear()
{
	m_is_occupied = false;
	m_is_ready = false;
	m_player_number = -1;
	m_player_number_text.reset();
	m_device_text.reset();
	m_ready_indicator.reset();
	m_background.setFillColor(sf::Color(50, 50, 50, 100));
	m_background.setOutlineColor(sf::Color(80, 80, 80));
}

bool PlayerBindingDisplay::IsOccupied() const
{
	return m_is_occupied;
}

sf::FloatRect PlayerBindingDisplay::GetBounds() const
{
	return sf::FloatRect(m_position, m_size);
}

void PlayerBindingDisplay::UpdateLayout()
{
	m_background.setPosition(m_position);
	m_background.setSize(m_size);

	sf::Vector2f spritePreviewPos = m_position + sf::Vector2f(
		m_size.x / 2.f,
		45.f
	);

	m_player_idle_animation.setPosition(spritePreviewPos);

	if (m_is_ready)
	{
		m_sprite_preview_bg.setPosition({
			m_position.x + (m_size.x - m_sprite_preview_bg.getSize().x) / 2.f,
			m_position.y + 120.f
			});
	}

	if (m_player_number_text)
	{
		Utility::CentreOrigin(*m_player_number_text);
		m_player_number_text->setPosition({
			m_position.x + m_size.x / 2.f,
			m_position.y + 85.f
			});
	}

	if (m_device_text)
	{
		Utility::CentreOrigin(*m_device_text);
		m_device_text->setPosition({
			m_position.x + m_size.x / 2.f,
			m_position.y + 110.f
			});
	}

	if (m_ready_indicator)
	{
		Utility::CentreOrigin(*m_ready_indicator);
		m_ready_indicator->setPosition({
			m_position.x + m_size.x / 2.f,
			m_position.y + 130.f
			});
	}
}

void PlayerBindingDisplay::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
	if (!m_is_occupied)
		return;

	target.draw(m_background, states);
	target.draw(m_player_idle_animation, states);

	if (m_is_ready)
	{
		target.draw(m_sprite_preview_bg, states);
	}

	if (m_player_number_text)
		target.draw(*m_player_number_text, states);

	if (m_device_text)
		target.draw(*m_device_text, states);

	if (m_ready_indicator)
		target.draw(*m_ready_indicator, states);
}

bool PlayerBindingDisplay::IsReady() const
{
	return m_is_ready;
}