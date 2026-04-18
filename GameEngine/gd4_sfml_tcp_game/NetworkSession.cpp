#include "NetworkSession.hpp"
#include "GameServer.hpp"
#include "NetworkProtocol.hpp"

#include <SFML/Network/TcpSocket.hpp>
#include <SFML/Network/IpAddress.hpp>
#include <SFML/Network/Packet.hpp>
#include <SFML/System/Sleep.hpp>

NetworkSession::NetworkSession()
	: m_mode(NetworkMode::kNone)
	, m_server(nullptr)
	, m_client_socket(nullptr)
	, m_client_connected(false)
	, m_last_error()

{
	m_lobby_ready_state.fill(false);
	m_lobby_color_state.fill(-1);
	m_lobby_connected.fill(false);
}

NetworkSession::~NetworkSession()
{
	Reset();
}

bool NetworkSession::StartHosting(const sf::Vector2f& battlefieldSize)
{
	Reset();

	try
	{
		m_server = std::make_unique<GameServer>(battlefieldSize);
		m_mode = NetworkMode::kHost;
		m_last_error.clear();
		return true;
	}
	catch (...)
	{
		m_server.reset();
		m_mode = NetworkMode::kNone;
		SetError("Failed to start host server.");
		return false;
	}
}

void NetworkSession::StopHosting()
{
	if (m_mode == NetworkMode::kHost)
	{
		m_server.reset();
		m_mode = NetworkMode::kNone;
	}
}

bool NetworkSession::HasHostClientConnected() const
{
	return m_mode == NetworkMode::kHost
		&& m_server
		&& m_server->GetConnectedPlayerCount() > 0;
}

void NetworkSession::StartClientMode()
{
	Reset();
	m_mode = NetworkMode::kClient;
	m_client_socket = std::make_unique<sf::TcpSocket>();
	m_client_connected = false;
	m_last_error.clear();
}

bool NetworkSession::ConnectToHost(const std::string& hostOrIp, sf::Time timeout)
{
	if (hostOrIp.empty())
	{
		SetError("IP/host is empty.");
		return false;
	}

	if (m_mode != NetworkMode::kClient || !m_client_socket)
	{
		StartClientMode();
	}

	auto resolved = sf::IpAddress::resolve(hostOrIp);
	if (!resolved.has_value())
	{
		SetError("Invalid IP/host.");
		return false;
	}

	m_client_socket->setBlocking(true);
	const auto status = m_client_socket->connect(*resolved, SERVER_PORT, timeout);

	if (status == sf::Socket::Status::Done)
	{
		m_client_connected = true;
		m_last_error.clear();
		m_client_socket->setBlocking(false);
		return true;
	}

	m_client_connected = false;
	m_client_socket->disconnect();

	switch (status)
	{
	case sf::Socket::Status::NotReady:     SetError("Connection not ready."); break;
	case sf::Socket::Status::Partial:      SetError("Connection partial."); break;
	case sf::Socket::Status::Disconnected: SetError("Disconnected while connecting."); break;
	case sf::Socket::Status::Error:
	default:                               SetError("Connection failed."); break;
	}

	return false;
}

void NetworkSession::DisconnectClient()
{
	if (m_client_socket)
	{
		m_client_socket->disconnect();
	}
	m_client_connected = false;

	if (m_mode == NetworkMode::kClient)
	{
		m_mode = NetworkMode::kNone;
	}
}

bool NetworkSession::IsClientConnected() const
{
	return m_mode == NetworkMode::kClient && m_client_connected;
}

void NetworkSession::Reset()
{
	m_server.reset();

	if (m_client_socket)
		m_client_socket->disconnect();

	m_client_socket.reset();
	m_client_connected = false;
	m_mode = NetworkMode::kNone;
	m_last_error.clear();

	m_pending_remote_binding_events.clear();
	m_pending_start_game = false;
	m_pending_player_left_events.clear();
	m_pending_assigned_local_player_index = -1;
	m_has_pending_assigned_local_player_index = false;
	m_pending_gameplay_packets.clear();
}

