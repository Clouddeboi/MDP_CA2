#include "BindingState.hpp"
#include "ResourceHolder.hpp"
#include "Utility.hpp"
#include "PlayerBindingConfig.hpp"
#include "NetworkSession.hpp" 
#include "NetworkSlotColor.hpp"
#include <SFML/Graphics/RenderWindow.hpp>
#include <iostream>

/*
 * Code implementation assisted by Claude Sonnet 4.5
 * Used for: "HandleEvent functionality"
 * Original implementation, modified/adapted by Michal Becmer (D00256088) for project requirements
 */

BindingState::BindingState(StateStack& stack, Context context)
    : State(stack, context)
    , m_background_sprite(context.textures->Get(TextureID::kTitleScreen))
    , m_device_detector()
{
    sf::RenderWindow& window = *GetContext().window;
    sf::Vector2f size(window.getSize());

    m_title_text.emplace(context.fonts->Get(Font::kMain), "LOBBY");
    m_title_text->setCharacterSize(60);
    Utility::CentreOrigin(*m_title_text);
    m_title_text->setPosition({ size.x / 2.f, 60.f });

    m_instructions_text.emplace(context.fonts->Get(Font::kMain), "Join in with any input. Host will start the game.");
    m_instructions_text->setCharacterSize(26);
    Utility::CentreOrigin(*m_instructions_text);
    m_instructions_text->setPosition({ size.x / 2.f, 140.f });

    m_info_text.emplace(context.fonts->Get(Font::kMain), "");
    m_info_text->setCharacterSize(24);
    Utility::CentreOrigin(*m_info_text);
    m_info_text->setPosition({ size.x / 2.f, size.y - 80.f });

    const float gridStartX = 200.f;
    const float gridStartY = 200.f;
    const float spacing = 180.f;

    for (int i = 0; i < kMaxPlayers; ++i)
    {
        m_player_slots.emplace_back(*context.fonts, *context.textures);

        int row = i / kGridColumns;
        int col = i % kGridColumns;

        m_player_slots[i].SetPosition({
            gridStartX + col * spacing,
            gridStartY + row * spacing
            });
    }

    m_network_mode = (GetContext().network && GetContext().network->IsActive());

    if (m_network_mode)
    {
        m_local_player_index = GetContext().network->IsHosting() ? 0 : 1;
        m_instructions_text->setString("Waiting in lobby...");
    }

    std::cout << "[Lobby] Simplified lobby initialized\n";
}

void BindingState::Draw()
{
    sf::RenderWindow& window = *GetContext().window;

    window.draw(m_background_sprite);

    if (m_title_text)
        window.draw(*m_title_text);

    if (m_instructions_text)
        window.draw(*m_instructions_text);

    for (const auto& slot : m_player_slots)
        window.draw(slot);

    if (m_info_text)
        window.draw(*m_info_text);
}

bool BindingState::Update(sf::Time dt)
{
    for (auto& slot : m_player_slots)
        slot.Update(dt);

    if (m_network_mode && GetContext().network)
    {
        GetContext().network->PollLobbyPackets();

        int playerIndex = -1;

        while (GetContext().network->ConsumeRemotePlayerJoined(playerIndex))
        {
            EnsurePlayerSlotExists(playerIndex);
        }

        while (GetContext().network->ConsumeRemotePlayerLeft(playerIndex))
        {
            RemovePlayer(playerIndex);
        }

        if (GetContext().network->ConsumeStartGameSignal())
        {
            auto& config = PlayerBindingConfig::GetInstance();
            config.SetPlayerCount(GetJoinedPlayerCount());

            RequestStackPop();
            RequestStackPush(StateID::kMultiplayerGame);
            return false;
        }
    }

    std::string msg = "Players: " + std::to_string(GetJoinedPlayerCount()) +
        " | Host will start the game";
    m_info_text->setString(msg);
    Utility::CentreOrigin(*m_info_text);

    return true;
}

bool BindingState::HandleEvent(const sf::Event& event)
{
    if (m_network_mode && GetContext().network)
    {
        if (const auto* key = event.getIf<sf::Event::KeyPressed>())
        {
            if (key->code == sf::Keyboard::Key::Enter &&
                GetContext().network->IsHosting())
            {
                GetContext().network->SendLobbyStartGame();
                return false;
            }

            if (key->code == sf::Keyboard::Key::Escape)
            {
                GetContext().network->Reset();
                RequestStackClear();
                RequestStackPush(StateID::kMenu);
                return false;
            }
        }

        return true;
    }

    if (m_device_detector.IsInputEvent(event) && CanAddMorePlayers())
    {
        auto device = m_device_detector.DetectDeviceFromEvent(event);

        if (device.has_value())
        {
            AddPlayer(device.value());
        }
    }

    return false;
}

void BindingState::AddPlayer(const InputDeviceInfo& device)
{
    if (!CanAddMorePlayers())
        return;

    int index = static_cast<int>(m_joined_players.size());

    PlayerBinding p;
    p.playerId = index;
    p.device = device;
    m_joined_players.push_back(p);

    m_player_slots[index].SetPlayerInfo(index + 1, device);

    if (m_network_mode)
        m_player_slots[index].SetNetworkMode(true);

    std::cout << "[Lobby] Player joined: " << index << "\n";
}

void BindingState::RemovePlayer(int index)
{
    if (index < 0 || index >= (int)m_joined_players.size())
        return;

    m_joined_players.erase(m_joined_players.begin() + index);

    for (int i = 0; i < kMaxPlayers; ++i)
        m_player_slots[i].Clear();

    for (size_t i = 0; i < m_joined_players.size(); ++i)
    {
        m_joined_players[i].playerId = i;
        m_player_slots[i].SetPlayerInfo(i + 1, m_joined_players[i].device);
    }

    std::cout << "[Lobby] Player removed: " << index << "\n";
}

int BindingState::GetJoinedPlayerCount() const
{
    return static_cast<int>(m_joined_players.size());
}

bool BindingState::CanAddMorePlayers() const
{
    return m_joined_players.size() < kMaxPlayers;
}

void BindingState::EnsurePlayerSlotExists(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= kMaxPlayers)
        return;

    if (playerIndex < (int)m_joined_players.size())
        return;

    InputDeviceInfo dummyDevice;
    dummyDevice.type = InputDeviceType::kKeyboardMouse;

    PlayerBinding p;
    p.playerId = playerIndex;
    p.device = dummyDevice;

    m_joined_players.push_back(p);

    m_player_slots[playerIndex].SetPlayerInfo(playerIndex + 1, dummyDevice);
    m_player_slots[playerIndex].SetNetworkMode(true);

    std::cout << "[Lobby] Network player joined: " << playerIndex << "\n";
}
