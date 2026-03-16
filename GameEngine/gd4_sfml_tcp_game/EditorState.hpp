#pragma once
#include "State.hpp"
#include "LevelData.hpp"
#include "LevelManager.hpp"
#include <SFML/Graphics/View.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/Sprite.hpp>

class EditorState : public State
{
public:
	EditorState(StateStack& stack, Context context);
	virtual void Draw() override;
	virtual bool Update(sf::Time dt) override;
	virtual bool HandleEvent(const sf::Event& event) override;

private:
	void DrawGrid();
	void DrawBounds();
	void HandleCameraMovement(sf::Time dt);
	void HandleCameraZoom(float delta);
	void UpdateMousePosition();

	//Tile interaction
	void HandleTilePlacement();
	void HandleTileDeletion();
	void SelectNextTileType();
	void SelectPreviousTileType();
	void SelectNextVariant();
	void SelectPreviousVariant();
	void IncreaseTileWidth();
	void DecreaseTileWidth();
	void IncreaseTileHeight();
	void DecreaseTileHeight();
	void AddTile(const sf::Vector2f& position);
	void RemoveTile(const sf::Vector2f& position);

	TileData* GetTileAt(const sf::Vector2f& position);
	void UpdateStatusText();
	void DrawTiles();
	void DrawPreviewTile();

	//Coordinates snapping helper
	sf::Vector2f GetSnappedPosition(const sf::Vector2f& position) const;

private:
	//Level Editor Data
	LevelData m_level_data;
	LevelManager m_level_manager;

	//Camera and View
	sf::View m_editor_view;
	sf::View m_ui_view;
	float m_zoom_level;

	//Mouse interaction
	sf::Vector2f m_world_mouse_pos;
	sf::Vector2i m_pixel_mouse_pos;
	bool m_is_panning;
	sf::Vector2f m_pan_start_mouse_pos;
	sf::Vector2f m_pan_start_camera_center;

	//UI Elements
	sf::Text m_status_text;
	sf::Text m_mouse_pos_text;
	sf::RectangleShape m_ui_background;

	//Grid Settings
	bool m_show_grid;
	float m_grid_size;
	sf::VertexArray m_grid_vertices;

	//Editor Status
	bool m_is_dirty;//Has unsaved changes
	std::string m_current_filename;

	//Tile Placement State
	TileType m_current_tile_type;
	int m_current_variant;
	float m_current_tile_width;
	float m_current_tile_height;
	bool m_is_placing_tile;
	bool m_is_deleting_tile;
	sf::Vector2f m_preview_pos;
	sf::Sprite m_preview_sprite;
	int m_current_spawn_index;
};