#pragma once
#include "State.hpp"
#include "InputDevice.hpp"
#include "PlayerBindingDisplay.hpp"
#include "NetworkSession.hpp"
#include "PlayerBindingManager.hpp"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include <vector>
#include <optional>

class BindingState : public State
{
public:
    BindingState(StateStack& stack, Context context);

    void Draw() override;
    bool Update(sf::Time dt) override;
    bool HandleEvent(const sf::Event& event) override;

private:
    void AddPlayer(const InputDeviceInfo& device);
    void RemovePlayer(int index);

    int GetJoinedPlayerCount() const;
    bool CanAddMorePlayers() const;

    void EnsurePlayerSlotExists(int playerIndex);

private:
    static constexpr int kMaxPlayers = 20;
    static constexpr int kGridColumns = 4;
    static constexpr int kGridRows = 5;

    sf::Sprite m_background_sprite;
    std::optional<sf::Text> m_title_text;
    std::optional<sf::Text> m_instructions_text;
    std::optional<sf::Text> m_info_text;

    InputDeviceDetector m_device_detector;

    std::vector<PlayerBinding> m_joined_players;
    std::vector<PlayerBindingDisplay> m_player_slots;

    bool m_network_mode = false;
    int m_local_player_index = 0;
};