bool NetworkSession::IsActive() const
{
	return m_mode != NetworkMode::kNone;
}

bool NetworkSession::IsHosting() const
{
	return m_mode == NetworkMode::kHost;
}

bool NetworkSession::IsClient() const
{
	return m_mode == NetworkMode::kClient;
}

NetworkMode NetworkSession::GetMode() const
{
	return m_mode;
}

const std::string& NetworkSession::GetLastError() const
{
	return m_last_error;
}

GameServer* NetworkSession::GetServer()
{
	return m_server.get();
}

const GameServer* NetworkSession::GetServer() const
{
	return m_server.get();
}

void NetworkSession::SetError(const std::string& message)
{
	m_last_error = message;
}

void NetworkSession::SendLobbyBindingState(int playerIndex, int colorIndex, bool ready)
{
	if (m_mode == NetworkMode::kHost && m_server)
	{
		m_server->BroadcastLobbyBindingState(static_cast<std::uint8_t>(playerIndex), colorIndex, ready);
		return;
	}

	if (m_mode == NetworkMode::kClient && m_client_socket && m_client_connected)
	{
		sf::Packet p;
		p << static_cast<std::uint8_t>(Client::PacketType::kLobbyBindingState);
		p << static_cast<std::uint8_t>(playerIndex);
		p << static_cast<std::int32_t>(colorIndex) << ready;

		m_client_socket->setBlocking(true);
		(void)m_client_socket->send(p);
		m_client_socket->setBlocking(false);
	}
}

void NetworkSession::SendLobbyStartRequest()
{
	if (m_mode == NetworkMode::kClient && m_client_socket && m_client_connected)
	{
		sf::Packet p;
		p << static_cast<std::uint8_t>(Client::PacketType::kLobbyStartGameRequest);

		m_client_socket->setBlocking(true);
		(void)m_client_socket->send(p);
		m_client_socket->setBlocking(false);
	}
}

void NetworkSession::SendLobbyStartGame()
{
	if (m_mode == NetworkMode::kHost && m_server)
	{
		m_server->BroadcastLobbyStartGame();
	}
}

void NetworkSession::SendLobbyLeave()
{
	SendLobbyLeave(1);
}

void NetworkSession::SendLobbyLeave(int playerIndex)
{
	//Client notifies host
	if (m_mode == NetworkMode::kClient && m_client_socket && m_client_connected)
	{
		sf::Packet p;
		p << static_cast<std::uint8_t>(Client::PacketType::kLobbyLeave);
		p << static_cast<std::uint8_t>(playerIndex);

		m_client_socket->setBlocking(true);
		(void)m_client_socket->send(p);
		m_client_socket->setBlocking(false);
		return;
	}

	//Host can also broadcast local leave if needed
	if (m_mode == NetworkMode::kHost && m_server)
	{
		m_server->BroadcastLobbyPlayerLeft(static_cast<std::uint8_t>(playerIndex));
	}
}

