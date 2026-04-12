#include "MultiplayerGameState.hpp"
#include "MissionStatus.hpp"
#include "InputDevice.hpp"
#include "PlayerBindingConfig.hpp"
#include "NetworkProtocol.hpp"
#include "NetworkSession.hpp"
#include "GameServer.hpp"
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <cmath>

MultiplayerGameState::MultiplayerGameState(StateStack& stack, Context context)
	: State(stack, context)
	, m_world(
		*context.window,
		*context.fonts,
		*context.sounds,
		std::max(2, PlayerBindingConfig::GetInstance().GetPlayerCount())
	)
	, m_players()
	, m_sounds(*context.sounds)
{
	//Play the music
	context.music->Play(MusicThemes::kMissionTheme);

	auto& config = PlayerBindingConfig::GetInstance();

	//In network gameplay, this machine controls one local player by default.
	int player_count = (context.network && context.network->IsActive())
		? 1
		: std::max(1, std::min(config.GetPlayerCount(), PlayerBindingConfig::GetMaxPlayers()));

	for (int i = 0; i < player_count; ++i)
	{
		m_players.emplace_back(i);

		auto device = config.GetPlayerDevice(i);
		if (device.has_value())
		{
			if (device->type == InputDeviceType::kController)
				m_players[i].SetJoystickId(device->deviceIndex);
			else
				m_players[i].SetJoystickId(-1);
		}
		else
		{
			m_players[i].SetJoystickId(-1);
		}
	}

	RebuildNetworkPlayerMap();
	m_world.SetCollisionEnabled(false);
}

void MultiplayerGameState::Draw()
{
	m_world.Draw();
}

