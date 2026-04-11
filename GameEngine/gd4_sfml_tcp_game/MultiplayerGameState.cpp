#include "MultiplayerGameState.hpp"
#include "MissionStatus.hpp"
#include "InputDevice.hpp"
#include "PlayerBindingConfig.hpp"
#include "NetworkProtocol.hpp"
#include "NetworkSession.hpp"
#include <iostream>
#include <algorithm>
#include <cstdint>

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
	if (GetContext().network && GetContext().network->IsClient())
	{
		PollNetworkGameplay();
	}

	m_world.Update(dt);

	if (m_has_new_snapshot)
	{
		//Map snapshot entries to local player aircraft by player id index
		for (const auto& s : m_latest_snapshot)
		{
			const int playerIdx = static_cast<int>(s.id);
			Aircraft* a = m_world.GetPlayerAircraft(playerIdx);
			if (!a)
				continue;

			a->setPosition({ s.x, s.y });

			const int currentHp = a->GetHitPoints();
			if (s.hp < static_cast<std::uint8_t>(currentHp))
			{
				a->Damage(currentHp - static_cast<int>(s.hp));
			}
			else if (s.hp > static_cast<std::uint8_t>(currentHp))
			{
				a->Repair(static_cast<int>(s.hp) - currentHp);
			}
		}

		m_has_new_snapshot = false;
	}

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
	if (GetContext().network && GetContext().network->IsClient())
	{
		sf::Packet p;
		p << static_cast<std::uint8_t>(Client::PacketType::kStateUpdate);
		p << static_cast<std::uint8_t>(0);

		GetContext().network->SendGameplayPacket(p);
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

void MultiplayerGameState::PollNetworkGameplay()
{
	if (!GetContext().network)
		return;

	while (true)
	{
		sf::Packet p;
		if (!GetContext().network->PollGameplayPacket(p))
			break;

		HandleServerPacket(p);
	}
}

void MultiplayerGameState::HandleServerPacket(sf::Packet& packet)
{
	std::uint8_t type = 0;
	packet >> type;

	const auto packetType = static_cast<Server::PacketType>(type);

	switch (packetType)
	{
	case Server::PacketType::kPlayerConnect:
	{
		std::uint8_t aircraftId = 0;
		float x = 0.f, y = 0.f;
		packet >> aircraftId >> x >> y;
	}
	break;

	case Server::PacketType::kPlayerDisconnect:
	{
		std::uint8_t aircraftId = 0;
		packet >> aircraftId;
	}
	break;

	case Server::PacketType::kUpdateClientState:
	{
		float worldScroll = 0.f;
		std::uint8_t count = 0;
		packet >> worldScroll >> count;

		m_latest_world_scroll = worldScroll;
		m_latest_snapshot.clear();
		m_latest_snapshot.reserve(count);

		for (std::uint8_t i = 0; i < count; ++i)
		{
			NetActorState s;
			packet >> s.id >> s.x >> s.y >> s.hp >> s.ammo;
			m_latest_snapshot.push_back(s);
		}

		m_has_new_snapshot = true;
	}
	break;

	case Server::PacketType::kSpawnEnemy:
	case Server::PacketType::kSpawnPickup:
	case Server::PacketType::kSpawnSelf:
	case Server::PacketType::kPlayerEvent:
	case Server::PacketType::kPlayerRealtimeChange:
	case Server::PacketType::kMissionSuccess:
	default:
		break;
	}
}