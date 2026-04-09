#include "GameState.hpp"
#include "Player.hpp"
#include "MissionStatus.hpp"
#include "InputDevice.hpp"
#include "PlayerBindingManager.hpp"
#include "PlayerBindingConfig.hpp"
#include "SoundPlayer.hpp"
#include <iostream> 
#include <algorithm>

GameState::GameState(StateStack& stack, Context context)
	: State(stack, context)
	, m_world(*context.window, *context.fonts, *context.sounds, PlayerBindingConfig::GetInstance().GetPlayerCount())
	, m_players()
	, m_sounds(*context.sounds)
{
	//Play the music
	context.music->Play(MusicThemes::kMissionTheme);

	auto& config = PlayerBindingConfig::GetInstance();

	int player_count = config.GetPlayerCount();
	player_count = std::max(1, std::min(player_count, PlayerBindingConfig::GetMaxPlayers()));

	//Create players
	for (int i = 0; i < player_count; ++i)
	{
		m_players.emplace_back(i);
	}

	if (config.HasBindings())
	{
		std::cout << "[GAME] Using player bindings from binding screen (" << player_count << " players):\n";

		//Apply bindings from binding screen for all configured players
		for (int i = 0; i < player_count; ++i)
		{
			auto device = config.GetPlayerDevice(i);
			if (device.has_value())
			{
				std::cout << "[GAME] Player " << i << " -> "
					<< InputDeviceDetector::GetDeviceDescription(device.value()) << "\n";

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
				//Default fallback for unbound slots
				m_players[i].SetJoystickId(-1);
			}
		}
	}
	else
	{
		//Fallback: old auto assignment logic across all runtime players
		std::cout << "[GAME] No bindings found, using auto-assignment for " << player_count << " players\n";

		int assigned_controllers = 0;
		for (int i = 0; i < sf::Joystick::Count; ++i)
		{
			if (sf::Joystick::isConnected(i))
			{
				auto id = sf::Joystick::getIdentification(i);
				std::cout << "Controller connected at startup: id=" << i
					<< " name=\"" << id.name.toAnsiString() << "\"\n";

				if (assigned_controllers < static_cast<int>(m_players.size()))
				{
					m_players[assigned_controllers].SetJoystickId(i);
					std::cout << "[GAME] Assigned joystick " << i << " to player " << assigned_controllers << "\n";
					assigned_controllers++;
				}
			}
		}
	}
}

void GameState::Draw()
{
	m_world.Draw();
}

bool GameState::Update(sf::Time dt)
{

	m_world.Update(dt);

	if (m_world.ShouldReturnToMenu())
	{
		RequestStackClear();
		RequestStackPush(StateID::kMenu);
		return false;
	}

	if (!m_world.HasAlivePlayer())
	{
		m_players[0].SetMissionStatus(MissionStatus::kMissionFailure);
		RequestStackPush(StateID::kGameOver);
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

bool GameState::HandleEvent(const sf::Event& event)
{
	if (const auto* joystickConnected = event.getIf<sf::Event::JoystickConnected>())
	{
		auto id = sf::Joystick::getIdentification(joystickConnected->joystickId);
		std::cout << "Controller connected: id=" << joystickConnected->joystickId
			<< " name=\"" << id.name.toAnsiString() << "\"\n";

		//Try to assign to a player without a controller
		for (auto& player : m_players)
		{
			if (player.GetJoystickId() < 0)
			{
				player.SetJoystickId(static_cast<int>(joystickConnected->joystickId));
				std::cout << "[GAME] Assigned joystick " << joystickConnected->joystickId
					<< " to player " << player.GetPlayerId() << "\n";
				break;
			}
		}
	}
	else if (const auto* joystickDisconnected = event.getIf<sf::Event::JoystickDisconnected>())
	{
		std::cout << "Controller disconnected: id=" << joystickDisconnected->joystickId << "\n";

		//Remove joystick from player
		for (auto& player : m_players)
		{
			if (player.GetJoystickId() == static_cast<int>(joystickDisconnected->joystickId))
			{
				player.SetJoystickId(-1);
				std::cout << "[GAME] Removed joystick from player " << player.GetPlayerId() << "\n";
				break;
			}
		}
	}

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
