#include "MultiplayerGameState.hpp"
#include "MissionStatus.hpp"
#include "InputDevice.hpp"
#include "PlayerBindingConfig.hpp"
#include <iostream>
#include <algorithm>

MultiplayerGameState::MultiplayerGameState(StateStack& stack, Context context)
	: State(stack, context)
	, m_world(*context.window, *context.fonts, *context.sounds, PlayerBindingConfig::GetInstance().GetPlayerCount())
	, m_players()
	, m_sounds(*context.sounds)
{
	//Play the music
	context.music->Play(MusicThemes::kMissionTheme);

	auto& config = PlayerBindingConfig::GetInstance();
	int player_count = std::max(1, std::min(config.GetPlayerCount(), PlayerBindingConfig::GetMaxPlayers()));

	for (int i = 0; i < player_count; ++i)
	{
		m_players.emplace_back(i);

		auto device = config.GetPlayerDevice(i);
		if (device.has_value())
		{
			if (device->type == InputDeviceType::kController)
			{
				m_players[i].SetJoystickId(device->deviceIndex);
			}
			else
			{
				m_players[i].SetJoystickId(-1);
			}
		}
		else
		{
			m_players[i].SetJoystickId(-1);
		}
	}
}

void MultiplayerGameState::Draw()
{
	m_world.Draw();
}

bool MultiplayerGameState::Update(sf::Time dt)
{
	m_world.Update(dt);

	if (m_world.ShouldReturnToMenu())
	{
		RequestStackClear();
		RequestStackPush(StateID::kMenu);
		return false;
	}

	CommandQueue& commands = m_world.GetCommandQueue();

	//Handle input for all players
	for (size_t i = 0; i < m_players.size(); ++i)
	{
		m_players[i].HandleRealTimeInput(commands);

		//Handle aiming for each player
		sf::Vector2f aim = m_players[i].GetJoystickAim();
		const float kAimDeadzone = 0.2f;

		if (std::hypot(aim.x, aim.y) > kAimDeadzone)
		{
			m_world.SetPlayerAimDirection(static_cast<int>(i), aim);
		}
		else
		{
			//Only use mouse aiming if player doesn't have a controller
			if (i == 0 && m_players[i].GetJoystickId() < 0)
			{
				m_world.AimPlayerAtMouse(static_cast<int>(i));
			}
		}
	}

	return true;
}

bool MultiplayerGameState::HandleEvent(const sf::Event& event)
{
	CommandQueue& commands = m_world.GetCommandQueue();

	//Handle events for all players
	for (auto& player : m_players)
	{
		player.HandleEvent(event, commands);
	}

	if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>())
	{
		if (keyPressed->scancode == sf::Keyboard::Scancode::Escape)
			RequestStackPush(StateID::kPause);
	}

	return true;
}