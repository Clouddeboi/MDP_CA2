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
	{
		m_client_socket->disconnect();
	}
	m_client_socket.reset();
	m_client_connected = false;

	m_mode = NetworkMode::kNone;
	m_last_error.clear();

	m_pending_remote_binding_events.clear();
	m_pending_start_game = false;
	m_pending_player_left_events.clear();
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
			m_pending_remote_binding_events.emplace_back(playerIndex, color, ready);
		}

		if (m_server->PollClientStartRequest())
		{
			m_pending_start_game = true;
		}

		while (m_server->PollClientLeave(playerIndex))
		{
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
				std::uint8_t type = 0;
				p >> type;

				const auto packetType = static_cast<Server::PacketType>(type);

				if (packetType == Server::PacketType::kLobbyBindingState)
				{
					std::uint8_t playerIdx = 0;
					std::int32_t color = -1;
					bool ready = false;
					p >> playerIdx >> color >> ready;

					m_pending_remote_binding_events.emplace_back(
						static_cast<int>(playerIdx),
						static_cast<int>(color),
						ready
					);
				}
				else if (packetType == Server::PacketType::kLobbyStartGame)
				{
					m_pending_start_game = true;
				}
				else if (packetType == Server::PacketType::kLobbyPlayerLeft)
				{
					std::uint8_t leftIdx = 0;
					p >> leftIdx;
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

						if (connected)
						{
							m_pending_remote_binding_events.emplace_back(
								static_cast<int>(playerIdx),
								static_cast<int>(color),
								ready
							);
						}
						else
						{
							m_pending_player_left_events.push_back(static_cast<int>(playerIdx));
						}
					}
				}

				continue;
			}

			if (status == sf::Socket::Status::Disconnected)
			{
				//Host went away
				m_client_connected = false;
				m_pending_player_left_events.push_back(0);
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