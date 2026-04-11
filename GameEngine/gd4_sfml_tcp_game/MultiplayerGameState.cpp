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

	RebuildNetworkPlayerMap();
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
		//Apply only mapped/known local players for now
		for (const auto& s : m_latest_snapshot)
		{
			//If not mapped to a local player, keep as known remote actor
			if (!IsKnownLocalNetworkId(s.id))
			{
				m_known_remote_network_ids.insert(s.id);
				m_world.SpawnNetworkActor(s.id, { s.x, s.y }, sf::Color::Cyan);
				m_world.UpdateNetworkActorState(s.id, { s.x, s.y }, s.hp, s.ammo);
				continue;
			}

			auto it = m_net_to_local_player_index.find(s.id);
			if (it == m_net_to_local_player_index.end())
				continue;

			const int playerIdx = it->second;
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

		OnRemotePlayerConnected(aircraftId, x, y);
	}
	break;

	case Server::PacketType::kPlayerDisconnect:
	{
		std::uint8_t aircraftId = 0;
		packet >> aircraftId;

		OnRemotePlayerDisconnected(aircraftId);
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

void MultiplayerGameState::RebuildNetworkPlayerMap()
{
	m_net_to_local_player_index.clear();

	//Network id uses player index for locally instantiated players.
	for (int i = 0; i < static_cast<int>(m_players.size()); ++i)
	{
		m_net_to_local_player_index[static_cast<std::uint8_t>(i)] = i;
	}
}

bool MultiplayerGameState::IsKnownLocalNetworkId(std::uint8_t networkId) const
{
	return m_net_to_local_player_index.find(networkId) != m_net_to_local_player_index.end();
}

void MultiplayerGameState::OnRemotePlayerConnected(std::uint8_t networkId, float x, float y)
{
	if (IsKnownLocalNetworkId(networkId))
		return;

	m_known_remote_network_ids.insert(networkId);
	m_world.SpawnNetworkActor(networkId, { x, y }, sf::Color::Cyan);

	std::cout << "[MP] Remote player connected: id=" << static_cast<int>(networkId)
		<< " pos=(" << x << ", " << y << ")\n";
}

void MultiplayerGameState::OnRemotePlayerDisconnected(std::uint8_t networkId)
{
	if (IsKnownLocalNetworkId(networkId))
		return;

	m_known_remote_network_ids.erase(networkId);
	m_world.RemoveNetworkActor(networkId);

	std::cout << "[MP] Remote player disconnected: id=" << static_cast<int>(networkId) << "\n";
}