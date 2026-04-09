#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/System/Time.hpp>
#include <memory>
#include <string>
#include <optional>
#include <tuple> 

namespace sf
{
    class TcpSocket;
}

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

    //Host
    bool StartHosting(const sf::Vector2f& battlefieldSize);
    void StopHosting();
    bool HasHostClientConnected() const;

    //Client
    void StartClientMode();
    bool ConnectToHost(const std::string& hostOrIp, sf::Time timeout);
    void DisconnectClient();
    bool IsClientConnected() const;

    //Shared
    void Reset();
    bool IsActive() const;
    bool IsHosting() const;
    bool IsClient() const;
    NetworkMode GetMode() const;

    //Lobby sync
    void SendLobbyBindingState(int colorIndex, bool ready);//host or client
    void SendLobbyStartRequest();//client -> host
    void SendLobbyStartGame();//host -> client(s)
    void SendLobbyLeave();
    bool ConsumeRemotePlayerLeft(int& playerIndex);

    void PollLobbyPackets();
    bool ConsumeRemoteBindingState(int& playerIndex, int& colorIndex, bool& ready);
    bool ConsumeStartGameSignal();

    void HostBroadcastLobbyBindingState(int playerIndex, int colorIndex, bool ready);

    const std::string& GetLastError() const;

    GameServer* GetServer();
    const GameServer* GetServer() const;

private:
    void SetError(const std::string& message);

private:
    NetworkMode m_mode;
    std::unique_ptr<GameServer> m_server;

    std::unique_ptr<sf::TcpSocket> m_client_socket;
    bool m_client_connected;
    std::string m_last_error;

    std::optional<std::tuple<int, int, bool>> m_pending_remote_binding;
    bool m_pending_start_game = false;
    std::optional<int> m_pending_player_left;
};