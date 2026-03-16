#include "EditorState.hpp"
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include <sstream>
#include <iomanip>

EditorState::EditorState(StateStack& stack, Context context)
	: State(stack, context)
	, m_level_data()
	, m_zoom_level(1.0f)
	, m_is_panning(false)
	, m_show_grid(true)
	, m_grid_size(32.0f)
	, m_is_dirty(false)
	, m_current_filename("")
	, m_status_text(context.fonts->Get(Font::kMain), "Level Editor Mode - Empty Level", 20)
	, m_mouse_pos_text(context.fonts->Get(Font::kMain), "(0, 0)", 16)

{
	//Setup Editor View (initially matching window size)
	sf::Vector2f windowSize = sf::Vector2f(context.window->getSize());
	m_editor_view.setSize(windowSize);
	m_editor_view.setCenter({ windowSize.x / 2.f, windowSize.y / 2.f });

	//Setup UI View (fixed size matching window resolution)
	m_ui_view.setSize(windowSize);
	m_ui_view.setCenter({ windowSize.x / 2.f , windowSize.y / 2.f });

	// Setup Status Text
	m_status_text.setFont(context.fonts->Get(Font::kMain));
	m_status_text.setString("Level Editor Mode - Empty Level");
	m_status_text.setCharacterSize(20);
	m_status_text.setPosition({ 10.f, 10.f });
	m_status_text.setFillColor(sf::Color::White);

	//Setup Mouse Position Text
	m_mouse_pos_text.setFont(context.fonts->Get(Font::kMain));
	m_mouse_pos_text.setString("(0, 0)");
	m_mouse_pos_text.setCharacterSize(16);
	m_mouse_pos_text.setPosition({ 10.f, 40.f });
	m_mouse_pos_text.setFillColor(sf::Color::Yellow);

	//Setup UI Background panel
	m_ui_background.setSize(sf::Vector2f(windowSize.x, 70.f));
	m_ui_background.setFillColor(sf::Color(0, 0, 0, 150));
	m_ui_background.setPosition({ 0.f, 0.f });

	//Initialize default level
	m_level_data.m_world_bounds = sf::FloatRect({ 0.f, 0.f }, { 1600.f, 900.f });
	m_level_data.m_metadata.m_grid_size = static_cast<int>(m_grid_size);
}

void EditorState::Draw()
{
	sf::RenderWindow& window = *GetContext().window;

	window.setView(m_editor_view);

	//Draw Background (dark gray to distinguish from game)
	sf::RectangleShape bg(m_level_data.m_world_bounds.size);
	bg.setPosition(m_level_data.m_world_bounds.position);
	bg.setFillColor(sf::Color(40, 40, 40));
	window.draw(bg);

	if (m_show_grid)
	{
		DrawGrid();
	}

	DrawBounds();

	//Draw Highlight for current grid cell
	sf::Vector2f snappedPos = GetSnappedPosition(m_world_mouse_pos);
	sf::RectangleShape highlight(sf::Vector2f(m_grid_size, m_grid_size));
	highlight.setPosition(snappedPos);
	highlight.setFillColor(sf::Color(255, 255, 255, 50));
	highlight.setOutlineColor(sf::Color::Yellow);
	highlight.setOutlineThickness(1.f);
	window.draw(highlight);

	//Draw UI
	window.setView(m_ui_view);
	window.draw(m_ui_background);
	window.draw(m_status_text);
	window.draw(m_mouse_pos_text);
}

bool EditorState::Update(sf::Time dt)
{
	UpdateMousePosition();
	HandleCameraMovement(dt);
	return true;
}

bool EditorState::HandleEvent(const sf::Event& event)
{
	if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>())
	{
		//Press Escape to leave editor
		if (keyPressed->code == sf::Keyboard::Key::Escape)
		{
			RequestStackPop();
			RequestStackPush(StateID::kMenu);
		}
		//Toggle Grid
		else if (keyPressed->code == sf::Keyboard::Key::G)
		{
			m_show_grid = !m_show_grid;
		}
	}
	else if (const auto* mouseWheeled = event.getIf<sf::Event::MouseWheelScrolled>())
	{
		if (mouseWheeled->wheel == sf::Mouse::Wheel::Vertical)
		{
			HandleCameraZoom(mouseWheeled->delta);
		}
	}
	else if (const auto* mousePressed = event.getIf<sf::Event::MouseButtonPressed>())
	{
		//Middle click to start pan
		if (mousePressed->button == sf::Mouse::Button::Middle)
		{
			m_is_panning = true;
			m_pan_start_mouse_pos = sf::Vector2f(GetContext().window->mapPixelToCoords(
				sf::Vector2i(mousePressed->position.x, mousePressed->position.y), m_ui_view));
			m_pan_start_camera_center = m_editor_view.getCenter();
		}
	}
	else if (const auto* mouseReleased = event.getIf<sf::Event::MouseButtonReleased>())
	{
		if (mouseReleased->button == sf::Mouse::Button::Middle)
		{
			m_is_panning = false;
		}
	}

	return false;
}

