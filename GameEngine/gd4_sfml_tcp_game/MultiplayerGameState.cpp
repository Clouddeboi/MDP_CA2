#include "MultiplayerGameState.hpp"
#include "MissionStatus.hpp"
#include "InputDevice.hpp"
#include "PlayerBindingConfig.hpp"
#include "NetworkProtocol.hpp"
#include "NetworkSession.hpp"
#include "GameServer.hpp"
#include "NetworkSlotColor.hpp"
#include "PlayerNameReader.hpp"
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
		1  //Only 1 local aircraft per machine; remotes are network actors
	)
	, m_players()
	, m_sounds(*context.sounds)
{
	context.music->Play(MusicThemes::kMissionTheme);

	auto& config = PlayerBindingConfig::GetInstance();

	//Every machine controls exactly 1 local player (always local slot 0)
	m_players.emplace_back(0);

	auto device = config.GetPlayerDevice(0);
	if (device.has_value() && device->type == InputDeviceType::kController)
		m_players[0].SetJoystickId(device->deviceIndex);
	else
		m_players[0].SetJoystickId(-1);

	const bool isHost = (GetContext().network && GetContext().network->IsHosting());

	//Host self-assigns aircraft ID 0
	if (isHost)
	{
		m_local_player_to_aircraft_id[0] = 0;
		m_net_to_local_player_index[0] = 0;
	}

	m_world.SetCollisionEnabled(true);

	if (isHost)
	{
		std::string myName = PlayerNameReader::GetName(0);

		Aircraft* a = m_world.GetPlayerAircraft(0);
		if (a) a->SetPlayerName(myName);

		auto* srv = GetContext().network->GetServer();
		if (srv)
		{
			GameServer::HostEvent nameEv;
			nameEv.type = GameServer::HostEvent::kNameSync;
			nameEv.aircraft_id = 0;
			nameEv.name = myName;
			srv->PushHostEvent(nameEv);
		}
	}

	m_world.SetTotalNetworkPlayerCount(2);

	if (!isHost)
		m_world.SetScoreAuthoritative(false);
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

	if (GetContext().network && (GetContext().network->IsClient() || GetContext().network->IsHosting()))
	{
		sf::Clock section_timer;
		PollNetworkGameplay();
		m_perf.net_poll_time_total += section_timer.getElapsedTime();
	}

	sf::Clock world_timer;
	m_world.Update(dt);
	m_perf.world_update_time_total += world_timer.getElapsedTime();

	if (m_has_new_snapshot)
	{
		sf::Clock snapshot_timer;

		for (const auto& s : m_latest_snapshot)
		{
			if (IsKnownLocalNetworkId(s.id) && s.id != 0)
				continue;

			if (m_known_remote_network_ids.find(s.id) == m_known_remote_network_ids.end())
				continue;

			auto& interp = m_remote_interp[s.id];
			if (!interp.initialized)
			{
				interp.current = { s.x, s.y };
				interp.target = { s.x, s.y };
				interp.velocity = { s.vx, s.vy };
				interp.time_since_snap = sf::Time::Zero;
				interp.hp = s.hp;
				interp.ammo = s.ammo;
				interp.anim = s.anim;
				interp.initialized = true;
			}
			else
			{
				interp.target = { s.x, s.y };
				interp.velocity = { s.vx, s.vy };
				interp.time_since_snap = sf::Time::Zero;
				interp.hp = s.hp;
				interp.ammo = s.ammo;
				interp.anim = s.anim;
			}
		}

		m_has_new_snapshot = false;
		m_perf.snapshot_apply_time_total += snapshot_timer.getElapsedTime();
	}

	if (m_world.ShouldReturnToMenu())
	{
		if (GetContext().music)
			GetContext().music->Stop();
		RequestStackClear();
		RequestStackPush(StateID::kMenu);
		return false;
	}

	CommandQueue& commands = m_world.GetCommandQueue();

	for (size_t i = 0; i < m_players.size(); ++i)
	{
		m_players[i].HandleRealTimeInput(commands);

		sf::Vector2f aim = m_players[i].GetJoystickAim();
		const float kAimDeadzone = 0.2f;

		if (std::hypot(aim.x, aim.y) > kAimDeadzone)
		{
			m_world.SetPlayerAimDirection(static_cast<int>(i), aim);
		}
		else
		{
			if (i == 0 && m_players[i].GetJoystickId() < 0)
			{
				m_world.AimPlayerAtMouse(static_cast<int>(i));
			}
		}
	}

	m_state_send_timer += dt;
	m_state_force_send_timer += dt;

	if (GetContext().network && GetContext().network->IsActive() && m_state_send_timer >= m_state_send_interval)
	{
		m_state_send_timer = sf::Time::Zero;
		const bool force_send = m_state_force_send_timer >= m_state_force_send_interval;


		if (GetContext().network->IsHosting())
		{
			Aircraft* a = m_world.GetPlayerAircraft(0);
			if (a)
			{
				const sf::Vector2f pos = a->getPosition();
				const sf::Vector2f vel = a->GetVelocity();
				const uint8_t hp = static_cast<uint8_t>(std::max(0, std::min(255, a->GetHitPoints())));
				const uint8_t ammo = 0;
				const uint8_t anim = m_world.GetLocalPlayerAnimState(0);

				auto* server = GetContext().network->GetServer();
				if (server)
					server->UpdateHostAircraftState(pos, vel, hp, ammo, anim);
			}
		}
		else if (GetContext().network->IsClient())
		{
			sf::Packet p;
			p << static_cast<std::uint8_t>(Client::PacketType::kStateUpdate);

			std::vector<std::uint8_t> changed_ids;
			changed_ids.reserve(m_players.size());

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
					changed_ids.push_back(aircraft_identifier);
			}

			p << static_cast<std::uint8_t>(changed_ids.size());

			for (std::uint8_t aircraft_identifier : changed_ids)
			{
				int localSlot = -1;
				for (const auto& kv : m_local_player_to_aircraft_id)
				{
					if (kv.second == aircraft_identifier)
					{
						localSlot = kv.first;
						break;
					}
				}
				if (localSlot < 0)
					continue;

				Aircraft* a = m_world.GetPlayerAircraft(localSlot);
				if (!a)
					continue;

				const sf::Vector2f pos = a->getPosition();
				const sf::Vector2f vel = a->GetVelocity();
				const std::uint8_t hp = static_cast<std::uint8_t>(std::max(0, std::min(255, a->GetHitPoints())));
				const std::uint8_t ammo = 0;

				const std::uint8_t anim = m_world.GetLocalPlayerAnimState(localSlot);
				p << aircraft_identifier
					<< pos.x << pos.y
					<< vel.x << vel.y
					<< hp << ammo << anim;

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
		}

		if (force_send)
			m_state_force_send_timer = sf::Time::Zero;
	}

	sf::Clock interp_timer;
	for (auto& kv : m_remote_interp)
	{
		const std::uint8_t id = kv.first;
		auto& st = kv.second;

		if (!st.initialized)
			continue;

		//Advance the clock since the last received snapshot
		st.time_since_snap += dt;

		//Predict where the remote player is RIGHT NOW using their last known velocity.
		//Cap extrapolation at 150ms so a network stall doesn't send the ghost flying.
		constexpr float kMaxExtrapolation = 0.05f;
		const float t = std::min(st.time_since_snap.asSeconds(), kMaxExtrapolation);
		const sf::Vector2f deadReckoned = st.target + st.velocity * t;

		const float blend = std::min(1.f, dt.asSeconds() * 20.f);
		st.current += (deadReckoned - st.current) * blend;

		m_world.UpdateNetworkActorState(id, st.current, st.hp, st.ammo, st.anim);
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

	//Flush any new projectiles the local player fired this frame to the server
	if (GetContext().network && GetContext().network->IsActive())
	{
		sf::Vector2f projPos, projVel;
		std::uint8_t ownerId = 0;
		while (m_world.PollFiredProjectile(ownerId, projPos, projVel))
		{
			if (GetContext().network->IsClient())
			{
				sf::Packet p;
				p << static_cast<std::uint8_t>(Client::PacketType::kFireProjectile)
				  << ownerId << projPos.x << projPos.y << projVel.x << projVel.y;
				GetContext().network->SendGameplayPacket(p);
			}
			else if (GetContext().network->IsHosting())
			{
				if (auto* srv = GetContext().network->GetServer())
					srv->BroadcastProjectileSpawn(ownerId, projPos.x, projPos.y, projVel.x, projVel.y);
			}
			std::cout << "[MP] Fire broadcast owner=" << (int)ownerId
				<< " pos=(" << projPos.x << "," << projPos.y << ")\n";
		}
	}

	if (GetContext().network && GetContext().network->IsHosting())
	{
		std::vector<int> updatedScores;
		if (m_world.PollScoresChanged(updatedScores))
		{
			if (auto* srv = GetContext().network->GetServer())
			{
				srv->BroadcastScores(updatedScores);
				std::cout << "[MP] BroadcastScores: ";
				for (int i = 0; i < static_cast<int>(updatedScores.size()); ++i)
					std::cout << "P" << i << "=" << updatedScores[i] << " ";
				std::cout << "\n";
			}
		}

		uint8_t levelIndex = 0;
		if (m_world.PollNewRoundBroadcast(levelIndex))
		{
			if (auto* srv = GetContext().network->GetServer())
				srv->BroadcastNewRound(levelIndex);

			std::cout << "[MP] BroadcastNewRound level=" << (int)levelIndex << "\n";
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

void MultiplayerGameState::PollNetworkGameplay()
{
	if (!GetContext().network)
		return;

	//Host has no client socket, but still needs synthesized packets from NetworkSession
	if (!GetContext().network->IsHosting() && !GetContext().network->IsClientConnected())
		return;

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
		if (!(packet >> aircraftId >> x >> y)) return;

		if (!IsKnownLocalNetworkId(aircraftId))
		{
			//It's a genuine remote — register and spawn
			OnRemotePlayerConnected(aircraftId, x, y);
		}
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

		m_local_player_to_aircraft_id[0] = aircraftId;
		m_net_to_local_player_index[aircraftId] = 0;
		m_world.SetLocalNetworkId(static_cast<int>(aircraftId));

		std::cout << "[MP] kSpawnSelf: local aircraft id=" << static_cast<int>(aircraftId)
			<< " server pos=(" << x << ", " << y << ")\n";

		const sf::Color myColor = NetworkSlotColor(aircraftId);
		Aircraft* a = m_world.GetPlayerAircraft(0);
		if (a) a->SetPlayerColor(myColor);

		if (GetContext().network)
		{
			//Read the name for this local slot
			std::string myName = PlayerNameReader::GetName(0);
			std::cout << "[DEBUG] My ID is " << (int)aircraftId << ", sending name: " << myName << "\n";

			//Wrap it in a packet
			sf::Packet namePacket;
			namePacket << static_cast<std::uint8_t>(Client::PacketType::kPlayerNameSync);
			namePacket << static_cast<std::int32_t>(aircraftId);
			namePacket << myName;

			//Send to server so it can tell everyone else
			GetContext().network->SendGameplayPacket(namePacket);
		}

		if (GetContext().network && GetContext().network->IsClient())
		{
			if (a)
			{
				const sf::Vector2f pos = a->getPosition();
				const sf::Vector2f vel = a->GetVelocity();
				const std::uint8_t hp = static_cast<std::uint8_t>(
					std::max(0, std::min(255, a->GetHitPoints())));
				const std::uint8_t anim = m_world.GetLocalPlayerAnimState(0);

				sf::Packet p;
				p << static_cast<std::uint8_t>(Client::PacketType::kStateUpdate);
				p << static_cast<std::uint8_t>(1);
				p << aircraftId << pos.x << pos.y << vel.x << vel.y
					<< hp << static_cast<std::uint8_t>(0) << anim;
				GetContext().network->SendGameplayPacket(p);

				std::string name = GetContext().network->GetLocalPlayerName();
				if (!name.empty())
					GetContext().network->SendPlayerNameSync(
						static_cast<std::int32_t>(aircraftId), name);

			}
		}
	}
	break;

	case Server::PacketType::kNewRound:
	{
		uint8_t levelIndex = 0;
		if (!(packet >> levelIndex))
			return;

		std::cout << "[MP] kNewRound received: level=" << (int)levelIndex << "\n";
		m_world.StartNewRoundWithLevel(levelIndex);
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
			if (!(packet >> s.id >> s.x >> s.y >> s.vx >> s.vy >> s.hp >> s.ammo >> s.anim))
				return;
			m_latest_snapshot.push_back(s);

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
	case Server::PacketType::kScoreUpdate:
	{
		std::uint8_t count = 0;
		if (!(packet >> count))
			return;

		std::vector<int> scores(count);
		for (std::uint8_t i = 0; i < count; ++i)
		{
			std::int32_t s = 0;
			if (!(packet >> s))
				return;
			scores[i] = static_cast<int>(s);
		}

		m_world.ApplyNetworkScores(scores);
		std::cout << "[MP] kScoreUpdate received: ";
		for (int i = 0; i < static_cast<int>(scores.size()); ++i)
			std::cout << "P" << i << "=" << scores[i] << " ";
		std::cout << "\n";
	}
	break;
	case Server::PacketType::kPlayerNameSync:
	{
		std::uint8_t id = 0;
		std::string name;
		if (!(packet >> id >> name)) return;

		//Check if this is our own local aircraft
		auto localIt = m_net_to_local_player_index.find(id);
		if (localIt != m_net_to_local_player_index.end())
		{
			Aircraft* a = m_world.GetPlayerAircraft(localIt->second);
			if (a) a->SetPlayerName(name);
		}
		else
		{
			//It's a remote actor
			m_world.SetNetworkActorName(id, name);
		}

		std::cout << "[MP] kPlayerNameSync id=" << (int)id << " name=" << name << "\n";
	}
	break;
	case Server::PacketType::kPlayerColorSync:
	break;
	case Server::PacketType::kSpawnProjectile:
	{
		std::uint8_t ownerId = 0;
		float x = 0.f, y = 0.f, vx = 0.f, vy = 0.f;
		if (!(packet >> ownerId >> x >> y >> vx >> vy))
			return;

		if (IsKnownLocalNetworkId(ownerId))
			return;

		std::cout << "[MP] kSpawnProjectile owner=" << (int)ownerId
			<< " pos=(" << x << "," << y << ") vel=(" << vx << "," << vy << ")\n";

		m_world.SpawnNetworkProjectile(ownerId, { x, y }, { vx, vy });
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

	const sf::Color tint = NetworkSlotColor(networkId);
	m_world.SpawnNetworkActor(networkId, { x, y }, tint);

	std::cout << "[MP] Remote player connected: id=" << static_cast<int>(networkId)
		<< " pos=(" << x << ", " << y << ")"
		<< " tint=(" << (int)tint.r << "," << (int)tint.g << "," << (int)tint.b << ")\n";
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