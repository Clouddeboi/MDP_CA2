#include "EditorState.hpp"
#include "Utility.hpp"
#include "TileRegistry.hpp"
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
	, m_grid_size(18.0f)
	, m_is_dirty(false)
	, m_current_filename("")
	, m_status_text(context.fonts->Get(Font::kMain), "Level Editor Mode - Empty Level", 20)
	, m_mouse_pos_text(context.fonts->Get(Font::kMain), "(0, 0)", 16)
	, m_current_tile_type(TileType::kPlatform)
	, m_current_variant(0)
	, m_current_tile_width(m_grid_size)
	, m_current_tile_height(m_grid_size)
	, m_is_placing_tile(false)
	, m_is_deleting_tile(false)
	, m_current_spawn_index(0)
	, m_preview_sprite(context.textures->Get(TextureID::kEntities))
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
	m_status_text.setCharacterSize(18);
	m_status_text.setPosition({ 10.f, 10.f });
	m_status_text.setFillColor(sf::Color::White);

	//Setup Mouse Position Text
	m_mouse_pos_text.setFont(context.fonts->Get(Font::kMain));
	m_mouse_pos_text.setString("(0, 0)");
	m_mouse_pos_text.setCharacterSize(16);
	m_mouse_pos_text.setPosition({ windowSize.x - 300.f, 10.f });
	m_mouse_pos_text.setFillColor(sf::Color::Yellow);

	//Setup UI Background panel
	m_ui_background.setSize(sf::Vector2f(windowSize.x, 100.f));
	m_ui_background.setFillColor(sf::Color(0, 0, 0, 200));
	m_ui_background.setPosition({ 0.f, 0.f });

	//Initialize default level
	m_level_data.m_world_bounds = sf::FloatRect({ 0.f, 0.f }, { 1600.f, 900.f });
	m_level_data.m_metadata.m_grid_size = static_cast<int>(m_grid_size);

	UpdateStatusText();
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
	DrawTiles();
	DrawPreviewTile();

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
		//Cycle Tile Type
		else if (keyPressed->code == sf::Keyboard::Key::Tab)
		{
			SelectNextTileType();
		}
		//Cycle Variant
		else if (keyPressed->code == sf::Keyboard::Key::E)
		{
			SelectNextVariant();
		}
		else if (keyPressed->code == sf::Keyboard::Key::Q)
		{
			SelectPreviousVariant();
		}
		//Adjust Size (for platforms)
		else if (keyPressed->code == sf::Keyboard::Key::Right)
		{
			IncreaseTileWidth();
		}
		else if (keyPressed->code == sf::Keyboard::Key::Left)
		{
			DecreaseTileWidth();
		}
		else if (keyPressed->code == sf::Keyboard::Key::Down)
		{
			IncreaseTileHeight();
		}
		else if (keyPressed->code == sf::Keyboard::Key::Up)
		{
			DecreaseTileHeight();
		}
		//Adjust Spawn Index
		else if (keyPressed->code == sf::Keyboard::Key::Num1)
		{
			m_current_spawn_index = std::max(0, m_current_spawn_index - 1);
		}
		else if (keyPressed->code == sf::Keyboard::Key::Num2)
		{
			m_current_spawn_index = std::min(19, m_current_spawn_index + 1);
		}

		UpdateStatusText();
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
		else if (mousePressed->button == sf::Mouse::Button::Left)
		{
			//Check if clicking in UI area (top 100 pixels)
			sf::Vector2f uiCoords = sf::Vector2f(GetContext().window->mapPixelToCoords(
				sf::Vector2i(mousePressed->position.x, mousePressed->position.y), m_ui_view));

			if (uiCoords.y > 100.f)//Only place if not clicking UI
			{
				m_is_placing_tile = true;
				HandleTilePlacement();
			}
		}
		else if (mousePressed->button == sf::Mouse::Button::Right)
		{
			m_is_deleting_tile = true;
			HandleTileDeletion();
		}
	}
	else if (const auto* mouseReleased = event.getIf<sf::Event::MouseButtonReleased>())
	{
		if (mouseReleased->button == sf::Mouse::Button::Middle)
		{
			m_is_panning = false;
		}
		else if (mouseReleased->button == sf::Mouse::Button::Left)
		{
			m_is_placing_tile = false;
		}
		else if (mouseReleased->button == sf::Mouse::Button::Right)
		{
			m_is_deleting_tile = false;
		}
	}

	return false;
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

void EditorState::UpdateMousePosition()
{
	sf::RenderWindow& window = *GetContext().window;
	m_pixel_mouse_pos = sf::Mouse::getPosition(window);
	m_world_mouse_pos = window.mapPixelToCoords(m_pixel_mouse_pos, m_editor_view);
	m_preview_pos = GetSnappedPosition(m_world_mouse_pos);

	//Continuous placement while dragging
	if (m_is_placing_tile)
		HandleTilePlacement();
	else if (m_is_deleting_tile)
		HandleTileDeletion();

	//Update text
	std::stringstream ss;
	ss << "Pos: (" << static_cast<int>(m_world_mouse_pos.x) << ", "
		<< static_cast<int>(m_world_mouse_pos.y) << ")";

	ss << " | Grid: (" << static_cast<int>(m_preview_pos.x) << ", " << static_cast<int>(m_preview_pos.y) << ")";
	m_mouse_pos_text.setString(ss.str());
}