void NetworkSession::PollLobbyPackets()
{
	if (m_mode == NetworkMode::kHost && m_server)
	{
		int playerIndex = -1;
		int color = -1;
		bool ready = false;

		while (m_server->PollClientLobbyBindingState(playerIndex, color, ready))
		{
			bool wasAlreadyConnected = m_lobby_connected[playerIndex];

			m_lobby_ready_state[playerIndex] = ready;
			m_lobby_color_state[playerIndex] = color;

			if (!wasAlreadyConnected)
			{
				m_lobby_connected[playerIndex] = true;
				m_pending_player_joined_events.push_back(playerIndex);
			}

			m_pending_remote_binding_events.emplace_back(playerIndex, color, ready);
		}

		if (m_server->PollClientStartRequest())
			m_pending_start_game = true;

		while (m_server->PollClientLeave(playerIndex)) {
			m_lobby_connected[playerIndex] = false;
			m_pending_player_left_events.push_back(playerIndex);
		}

		return;
	}

	if (m_mode == NetworkMode::kClient && m_client_socket && m_client_connected)
	{
		m_client_socket->setBlocking(false);

		while (true)
		{
			sf::Packet p;
			const auto status = m_client_socket->receive(p);

			if (status == sf::Socket::Status::Done)
			{
				// Peek at the type byte without consuming it
				sf::Packet peek = p;
				std::uint8_t type = 0;
				peek >> type;

				const auto packetType = static_cast<Server::PacketType>(type);

				if (packetType == Server::PacketType::kLobbyBindingState)
				{
					std::uint8_t playerIdx = 0;
					std::int32_t color = -1;
					bool ready = false;
					peek >> playerIdx >> color >> ready;

					const int idx = static_cast<int>(playerIdx);
					m_lobby_ready_state[idx] = ready;
					m_lobby_color_state[idx] = static_cast<int>(color);
					m_lobby_connected[idx] = true;

					m_pending_remote_binding_events.emplace_back(idx, static_cast<int>(color), ready);
				}
				else if (packetType == Server::PacketType::kLobbyStartGame)
				{
					m_pending_start_game = true;
				}
				else if (packetType == Server::PacketType::kLobbyPlayerLeft)
				{
					std::uint8_t leftIdx = 0;
					peek >> leftIdx;
					m_pending_player_left_events.push_back(static_cast<int>(leftIdx));
				}
				else if (packetType == Server::PacketType::kLobbySnapshot)
				{
					std::uint8_t count = 0;
					p >> count;

					for (std::uint8_t i = 0; i < count; ++i)
					{
						std::uint8_t playerIdx = 0;
						std::int32_t color = -1;
						bool ready = false;
						bool connected = false;

						p >> playerIdx >> color >> ready >> connected;

						const int idx = static_cast<int>(playerIdx);

						if (connected)
						{
							bool wasAlreadyConnected = m_lobby_connected[idx];

							m_lobby_ready_state[idx] = ready;
							m_lobby_color_state[idx] = static_cast<int>(color);

							if (!wasAlreadyConnected)
							{
								m_pending_player_joined_events.push_back(idx);
							}

							if (connected && !m_lobby_was_connected[idx])
							{
								m_pending_player_joined_events.push_back(idx);
							}

							m_lobby_was_connected[idx] = connected;

							m_lobby_connected[idx] = true;
							m_pending_remote_binding_events.emplace_back(idx, static_cast<int>(color), ready);
						}
						else
						{
							m_lobby_connected[idx] = false;
							m_pending_player_left_events.push_back(idx);
						}
					}
				}
				else if (packetType == Server::PacketType::kLobbyAssignedIndex)
				{
					std::uint8_t assigned = 0;
					peek >> assigned;
					m_pending_assigned_local_player_index = static_cast<int>(assigned);
					m_has_pending_assigned_local_player_index = true;
				}
				else if (packetType == Server::PacketType::kPlayerConnect)
				{
					std::uint8_t playerId = 0;
					float x, y;

					peek >> playerId >> x >> y;

					m_pending_player_joined_events.push_back(static_cast<int>(playerId));
				}
				else
				{
					// Not a lobby packet — preserve it for PollGameplayPacket
					m_pending_gameplay_packets.push_back(p);
				}

				continue;
			}

			if (status == sf::Socket::Status::Disconnected)
			{
				m_client_connected = false;
				m_pending_player_left_events.push_back(0);

				for (int i = 0; i < 4; ++i)
					m_lobby_connected[i] = false;
			}

			break;
		}
	}
}

bool NetworkSession::ConsumeRemoteBindingState(int& playerIndex, int& colorIndex, bool& ready)
{
	if (m_pending_remote_binding_events.empty())
		return false;

	auto front = m_pending_remote_binding_events.front();
	m_pending_remote_binding_events.pop_front();

	playerIndex = std::get<0>(front);
	colorIndex = std::get<1>(front);
	ready = std::get<2>(front);
	return true;
}

bool NetworkSession::ConsumeStartGameSignal()
{
	const bool v = m_pending_start_game;
	m_pending_start_game = false;
	return v;
}

bool NetworkSession::ConsumeRemotePlayerLeft(int& playerIndex)
{
	if (m_pending_player_left_events.empty())
		return false;

	playerIndex = m_pending_player_left_events.front();
	m_pending_player_left_events.pop_front();
	return true;
}

