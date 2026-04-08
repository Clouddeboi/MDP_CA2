#include "NetworkSession.hpp"
#include "GameServer.hpp"
#include "NetworkProtocol.hpp"

#include <SFML/Network/TcpSocket.hpp>
#include <SFML/Network/IpAddress.hpp>

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
        return true;
    }

    m_client_connected = false;
    m_client_socket->disconnect();

    switch (status)
    {
    case sf::Socket::Status::NotReady:    SetError("Connection not ready."); break;
    case sf::Socket::Status::Partial:     SetError("Connection partial."); break;
    case sf::Socket::Status::Disconnected:SetError("Disconnected while connecting."); break;
    case sf::Socket::Status::Error:
    default:                              SetError("Connection failed."); break;
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

void NetworkSession::SendLobbyBindingState(int colorIndex, bool ready)
{
    if (m_mode == NetworkMode::kHost && m_server)
    {
        m_server->BroadcastLobbyBindingState(0, colorIndex, ready);//Host is player 0
        return;
    }

    if (m_mode == NetworkMode::kClient && m_client_socket && m_client_connected)
    {
        sf::Packet p;
        p << static_cast<uint8_t>(Client::PacketType::kLobbyBindingState);
        p << static_cast<int32_t>(colorIndex) << ready;
        (void)m_client_socket->send(p);
    }
}

void NetworkSession::SendLobbyStartRequest()
{
    if (m_mode == NetworkMode::kClient && m_client_socket && m_client_connected)
    {
        sf::Packet p;
        p << static_cast<uint8_t>(Client::PacketType::kLobbyStartGameRequest);
        (void)m_client_socket->send(p);
    }
}

void NetworkSession::SendLobbyStartGame()
{
    if (m_mode == NetworkMode::kHost && m_server)
    {
        m_server->BroadcastLobbyStartGame();
    }
}

void NetworkSession::PollLobbyPackets()
{
    if (m_mode == NetworkMode::kHost && m_server)
    {
        int color = -1; bool ready = false;
        if (m_server->PollClientLobbyBindingState(color, ready))
            m_pending_remote_binding = std::make_tuple(1, color, ready);//Client is player 1

        if (m_server->PollClientStartRequest())
            m_pending_start_game = true;
        return;
    }

    if (m_mode == NetworkMode::kClient && m_client_socket && m_client_connected)
    {
        m_client_socket->setBlocking(false);
        sf::Packet p;
        while (m_client_socket->receive(p) == sf::Socket::Status::Done)
        {
            uint8_t type = 0;
            p >> type;

            if (static_cast<Server::PacketType>(type) == Server::PacketType::kLobbyBindingState)
            {
                uint8_t playerIdx = 0; int32_t color = -1; bool ready = false;
                p >> playerIdx >> color >> ready;
                m_pending_remote_binding = std::make_tuple(static_cast<int>(playerIdx), static_cast<int>(color), ready);
            }
            else if (static_cast<Server::PacketType>(type) == Server::PacketType::kLobbyStartGame)
            {
                m_pending_start_game = true;
            }

            p.clear();
        }
    }
}

bool NetworkSession::ConsumeRemoteBindingState(int& playerIndex, int& colorIndex, bool& ready)
{
    if (!m_pending_remote_binding.has_value()) return false;
    std::tie(playerIndex, colorIndex, ready) = *m_pending_remote_binding;
    m_pending_remote_binding.reset();
    return true;
}

bool NetworkSession::ConsumeStartGameSignal()
{
    const bool v = m_pending_start_game;
    m_pending_start_game = false;
    return v;
}