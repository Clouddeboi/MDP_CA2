#pragma once

#include <SFML/System/Vector2.hpp>
#include <memory>

class GameServer;

enum class NetworkMode
{
    kNone,
    kHost,
    kClient
};

class NetworkSession
{
public:
    NetworkSession();
    ~NetworkSession();

    NetworkSession(const NetworkSession&) = delete;
    NetworkSession& operator=(const NetworkSession&) = delete;

    bool StartHosting(const sf::Vector2f& battlefieldSize);
    void StopHosting();

    void StartClientMode(); //Placeholder
    void Reset();

    bool IsActive() const;
    bool IsHosting() const;
    bool IsClient() const;
    NetworkMode GetMode() const;

    GameServer* GetServer();
    const GameServer* GetServer() const;

private:
    NetworkMode m_mode;
    std::unique_ptr<GameServer> m_server;
};