void NetworkSession::HostBroadcastLobbyBindingState(int playerIndex, int colorIndex, bool ready)
{
	if (m_mode == NetworkMode::kHost && m_server)
	{
		m_server->BroadcastLobbyBindingState(
			static_cast<std::uint8_t>(playerIndex),
			colorIndex,
			ready
		);
	}
}

bool NetworkSession::ConsumeAssignedLocalPlayerIndex(int& playerIndex)
{
	if (!m_has_pending_assigned_local_player_index)
		return false;

	playerIndex = m_pending_assigned_local_player_index;
	m_has_pending_assigned_local_player_index = false;
	m_pending_assigned_local_player_index = -1;
	return true;
}

bool NetworkSession::PollGameplayPacket(sf::Packet& outPacket)
{
	// Client path: read from TCP socket
	if (m_mode == NetworkMode::kClient && m_client_socket && m_client_connected)
	{
		// Drain any packets that were buffered during the lobby phase first
		if (!m_pending_gameplay_packets.empty())
		{
			outPacket = std::move(m_pending_gameplay_packets.front());
			m_pending_gameplay_packets.pop_front();
			return true;
		}

		m_client_socket->setBlocking(false);
		const auto status = m_client_socket->receive(outPacket);

		if (status == sf::Socket::Status::Done)
			return true;

		if (status == sf::Socket::Status::Disconnected)
			m_client_connected = false;

		return false;
	}

	// Host path: first drain host events (connect/disconnect), then snapshot
	if (m_mode == NetworkMode::kHost && m_server)
	{
		GameServer::HostEvent event;
		if (m_server->PollHostEvent(event))
		{
			outPacket.clear();
			if (event.type == GameServer::HostEvent::kConnect)
			{
				outPacket << static_cast<std::uint8_t>(Server::PacketType::kPlayerConnect);
				outPacket << event.aircraft_id << event.x << event.y;
			}
			else if (event.type == GameServer::HostEvent::kColorSync)
			{
				outPacket << static_cast<std::uint8_t>(Server::PacketType::kPlayerColorSync);
				outPacket << event.aircraft_id << event.r << event.g << event.b;
			}
			else if (event.type == GameServer::HostEvent::kSpawnProjectile)
			{
				outPacket << static_cast<std::uint8_t>(Server::PacketType::kSpawnProjectile);
				outPacket << event.aircraft_id << event.x << event.y << event.vx << event.vy;
			}
			else
			{
				outPacket << static_cast<std::uint8_t>(Server::PacketType::kPlayerDisconnect);
				outPacket << event.aircraft_id;
			}
			return true;
		}

		// Rate-limit: only synthesize state snapshot at ~20 Hz
		if (m_host_snapshot_clock.getElapsedTime().asMilliseconds() < kHostSnapshotIntervalMs)
			return false;
		m_host_snapshot_clock.restart();

		std::vector<GameServer::NetAircraftState> states;
		m_server->CopyAircraftStates(states);

		if (states.empty())
			return false;

		outPacket.clear();
		outPacket << static_cast<std::uint8_t>(Server::PacketType::kUpdateClientState);
		outPacket << 0.f;
		outPacket << static_cast<std::uint8_t>(states.size());

		for (const auto& s : states)
			outPacket << s.id
			<< s.position.x << s.position.y
			<< s.velocity.x << s.velocity.y
			<< s.hp << s.ammo << s.anim;

		return true;
	}

	return false;
}

void NetworkSession::SendGameplayPacket(sf::Packet& packet)
{
	if (m_mode == NetworkMode::kClient && m_client_socket && m_client_connected)
	{
		m_client_socket->setBlocking(true);
		(void)m_client_socket->send(packet);
		m_client_socket->setBlocking(false);
		return;
	}
}

bool NetworkSession::ConsumeRemotePlayerJoined(int& playerIndex)
{
	if (m_pending_player_joined_events.empty())
		return false;

	playerIndex = m_pending_player_joined_events.front();
	m_pending_player_joined_events.pop_front();
	return true;
}