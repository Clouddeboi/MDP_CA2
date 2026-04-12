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
		1  // Only 1 local aircraft per machine; remotes are network actors
	)
	, m_players()
	, m_sounds(*context.sounds)
{
	context.music->Play(MusicThemes::kMissionTheme);

	auto& config = PlayerBindingConfig::GetInstance();

	// Every machine controls exactly 1 local player (always local slot 0)
	m_players.emplace_back(0);

	auto device = config.GetPlayerDevice(0);
	if (device.has_value() && device->type == InputDeviceType::kController)
		m_players[0].SetJoystickId(device->deviceIndex);
	else
		m_players[0].SetJoystickId(-1);

	const bool isHost = (GetContext().network && GetContext().network->IsHosting());

	// Host self-assigns aircraft ID 0
	if (isHost)
	{
		m_local_player_to_aircraft_id[0] = 0;
		m_net_to_local_player_index[0] = 0;
	}

	m_world.SetCollisionEnabled(true);

	if (isHost)
	{
		m_world.SetLocalNetworkId(0);

		auto& config = PlayerBindingConfig::GetInstance();
		auto hostColor = config.GetPlayerColor(0);
		if (hostColor.has_value() && GetContext().network->GetServer())
		{
			auto* srv = GetContext().network->GetServer();
			srv->SetAircraftColor(0, hostColor->r, hostColor->g, hostColor->b);

			// Push HostEvent so PollGameplayPacket applies color to host's own actor
			GameServer::HostEvent ev;
			ev.type = GameServer::HostEvent::kColorSync;
			ev.aircraft_id = 0;
			ev.r = hostColor->r;
			ev.g = hostColor->g;
			ev.b = hostColor->b;
			srv->PushHostEvent(ev);
			// NOTE: BroadcastAllColors is intentionally NOT called here —
			// no peers exist yet. It is called from HandleIncomingConnections
			// after m_connected_players is incremented.
		}
	}
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

	// --- Apply snapshots: only update REMOTE actors, never overwrite local ---
	if (m_has_new_snapshot)
	{
		sf::Clock snapshot_timer;

		for (const auto& s : m_latest_snapshot)
		{
			// Skip our own aircraft — local physics is authoritative
			if (IsKnownLocalNetworkId(s.id))
				continue;

			// Only update known remote actors
			if (m_known_remote_network_ids.find(s.id) == m_known_remote_network_ids.end())
				continue;

			auto& interp = m_remote_interp[s.id];
			//Always refresh velocity and anim — these drive dead-reckoning and animation
			interp.target    = { s.x, s.y };
			interp.velocity  = { s.vx, s.vy };
			interp.hp        = s.hp;
			interp.ammo      = s.ammo;
			interp.anim      = s.anim;
			if (!interp.initialized)
			{
				interp.current     = { s.x, s.y };
				interp.initialized = true;
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

	// --- State updates: BOTH host and client send position to server ---
	m_state_send_timer += dt;
	m_state_force_send_timer += dt;

	if (GetContext().network && GetContext().network->IsActive() && m_state_send_timer >= m_state_send_interval)
	{
		m_state_send_timer = sf::Time::Zero;
		const bool force_send = m_state_force_send_timer >= m_state_force_send_interval;

		// HOST path: update server directly (no TCP socket)
		if (GetContext().network->IsHosting())
		{
			Aircraft* a = m_world.GetPlayerAircraft(0);
			if (a)
			{
				const sf::Vector2f pos = a->getPosition();
				const sf::Vector2f vel = a->GetVelocity();
				const uint8_t hp = static_cast<uint8_t>(std::max(0, std::min(255, a->GetHitPoints())));
				const uint8_t anim = m_world.GetLocalPlayerAnimState(0);

				auto* server = GetContext().network->GetServer();
				if (server)
					server->UpdateHostAircraftState(pos, vel, hp, 0, anim);
			}
		}
		// CLIENT path: send via TCP (existing logic)
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
				const std::uint8_t hp = static_cast<std::uint8_t>(std::max(0, std::min(255, a->GetHitPoints())));
				const std::uint8_t ammo = 0;

				const sf::Vector2f vel = a->GetVelocity();
				const std::uint8_t anim = m_world.GetLocalPlayerAnimState(localSlot);
				p << aircraft_identifier << pos.x << pos.y << vel.x << vel.y << hp << ammo << anim;

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

	// Smooth remote actor movement
	sf::Clock interp_timer;
	for (auto& kv : m_remote_interp)
	{
		const std::uint8_t id = kv.first;
		auto& st = kv.second;
		if (!st.initialized) continue;

		// Dead-reckoning: advance by known velocity, then snap toward authoritative target
		st.current += st.velocity * dt.asSeconds();

		// Correct drift back toward the server-authoritative position
		const sf::Vector2f delta = st.target - st.current;
		const float dist = std::hypot(delta.x, delta.y);

		if (dist > 200.f)
		{
			// Too far — hard snap (teleport / respawn)
			st.current = st.target;
		}
		else if (dist > 2.f)
		{
			// Blend toward target at a rate proportional to distance
			// (faster correction when further away)
			const float correctionRate = std::min(1.f, dt.asSeconds() * 15.f);
			st.current += delta * correctionRate;
		}

		// Derive animation from velocity when moving; fall back to last known facing when idle
		const float vx = st.velocity.x;
		const bool isRunning = std::abs(vx) > 10.f;
		bool facingRight;
		if (std::abs(vx) > 1.f)
			facingRight = (vx > 0.f);          // moving: direction from velocity
		else
			facingRight = (st.anim & 1u) != 0; // idle: preserve last snapshotted facing
		const std::uint8_t animFlags = static_cast<std::uint8_t>(
			(facingRight ? 1u : 0u) | (isRunning ? 2u : 0u));

		m_world.UpdateNetworkActorState(id, st.current, st.hp, st.ammo, animFlags);
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

	// Flush any new projectiles the local player fired this frame to the server
	if (GetContext().network && GetContext().network->IsActive())
	{
		sf::Vector2f projPos, projVel;
		std::uint8_t ownerId = 0;
		// GetPendingFiredProjectiles drains a queue filled by World
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
				// Host: broadcast directly
				if (auto* srv = GetContext().network->GetServer())
					srv->BroadcastProjectileSpawn(ownerId, projPos.x, projPos.y, projVel.x, projVel.y);
			}
			std::cout << "[MP] Fire broadcast owner=" << (int)ownerId
				<< " pos=(" << projPos.x << "," << projPos.y << ")\n";
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

		m_local_player_to_aircraft_id[0] = aircraftId;
		m_net_to_local_player_index[aircraftId] = 0;
		m_world.SetLocalNetworkId(static_cast<int>(aircraftId));

		std::cout << "[MP] kSpawnSelf: local aircraft id=" << static_cast<int>(aircraftId)
			<< " server pos=(" << x << ", " << y << ")\n";

		// Re-register our lobby color under our actual network ID.
		// PlayerBindingConfig[0] held our color during lobby; now store it at
		// aircraftId so kPlayerColorSync packets from others don't overwrite us.
		auto& cfg = PlayerBindingConfig::GetInstance();
		auto myColor = cfg.GetPlayerColor(0);
		if (myColor.has_value() && aircraftId != 0)
			cfg.SetPlayerColor(static_cast<int>(aircraftId), myColor.value());

		// Re-tint the ALREADY-BUILT local aircraft with its correct color
		if (myColor.has_value())
		{
			Aircraft* a = m_world.GetPlayerAircraft(0);
			if (a)
			{
				a->SetPlayerColor(myColor.value());
				std::cout << "[MP] Applied local color to aircraft: id=" << (int)aircraftId
					<< " rgb=(" << (int)myColor->r << "," << (int)myColor->g << "," << (int)myColor->b << ")\n";
			}
		}

		// Immediately push corrected spawn position to server
		if (GetContext().network && GetContext().network->IsClient())
		{
			Aircraft* a = m_world.GetPlayerAircraft(0);
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
				p << aircraftId << pos.x << pos.y << vel.x << vel.y << hp
					<< static_cast<std::uint8_t>(0) << anim;
				GetContext().network->SendGameplayPacket(p);
			}
		}

		// Send our color to the server so it can replicate to the host
		if (myColor.has_value() && GetContext().network && GetContext().network->IsClient())
		{
			sf::Packet cp;
			cp << static_cast<std::uint8_t>(Client::PacketType::kPlayerColorSync)
				<< aircraftId
				<< myColor->r
				<< myColor->g
				<< myColor->b;
			GetContext().network->SendGameplayPacket(cp);
			std::cout << "[MP] Sent kPlayerColorSync id=" << (int)aircraftId
				<< " rgb=(" << (int)myColor->r << "," << (int)myColor->g << "," << (int)myColor->b << ")\n";
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
			if (!(packet >> s.id >> s.x >> s.y >> s.vx >> s.vy >> s.hp >> s.ammo >> s.anim))
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
	case Server::PacketType::kPlayerColorSync:
	{
		std::uint8_t id = 0, r = 0, g = 0, b = 0;
		if (!(packet >> id >> r >> g >> b))
			return;

		const sf::Color color(r, g, b);

		std::cout << "[MP] kPlayerColorSync id=" << (int)id
			<< " rgb=(" << (int)r << "," << (int)g << "," << (int)b << ")\n";

		// Store in global config keyed by network ID
		PlayerBindingConfig::GetInstance().SetPlayerColor(static_cast<int>(id), color);

		// Apply to remote network actor if it exists
		m_world.SetNetworkActorColor(id, color);

		// If this is our own ID (host receiving its own sync-back, or any echo),
		// also apply to local physics player so colors are consistent
		auto it = m_local_player_to_aircraft_id.begin();
		for (; it != m_local_player_to_aircraft_id.end(); ++it)
		{
			if (it->second == id)
			{
				Aircraft* a = m_world.GetPlayerAircraft(it->first);
				if (a) a->SetPlayerColor(color);
				break;
			}
		}
	}
	break;
	case Server::PacketType::kSpawnProjectile:
	{
		std::uint8_t ownerId = 0;
		float x = 0.f, y = 0.f, vx = 0.f, vy = 0.f;
		if (!(packet >> ownerId >> x >> y >> vx >> vy))
			return;

		// Don't spawn a ghost bullet for our own shots (we already spawned it locally)
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

	// Spawn with white — kPlayerColorSync arrives immediately after and applies
	// the real color. Using a stale config value here causes the wrong color to
	// briefly (or permanently) show when the sync packet hasn't arrived yet.
	sf::Color tint = sf::Color::White;

	// Use config color only if it's already been stored under the correct network ID
	// (i.e. a prior kPlayerColorSync already arrived for this ID)
	auto remoteColor = PlayerBindingConfig::GetInstance().GetPlayerColor(
		static_cast<int>(networkId));
	if (remoteColor.has_value())
		tint = remoteColor.value();

	m_world.SpawnNetworkActor(networkId, { x, y }, tint);

	std::cout << "[MP] Remote player connected: id=" << static_cast<int>(networkId)
		<< " pos=(" << x << ", " << y << ")"
		<< " tint=(" << (int)tint.r << "," << (int)tint.g << "," << (int)tint.b
		<< ") config_hit=" << remoteColor.has_value() << "\n";
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