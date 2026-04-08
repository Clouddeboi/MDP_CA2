#include "NetworkSession.hpp"
#include "GameServer.hpp"

NetworkSession::NetworkSession()
    : m_mode(NetworkMode::kNone)
    , m_server(nullptr)
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
        return true;
    }
    catch (...)
    {
        m_server.reset();
        m_mode = NetworkMode::kNone;
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

void NetworkSession::StartClientMode()
{
    Reset();
    m_mode = NetworkMode::kClient;
}

void NetworkSession::Reset()
{
    m_server.reset();
    m_mode = NetworkMode::kNone;
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

GameServer* NetworkSession::GetServer()
{
    return m_server.get();
}

const GameServer* NetworkSession::GetServer() const
{
    return m_server.get();
}