void EditorState::HandleTilePlacement()
{
	//Bounds check to ensure we don't start placing way outside the level bounds
	if (!m_level_data.m_world_bounds.contains(m_preview_pos))
		return;

	//Don't place duplicates over the exact same spot with same type/size
	TileData* existingTile = GetTileAt(m_preview_pos);

	if (existingTile)
	{
		//Don't replace if it's identical
		if (existingTile->m_type == m_current_tile_type &&
			existingTile->m_texture_variant == m_current_variant &&
			existingTile->m_width == m_current_tile_width &&
			existingTile->m_height == m_current_tile_height)
		{
			return;
		}

		//Different tile exists, remove it first
		RemoveTile(m_preview_pos);
	}

	AddTile(m_preview_pos);
}

void EditorState::HandleTileDeletion()
{
	RemoveTile(m_preview_pos);
}

void EditorState::AddTile(const sf::Vector2f& position)
{
	if (m_current_tile_type == TileType::kPlayerSpawn)
	{
		//Check if spawn index already exists, if so, move it
		for (auto& spawn : m_level_data.m_player_spawns)
		{
			if (spawn.m_spawn_index == m_current_spawn_index)
			{
				spawn.m_position = position;
				m_is_dirty = true;
				return;
			}
		}

		//New spawn if below max
		if (m_level_data.m_player_spawns.size() < 20)
		{
			m_level_data.m_player_spawns.emplace_back(m_current_spawn_index, position);
			m_is_dirty = true;
		}
	}
	else if (m_current_tile_type != TileType::kNone)
	{
		//Regular tile
		m_level_data.m_tiles.emplace_back(
			m_current_tile_type,
			position,
			m_current_variant,
			m_current_tile_width,
			m_current_tile_height
		);
		m_is_dirty = true;
	}
}

void EditorState::RemoveTile(const sf::Vector2f& position)
{
	//Check spawns
	auto spawnIt = std::remove_if(m_level_data.m_player_spawns.begin(), m_level_data.m_player_spawns.end(),
		[&](const PlayerSpawnData& s) {
			//Delete within a small radius
			return std::abs(s.m_position.x - position.x) < (m_grid_size * 0.5f) &&
				std::abs(s.m_position.y - position.y) < (m_grid_size * 0.5f);
		});

	if (spawnIt != m_level_data.m_player_spawns.end())
	{
		m_level_data.m_player_spawns.erase(spawnIt, m_level_data.m_player_spawns.end());
		m_is_dirty = true;
		return;
	}

	//Check tiles using AABB
	auto tileIt = std::remove_if(m_level_data.m_tiles.begin(), m_level_data.m_tiles.end(),
		[&](const TileData& t) {
			sf::FloatRect bounds(t.m_position, sf::Vector2f(t.m_width, t.m_height));
			//Center the bounds around the position like entities do
			bounds.position.x -= t.m_width * 0.5f;
			bounds.position.y -= t.m_height * 0.5f;
			return bounds.contains(position);
		});

	if (tileIt != m_level_data.m_tiles.end())
	{
		m_level_data.m_tiles.erase(tileIt, m_level_data.m_tiles.end());
		m_is_dirty = true;
	}
}

TileData* EditorState::GetTileAt(const sf::Vector2f& position)
{
	for (auto& t : m_level_data.m_tiles)
	{
		sf::FloatRect bounds(t.m_position, sf::Vector2f(t.m_width, t.m_height));
		bounds.position.x -= t.m_width * 0.5f;
		bounds.position.y -= t.m_height * 0.5f;

		if (bounds.contains(position))
			return &t;
	}
	return nullptr;
}

void EditorState::UpdateStatusText()
{
	std::stringstream ss;
	auto& registry = TileRegistry::GetInstance();

	ss << "Tool: " << registry.GetTileTypeName(m_current_tile_type);

	if (m_current_tile_type == TileType::kPlayerSpawn)
	{
		ss << " (Index: " << m_current_spawn_index << ") | [1/2] Change Index";
	}
	else if (m_current_tile_type != TileType::kNone)
	{
		const auto* variantInfo = registry.GetVariant(m_current_tile_type, m_current_variant);
		if (variantInfo)
		{
			ss << " | Variant: " << variantInfo->m_variant_name << " (" << (m_current_variant + 1)
				<< "/" << registry.GetVariantCount(m_current_tile_type) << ")";
		}

		ss << " | Size: " << m_current_tile_width << "x" << m_current_tile_height;
		ss << " | [Arrow Keys] Resize | [Q/E] Variants";
	}

	ss << "\n[Tab] Cycle Tool | [L-Click] Draw | [R-Click] Erase";

	m_status_text.setString(ss.str());
}