bool MultiplayerGameState::Update(sf::Time dt)
{
   sf::Clock update_timer;
	m_perf.sample_window += dt;
	++m_perf.frames;

	if (GetContext().network && GetContext().network->IsClient())
	{
      sf::Clock section_timer;
		PollNetworkGameplay();
       m_perf.net_poll_time_total += section_timer.getElapsedTime();
	}

	if (GetContext().network && GetContext().network->IsHosting())
	{
		GameServer* server = GetContext().network->GetServer();
		if (server)
		{
			std::vector<GameServer::NetAircraftState> hostStates;
			server->CopyAircraftStates(hostStates);

			//Track ids present this tick
			std::unordered_set<std::uint8_t> presentIds;
			for (const auto& s : hostStates)
			{
				presentIds.insert(s.id);

				//Do not overwrite local known player slots on host
				auto localIt = m_net_to_local_player_index.find(s.id);
				if (localIt != m_net_to_local_player_index.end())
				{
					Aircraft* a = m_world.GetPlayerAircraft(localIt->second);
					if (a)
					{
						a->setPosition(s.position);
					}
					continue;
				}

				//Treat as remote actor on host view
				m_known_remote_network_ids.insert(s.id);
				m_world.SpawnNetworkActor(s.id, s.position, sf::Color::Cyan);
				m_world.UpdateNetworkActorState(s.id, s.position, s.hp, s.ammo);
			}

			//Remove stale remote actors not present anymore
			for (auto it = m_known_remote_network_ids.begin(); it != m_known_remote_network_ids.end();)
			{
				if (presentIds.find(*it) == presentIds.end())
				{
					m_world.RemoveNetworkActor(*it);
					m_remote_interp.erase(*it);
					it = m_known_remote_network_ids.erase(it);
				}
				else
				{
					++it;
				}
			}
		}
	}

 sf::Clock world_timer;
	m_world.Update(dt);
	m_perf.world_update_time_total += world_timer.getElapsedTime();

	if (m_has_new_snapshot)
	{
     sf::Clock snapshot_timer;
		for (const auto& s : m_latest_snapshot)
		{
			//Unknown ids in snapshot are ignored unless already known remote players.
			//This prevents enemy/projectile IDs being mis-created as player actors.
			if (!IsKnownLocalNetworkId(s.id))
			{
				if (m_known_remote_network_ids.find(s.id) == m_known_remote_network_ids.end())
				{
					continue;
				}

				auto& interp = m_remote_interp[s.id];
				if (!interp.initialized)
				{
					interp.current = { s.x, s.y };
					interp.target = { s.x, s.y };
					interp.hp = s.hp;
					interp.ammo = s.ammo;
					interp.initialized = true;
				}
				else
				{
					interp.target = { s.x, s.y };
					interp.hp = s.hp;
					interp.ammo = s.ammo;
				}

				continue;
			}

			//Remote known actor path (MUST already be known from connect/spawn event)
			if (m_known_remote_network_ids.find(s.id) != m_known_remote_network_ids.end())
			{
				auto& interp = m_remote_interp[s.id];
				if (!interp.initialized)
				{
					interp.current = { s.x, s.y };
					interp.target = { s.x, s.y };
					interp.hp = s.hp;
					interp.ammo = s.ammo;
					interp.initialized = true;
				}
				else
				{
					interp.target = { s.x, s.y };
					interp.hp = s.hp;
					interp.ammo = s.ammo;
				}
			}
		}

		m_has_new_snapshot = false;
       m_perf.snapshot_apply_time_total += snapshot_timer.getElapsedTime();
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

	m_state_send_timer += dt;
    m_state_force_send_timer += dt;
	if (GetContext().network && GetContext().network->IsClient() && m_state_send_timer >= m_state_send_interval)
	{
		m_state_send_timer = sf::Time::Zero;

		sf::Packet p;
		p << static_cast<std::uint8_t>(Client::PacketType::kStateUpdate);

        std::vector<std::uint8_t> changed_ids;
		changed_ids.reserve(m_players.size());

		const bool force_send = m_state_force_send_timer >= m_state_force_send_interval;

		for (size_t i = 0; i < m_players.size(); ++i)
		{
         auto idIt = m_local_player_to_aircraft_id.find(static_cast<int>(i));
			if (idIt == m_local_player_to_aircraft_id.end())
				continue;

			Aircraft* a = m_world.GetPlayerAircraft(static_cast<int>(i));
          if (!a)
				continue;

			const std::uint8_t aircraft_identifier = idIt->second;
			const sf::Vector2f pos = a->getPosition();
			const std::uint8_t hp = static_cast<std::uint8_t>(std::max(0, std::min(255, a->GetHitPoints())));
			const std::uint8_t ammo = 0;

			auto& last = m_last_sent_local_states[aircraft_identifier];
			const float dx = pos.x - last.position.x;
			const float dy = pos.y - last.position.y;
			const float moved_sq = dx * dx + dy * dy;

			const bool changed = !last.initialized
				|| moved_sq >= 4.f
				|| hp != last.hp
				|| ammo != last.ammo
				|| force_send;

			if (changed)
			{
				changed_ids.push_back(aircraft_identifier);
			}
		}

        p << static_cast<std::uint8_t>(changed_ids.size());

        for (std::uint8_t aircraft_identifier : changed_ids)
		{
            auto localSlotIt = m_net_to_local_player_index.find(aircraft_identifier);
			if (localSlotIt == m_net_to_local_player_index.end())
				continue;

			Aircraft* a = m_world.GetPlayerAircraft(localSlotIt->second);
			if (!a)
				continue;

			const sf::Vector2f pos = a->getPosition();
			const std::uint8_t hp = static_cast<std::uint8_t>(std::max(0, std::min(255, a->GetHitPoints())));
			const std::uint8_t ammo = 0;

			p << aircraft_identifier
				<< pos.x
				<< pos.y
				<< hp
				<< ammo;

			auto& last = m_last_sent_local_states[aircraft_identifier];
			last.position = pos;
			last.hp = hp;
			last.ammo = ammo;
			last.initialized = true;
		}

        if (!changed_ids.empty() || force_send)
		{
			GetContext().network->SendGameplayPacket(p);
			++m_perf.tx_packets;
			m_perf.tx_bytes += p.getDataSize();
		}

		if (force_send)
		{
			m_state_force_send_timer = sf::Time::Zero;
		}
	}

	//Smooth remote actor movement
    sf::Clock interp_timer;
	for (auto& kv : m_remote_interp)
	{
		const std::uint8_t id = kv.first;
		auto& st = kv.second;

		if (!st.initialized)
			continue;

		const sf::Vector2f delta = st.target - st.current;
		const float lerpFactor = std::min(1.f, dt.asSeconds() * 12.f);//Smoothing speed
		st.current += delta * lerpFactor;

		m_world.UpdateNetworkActorState(id, st.current, st.hp, st.ammo);
	}
	m_perf.interp_time_total += interp_timer.getElapsedTime();

	if (GetContext().network && m_state_send_timer > sf::seconds(5.f) && !GetContext().network->IsClientConnected())
	{
		RequestStackClear();
		RequestStackPush(StateID::kMenu);
		return false;
	}

    m_perf.update_time_total += update_timer.getElapsedTime();

	if (m_perf.sample_window >= sf::seconds(1.f))
	{
		const float sample_secs = m_perf.sample_window.asSeconds();
		const float fps = (sample_secs > 0.f) ? static_cast<float>(m_perf.frames) / sample_secs : 0.f;

		auto avg_ms = [frames = m_perf.frames](sf::Time t) -> float
			{
				return (frames > 0) ? (t.asMicroseconds() / 1000.f) / static_cast<float>(frames) : 0.f;
			};

		std::cout
			<< "[MP PERF] fps=" << fps
			<< " rx=" << m_perf.rx_packets << "pkts/" << m_perf.rx_bytes << "B"
			<< " tx=" << m_perf.tx_packets << "pkts/" << m_perf.tx_bytes << "B"
			<< " snapshots=" << m_perf.snapshot_packets << " actors=" << m_perf.snapshot_actors
			<< " capHits=" << m_perf.poll_cap_hits
			<< " remote(+" << m_perf.remote_connects << ",-" << m_perf.remote_disconnects << ")"
			<< " t_ms{update=" << avg_ms(m_perf.update_time_total)
			<< ",net=" << avg_ms(m_perf.net_poll_time_total)
			<< ",world=" << avg_ms(m_perf.world_update_time_total)
			<< ",snapshot=" << avg_ms(m_perf.snapshot_apply_time_total)
			<< ",interp=" << avg_ms(m_perf.interp_time_total)
			<< "}\n";

		m_perf = PerfCounters{};
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
	if (!GetContext().network || !GetContext().network->IsClientConnected())
		return;

	//Prevent frame starvation if socket backlog is large
	const int kMaxPacketsPerFrame = 64;

   for (int i = 0; i < kMaxPacketsPerFrame; ++i)
	{
		sf::Packet p;
		if (!GetContext().network->PollGameplayPacket(p))
			break;

		++m_perf.rx_packets;
		m_perf.rx_bytes += p.getDataSize();
		if (i == kMaxPacketsPerFrame - 1)
			++m_perf.poll_cap_hits;

		HandleServerPacket(p);
	}
}

void MultiplayerGameState::HandleServerPacket(sf::Packet& packet)
{
	std::uint8_t type = 0;
	if (!(packet >> type))
		return;

	const auto packetType = static_cast<Server::PacketType>(type);

	switch (packetType)
	{
	case Server::PacketType::kPlayerConnect:
	{
		std::uint8_t aircraftId = 0;
		float x = 0.f, y = 0.f;
		if (!(packet >> aircraftId >> x >> y))
			return;

		OnRemotePlayerConnected(aircraftId, x, y);
	}
	break;

	case Server::PacketType::kPlayerDisconnect:
	{
		std::uint8_t aircraftId = 0;
		if (!(packet >> aircraftId))
			return;

		//If it was considered local-owned, drop that ownership mapping
		for (auto it = m_local_player_to_aircraft_id.begin(); it != m_local_player_to_aircraft_id.end();)
		{
			if (it->second == aircraftId)
				it = m_local_player_to_aircraft_id.erase(it);
			else
				++it;
		}

		OnRemotePlayerDisconnected(aircraftId);
	}
	break;

	case Server::PacketType::kSpawnSelf:
	{
		std::uint8_t aircraftId = 0;
		float x = 0.f, y = 0.f;
		if (!(packet >> aircraftId >> x >> y))
			return;

		//Assign this authoritative id to first unassigned local slot
		int assignedLocalSlot = -1;

		for (size_t i = 0; i < m_players.size(); ++i)
		{
			if (m_local_player_to_aircraft_id.find(static_cast<int>(i)) == m_local_player_to_aircraft_id.end())
			{
				assignedLocalSlot = static_cast<int>(i);
				break;
			}
		}

		//Fallback to slot 0 if all already mapped
		if (assignedLocalSlot < 0)
			assignedLocalSlot = 0;

		m_local_player_to_aircraft_id[assignedLocalSlot] = aircraftId;
		m_net_to_local_player_index[aircraftId] = assignedLocalSlot;

		Aircraft* a = m_world.GetPlayerAircraft(assignedLocalSlot);
		if (a)
		{
			a->setPosition({ x, y });
		}
	}
	break;

	case Server::PacketType::kUpdateClientState:
	{
		float worldScroll = 0.f;
		std::uint8_t count = 0;
		if (!(packet >> worldScroll >> count))
			return;

		//Sanity cap to avoid malformed/hostile packet blowing up client
		if (count > 200)
			return;

		m_latest_world_scroll = worldScroll;
		m_latest_snapshot.clear();
		m_latest_snapshot.reserve(count);
		++m_perf.snapshot_packets;
		m_perf.snapshot_actors += count;

		for (std::uint8_t i = 0; i < count; ++i)
		{
			NetActorState s;
			if (!(packet >> s.id >> s.x >> s.y >> s.hp >> s.ammo))
				return;

			m_latest_snapshot.push_back(s);

			//If this id belongs to any local-owned slot, keep reverse map fresh
			for (const auto& kv : m_local_player_to_aircraft_id)
			{
				if (kv.second == s.id)
				{
					m_net_to_local_player_index[s.id] = kv.first;
					break;
				}
			}
		}

		m_has_new_snapshot = true;
	}
	break;

	case Server::PacketType::kSpawnEnemy:
	case Server::PacketType::kSpawnPickup:
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

	//Start with no authoritative ownership until kSpawnSelf packets arrive
	m_local_player_to_aircraft_id.clear();
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
    ++m_perf.remote_connects;
	m_world.SpawnNetworkActor(networkId, { x, y }, sf::Color::Cyan);

	std::cout << "[MP] Remote player connected: id=" << static_cast<int>(networkId)
		<< " pos=(" << x << ", " << y << ")\n";
}

void MultiplayerGameState::OnRemotePlayerDisconnected(std::uint8_t networkId)
{
	if (IsKnownLocalNetworkId(networkId))
		return;

	m_known_remote_network_ids.erase(networkId);
  ++m_perf.remote_disconnects;
	m_world.RemoveNetworkActor(networkId);
	m_remote_interp.erase(networkId);

	std::cout << "[MP] Remote player disconnected: id=" << static_cast<int>(networkId) << "\n";
}