void EditorState::UpdateMousePosition()
{
	sf::RenderWindow& window = *GetContext().window;
	m_pixel_mouse_pos = sf::Mouse::getPosition(window);
	m_world_mouse_pos = window.mapPixelToCoords(m_pixel_mouse_pos, m_editor_view);

	//Update text
	std::stringstream ss;
	ss << "Pos: (" << static_cast<int>(m_world_mouse_pos.x) << ", "
		<< static_cast<int>(m_world_mouse_pos.y) << ")";

	//Add snapped pos info
	sf::Vector2f snapped = GetSnappedPosition(m_world_mouse_pos);
	ss << " | Grid: (" << static_cast<int>(snapped.x) << ", " << static_cast<int>(snapped.y) << ")";

	m_mouse_pos_text.setString(ss.str());
}

void EditorState::HandleCameraMovement(sf::Time dt)
{
	sf::RenderWindow& window = *GetContext().window;

	//Handle Pan
	if (m_is_panning)
	{
		//Get current mouse position in screen coordinates
		//Calculate delta and apply zoom factor so panning feels 1:1
		sf::Vector2f currentMousePos = 
			sf::Vector2f(window.mapPixelToCoords(sf::Mouse::getPosition(window), m_ui_view));
		sf::Vector2f delta = m_pan_start_mouse_pos - currentMousePos;
		delta *= m_zoom_level;

		m_editor_view.setCenter(m_pan_start_camera_center + delta);
	}

	//Handle keyboard movement (WASD or Arrows)
	float panSpeed = 500.f * m_zoom_level * dt.asSeconds();
	sf::Vector2f movement(0.f, 0.f);

	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) 
		|| sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
		movement.y -= panSpeed;
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) 
		|| sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
		movement.y += panSpeed;
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) 
		|| sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
		movement.x -= panSpeed;
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) 
		|| sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
		movement.x += panSpeed;

	if (movement.x != 0.f || movement.y != 0.f)
	{
		m_editor_view.move(movement);
	}
}

void EditorState::HandleCameraZoom(float delta)
{
	float zoomFactor = (delta > 0) ? 0.9f : 1.11f;

	m_zoom_level *= zoomFactor;

	if (m_zoom_level < 0.2f) m_zoom_level = 0.2f;
	if (m_zoom_level > 5.0f) m_zoom_level = 5.0f;

	m_editor_view.zoom(zoomFactor);
}

void EditorState::DrawGrid()
{
	sf::RenderWindow& window = *GetContext().window;

	//Calculate visible area to only draw grid where camera sees
	sf::Vector2f viewSize = m_editor_view.getSize();
	sf::Vector2f viewCenter = m_editor_view.getCenter();

	float left = viewCenter.x - viewSize.x / 2.f;
	float right = viewCenter.x + viewSize.x / 2.f;
	float top = viewCenter.y - viewSize.y / 2.f;
	float bottom = viewCenter.y + viewSize.y / 2.f;

	//Align start positions to grid
	int startX = static_cast<int>(std::floor(left / m_grid_size)) * static_cast<int>(m_grid_size);
	int startY = static_cast<int>(std::floor(top / m_grid_size)) * static_cast<int>(m_grid_size);

	m_grid_vertices.clear();
	m_grid_vertices.setPrimitiveType(sf::PrimitiveType::Lines);

	sf::Color gridColor(100, 100, 100, 100);

	//Vertical and Horizontal lines
	for (float x = startX; x <= right; x += m_grid_size)
	{
		m_grid_vertices.append(sf::Vertex({ x, top }, gridColor));
		m_grid_vertices.append(sf::Vertex({ x, bottom }, gridColor));
	}

	for (float y = startY; y <= bottom; y += m_grid_size)
	{
		m_grid_vertices.append(sf::Vertex({ left, y }, gridColor));
		m_grid_vertices.append(sf::Vertex({ right, y }, gridColor));
	}

	window.draw(m_grid_vertices);
}

void EditorState::DrawBounds()
{
	sf::RenderWindow& window = *GetContext().window;

	sf::RectangleShape boundsRect(m_level_data.m_world_bounds.size);
	boundsRect.setPosition(m_level_data.m_world_bounds.position);
	boundsRect.setFillColor(sf::Color::Transparent);
	boundsRect.setOutlineColor(sf::Color::Red);
	boundsRect.setOutlineThickness(2.f * m_zoom_level);//Keep thickness consistent regardless of zoom

	window.draw(boundsRect);
}

sf::Vector2f EditorState::GetSnappedPosition(const sf::Vector2f& position) const
{
	float snappedX = std::floor(position.x / m_grid_size) * m_grid_size;
	float snappedY = std::floor(position.y / m_grid_size) * m_grid_size;
	return sf::Vector2f(snappedX, snappedY);
}