void EditorState::SelectNextTileType()
{
	auto types = TileRegistry::GetInstance().GetAllTileTypes();
	auto it = std::find(types.begin(), types.end(), m_current_tile_type);

	if (it != types.end() && std::next(it) != types.end())
		m_current_tile_type = *std::next(it);
	else
		m_current_tile_type = types.front();

	//Reset selection state for new tool
	m_current_variant = 0;

	const auto* info = TileRegistry::GetInstance().GetTileTypeInfo(m_current_tile_type);
	if (info)
	{
		m_current_tile_width = info->m_default_width;
		m_current_tile_height = info->m_default_height;
	}
}

void EditorState::SelectNextVariant()
{
	int maxVariants = TileRegistry::GetInstance().GetVariantCount(m_current_tile_type);
	if (maxVariants > 1)
		m_current_variant = (m_current_variant + 1) % maxVariants;
}

void EditorState::SelectPreviousVariant()
{
	int maxVariants = TileRegistry::GetInstance().GetVariantCount(m_current_tile_type);
	if (maxVariants > 1)
	{
		m_current_variant = (m_current_variant - 1);
		if (m_current_variant < 0) m_current_variant = maxVariants - 1;
	}
}

void EditorState::IncreaseTileWidth() { m_current_tile_width += m_grid_size; }
void EditorState::DecreaseTileWidth() { m_current_tile_width = std::max(m_grid_size, m_current_tile_width - m_grid_size); }
void EditorState::IncreaseTileHeight() { m_current_tile_height += m_grid_size; }
void EditorState::DecreaseTileHeight() { m_current_tile_height = std::max(m_grid_size, m_current_tile_height - m_grid_size); }

void EditorState::DrawPreviewTile()
{
	sf::RenderWindow& window = *GetContext().window;

	if (m_current_tile_type == TileType::kNone)
		return;

	sf::RectangleShape preview(sf::Vector2f({ m_current_tile_width, m_current_tile_height }));
	//preview.setOrigin({m_current_tile_width / 2.f, m_current_tile_height / 2.f});
	preview.setPosition(m_preview_pos);

	//Style based on type
	if (m_current_tile_type == TileType::kPlayerSpawn)
	{
		preview.setSize({ m_grid_size, m_grid_size });
		//preview.setOrigin({ m_grid_size / 2.f, m_grid_size / 2.f });
		preview.setFillColor(sf::Color(0, 255, 0, 150));

		//Draw spawn number
		sf::Text text(GetContext().fonts->Get(Font::kMain), std::to_string(m_current_spawn_index), 20);
		text.setPosition({ m_preview_pos.x + m_grid_size / 2.f, m_preview_pos.y + m_grid_size / 2.f });
		text.setFillColor(sf::Color::White);
		Utility::CentreOrigin(text);

		window.draw(preview);
		window.draw(text);
	}
	else
	{
		//Try to pull visual from registry
		const auto* variant = TileRegistry::GetInstance().GetVariant(m_current_tile_type, m_current_variant);
		if (variant)
		{
			preview.setTexture(&GetContext().textures->Get(variant->m_texture_id));
			preview.setTextureRect(variant->m_texture_rect);
			preview.setFillColor(sf::Color(255, 255, 255, 180));
		}
		else
		{
			preview.setFillColor(sf::Color(150, 75, 0, 150));
		}
		window.draw(preview);
	}
}

void EditorState::DrawTiles()
{
	sf::RenderWindow& window = *GetContext().window;
	auto& registry = TileRegistry::GetInstance();

	// Draw placed tiles
	for (const auto& tile : m_level_data.m_tiles)
	{
		sf::RectangleShape visual(sf::Vector2f(tile.m_width, tile.m_height));
		//visual.setOrigin({ tile.m_width / 2.f, tile.m_height / 2.f });
		visual.setPosition(tile.m_position);

		const auto* variant = registry.GetVariant(tile.m_type, tile.m_texture_variant);
		if (variant)
		{
			visual.setTexture(&GetContext().textures->Get(variant->m_texture_id));
			visual.setTextureRect(variant->m_texture_rect);
		}
		else
		{
			//Fallback colors matching roughly what world.cpp does
			visual.setFillColor(tile.m_type == TileType::kPlatform ? sf::Color(150, 75, 0) : sf::Color::Yellow);
		}

		window.draw(visual);
	}

	//Draw placed spawns
	for (const auto& spawn : m_level_data.m_player_spawns)
	{
		sf::RectangleShape visual(sf::Vector2f(m_grid_size, m_grid_size));
		//visual.setOrigin({ m_grid_size / 2.f, m_grid_size / 2.f });
		visual.setPosition(spawn.m_position);
		visual.setFillColor(sf::Color(0, 200, 0));

		sf::Text text(GetContext().fonts->Get(Font::kMain), std::to_string(spawn.m_spawn_index), 20);
		text.setPosition(spawn.m_position);
		text.setFillColor(sf::Color::White);
		Utility::CentreOrigin(text);

		window.draw(visual);
		window.draw(text);
	}
}