#include "PlayerBindingDisplay.hpp"
#include "Utility.hpp"
#include "ResourceHolder.hpp"
#include "Animation.hpp"
#include <SFML/Graphics/RenderTarget.hpp>

PlayerBindingDisplay::PlayerBindingDisplay(FontHolder& fonts, TextureHolder& textures)
	: m_fonts(fonts)
	, m_textures(textures)
	, m_position(0.f, 0.f)
	, m_size(200.f, 200.f)
	, m_player_number(-1)
	, m_player_color(sf::Color::White)
	, m_is_ready(false)
	, m_is_occupied(false)
	, m_player_idle_animation(textures.Get(TextureID::kPlayerIdleAnimation))
	, m_showing_color_picker(false)
	, m_selected_color_index(-1)
	, m_current_color_cursor(0)
{
	m_background.setFillColor(sf::Color(50, 50, 50, 200));
	m_background.setOutlineColor(sf::Color(100, 100, 100));
	m_background.setOutlineThickness(2.f);

	//Configure idle animation
	m_player_idle_animation.SetFrameSize(sf::Vector2i(24, 24));
	m_player_idle_animation.SetNumFrames(4);
	m_player_idle_animation.SetDuration(sf::seconds(0.5f));
	m_player_idle_animation.SetRepeating(true);
	m_player_idle_animation.setScale({ 1.4f, 1.4f });
	Utility::CentreOrigin(m_player_idle_animation);

	m_sprite_preview_bg.setSize({ 80.f, 20.f });
	m_sprite_preview_bg.setFillColor(sf::Color::Transparent);

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

	if (m_showing_color_picker)
	{
		float gridWidth = kColorGridColumns * kColorBoxSpacing;
		float gridStartX = m_position.x + (m_size.x - gridWidth) / 2.f;
		float gridStartY = m_position.y + 115.f;

		for (size_t i = 0; i < m_color_boxes.size(); ++i)
		{
			int row = static_cast<int>(i) / kColorGridColumns;
			int col = static_cast<int>(i) % kColorGridColumns;

			m_color_boxes[i].setPosition({
				gridStartX + (col * kColorBoxSpacing),
				gridStartY + (row * kColorBoxSpacing)
				});
		}
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

	if (m_showing_color_picker)
	{
		for (const auto& box : m_color_boxes)
		{
			target.draw(box, states);
		}
	}

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

void PlayerBindingDisplay::ShowColorPicker(bool show)
{
	m_showing_color_picker = show;
	UpdateLayout();
}

bool PlayerBindingDisplay::IsShowingColorPicker() const
{
	return m_showing_color_picker;
}

void PlayerBindingDisplay::SetAvailableColors(const std::vector<sf::Color>& colors)
{
	m_available_colors = colors;
	m_color_boxes.clear();
	m_color_unavailable.clear();
	m_current_color_cursor = 0;

	//Create color box rectangles
	for (size_t i = 0; i < colors.size() && i < 20; ++i)
	{
		sf::RectangleShape box;
		box.setSize({ kColorBoxSize, kColorBoxSize });
		box.setFillColor(colors[i]);
		box.setOutlineColor(sf::Color::White);
		box.setOutlineThickness(1.f);
		m_color_boxes.push_back(box);
		m_color_unavailable.push_back(false);
	}

	UpdateColorCursorHighlight();
	UpdateLayout();
}

int PlayerBindingDisplay::GetColorIndexAtPosition(const sf::Vector2f& mousePos) const
{
	if (!m_showing_color_picker)
		return -1;

	for (size_t i = 0; i < m_color_boxes.size(); ++i)
	{
		if (m_color_boxes[i].getGlobalBounds().contains(mousePos))
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}

void PlayerBindingDisplay::SelectColorAtIndex(int index)
{
	if (index >= 0 && index < static_cast<int>(m_available_colors.size()))
	{
		m_selected_color_index = index;
		SetPlayerColor(m_available_colors[index]);
		UpdateColorCursorHighlight();
	}
}

int PlayerBindingDisplay::GetSelectedColorIndex() const
{
	return m_selected_color_index;
}

void PlayerBindingDisplay::NavigateColorGrid(int deltaX, int deltaY)
{
	if (!m_showing_color_picker || m_color_boxes.empty())
		return;

	int attempts = 0;
	int maxAttempts = 20; //Prevent infinite loop

	do
	{
		int currentRow = m_current_color_cursor / kColorGridColumns;
		int currentCol = m_current_color_cursor % kColorGridColumns;

		currentCol += deltaX;
		currentRow += deltaY;

		//Wrap around
		if (currentCol < 0) currentCol = kColorGridColumns - 1;
		if (currentCol >= kColorGridColumns) currentCol = 0;
		if (currentRow < 0) currentRow = kColorGridRows - 1;
		if (currentRow >= kColorGridRows) currentRow = 0;

		m_current_color_cursor = currentRow * kColorGridColumns + currentCol;

		//Clamp to available colors
		if (m_current_color_cursor >= static_cast<int>(m_color_boxes.size()))
			m_current_color_cursor = static_cast<int>(m_color_boxes.size()) - 1;

		attempts++;

		//Keep moving if this color is unavailable
	} while (m_current_color_cursor < static_cast<int>(m_color_unavailable.size()) &&
		m_color_unavailable[m_current_color_cursor] &&
		attempts < maxAttempts);

	UpdateColorCursorHighlight();
}

int PlayerBindingDisplay::GetCurrentColorCursor() const
{
	return m_current_color_cursor;
}

void PlayerBindingDisplay::ConfirmColorSelection()
{
	if (m_showing_color_picker && m_current_color_cursor >= 0)
	{
		SelectColorAtIndex(m_current_color_cursor);
		m_showing_color_picker = false;
		UpdateLayout();
	}
}

void PlayerBindingDisplay::UpdateColorCursorHighlight()
{
	//Update visual highlight for cursor position
	for (size_t i = 0; i < m_color_boxes.size(); ++i)
	{
		if (i < m_color_unavailable.size() && m_color_unavailable[i])
		{
			//Unavailable: darken and red outline
			sf::Color dimmedColor = m_color_boxes[i].getFillColor();
			dimmedColor.a = 100;//Semi-transparent
			m_color_boxes[i].setFillColor(dimmedColor);
			m_color_boxes[i].setOutlineColor(sf::Color(100, 100, 100));
			m_color_boxes[i].setOutlineThickness(1.f);
		}
		else if (static_cast<int>(i) == m_current_color_cursor)
		{
			sf::Color fullColor = m_available_colors[i];
			fullColor.a = 255;
			m_color_boxes[i].setFillColor(fullColor);
			m_color_boxes[i].setOutlineColor(sf::Color::White);
			m_color_boxes[i].setOutlineThickness(3.f);
		}
		else if (static_cast<int>(i) == m_selected_color_index)
		{
			sf::Color fullColor = m_available_colors[i];
			fullColor.a = 255;
			m_color_boxes[i].setFillColor(fullColor);
			m_color_boxes[i].setOutlineColor(sf::Color::Yellow);
			m_color_boxes[i].setOutlineThickness(2.f);
		}
		else
		{
			sf::Color fullColor = m_available_colors[i];
			fullColor.a = 255;
			m_color_boxes[i].setFillColor(fullColor);
			m_color_boxes[i].setOutlineColor(sf::Color::White);
			m_color_boxes[i].setOutlineThickness(1.f);
		}
	}
}

void PlayerBindingDisplay::MarkColorAsUnavailable(int colorIndex, bool unavailable)
{
	if (colorIndex >= 0 && colorIndex < static_cast<int>(m_color_unavailable.size()))
	{
		m_color_unavailable[colorIndex] = unavailable;
		UpdateColorCursorHighlight();
	}
}