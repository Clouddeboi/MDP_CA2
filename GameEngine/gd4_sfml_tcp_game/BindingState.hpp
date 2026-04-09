#pragma once
#include "State.hpp"
#include "Container.hpp"
#include "InputDevice.hpp"
#include "PlayerBindingManager.hpp"
#include "PlayerBindingDisplay.hpp"
#include "NetworkSession.hpp" 
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include <vector>
#include <optional>

class BindingState : public State
{
public:
	BindingState(StateStack& stack, Context context);
	virtual void Draw() override;
	virtual bool Update(sf::Time dt) override;
	virtual bool HandleEvent(const sf::Event& event) override;
	bool IsLocalControllableSlot(int index) const;

private:
	void AddPlayer(const InputDeviceInfo& device);
	void RemovePlayer(int index);
	int GetJoinedPlayerCount() const;
	bool CanAddMorePlayers() const;
	bool AreAllPlayersReady() const;
	void UpdateColorAvailability();

	int FindFirstFreeColorIndex(int blockedIndex = -1) const;
	void RebuildColorTakenFromSlots();
	void ApplyRemoteSlotState(int slotIndex, int colorIndex, bool ready);

private:
	static constexpr int kMaxPlayers = 20;
	TextureHolder m_textures;

	sf::Sprite m_background_sprite;
	std::optional<sf::Text> m_title_text;
	std::optional<sf::Text> m_instructions_text;
	std::array<std::optional<sf::Text>, kMaxPlayers> m_player_status_text;
	std::optional<sf::Text> m_ready_text;

	InputDeviceDetector m_device_detector;
	std::vector<PlayerBinding> m_joined_players;
	PlayerBindingManager m_binding_manager;

	bool m_all_players_bound;
	sf::Time m_elapsed_time;

	static constexpr int kGridColumns = 4;
	static constexpr int kGridRows = 5;
	std::vector<PlayerBindingDisplay> m_player_slots;

	std::vector<sf::Color> m_all_colors;
	std::vector<bool> m_color_taken;

	bool m_network_mode = false;
	int m_local_player_index = 0;
	int m_last_sent_color = -1;
	bool m_last_sent_ready = false;

	sf::Time m_network_sync_timer = sf::Time::Zero;
	sf::Time m_network_sync_interval = sf::milliseconds(250);
};