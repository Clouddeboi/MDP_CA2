#pragma once
#include <SFML/System/Vector2.hpp>
#include <SFML/Network/TcpSocket.hpp>
#include <SFML/Network/TcpListener.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Network/Packet.hpp>
#include <thread>
#include <cstdint>
#include <map>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <deque>
#include <tuple>

class GameServer
{
public:
	explicit GameServer(sf::Vector2f battlefield_size);
	~GameServer();
	void NotifyPlayerSpawn(uint8_t aircraft_identifier);
	void NotifyPlayerRealtimeChange(uint8_t aircraft_identifier, uint8_t action, bool action_enabled);
	void NotifyPlayerEvent(uint8_t aircraft_identifier, uint8_t action);
	std::size_t GetConnectedPlayerCount() const;

	void BroadcastLobbyBindingState(uint8_t playerIndex, int colorIndex, bool ready);
	void BroadcastLobbyStartGame();

	bool PollClientLobbyBindingState(int& playerIndex, int& colorIndex, bool& ready);
	bool PollClientStartRequest();

	void BroadcastLobbyPlayerLeft(std::uint8_t playerIndex);
	bool PollClientLeave(int& playerIndex);

	void BroadcastLobbySnapshot();

private:
	struct RemotePeer
	{
		RemotePeer();
		sf::TcpSocket m_socket;
		sf::Time m_last_packet_time;
		std::vector<uint8_t> m_aircraft_identifiers;
		bool m_ready;
		bool m_timed_out;

		int m_lobby_player_index = -1;
	};

	struct AircraftInfo
	{
		sf::Vector2f m_position;
		uint8_t m_hitpoints;
		uint8_t m_missile_ammo;
		std::map<uint8_t, bool> m_real_time_actions;
	};

	typedef std::unique_ptr<RemotePeer> PeerPtr;

private:
	void SetListening(bool enable);
	void ExecutionThread();
	void Tick();
	sf::Time Now() const;

	void HandleIncomingPackets();
	void HandleIncomingPackets(sf::Packet& packet, RemotePeer& receiving_peer, bool& detected_timeout);

	void HandleIncomingConnections();
	void HandleDisconnections();

	void InformWorldState(sf::TcpSocket& socket);
	void BroadcastMessage(const std::string& message);
	void SendToAll(sf::Packet& packet);
	void UpdateClientState();

	int FindFreeLobbyPlayerIndex() const;
	void EnsureHostLobbyState();

private:
	std::thread m_thread;
	sf::Clock m_clock;
	sf::TcpListener m_listener_socket;
	bool m_listening_state;
	sf::Time m_client_timeout;

	std::size_t m_max_connected_players;
	std::size_t m_connected_players;

	float m_world_height;
	sf::FloatRect m_battlefield_rect;
	float m_battlefield_scrollspeed;

	std::size_t m_aircraft_count;
	std::map<uint8_t, AircraftInfo> m_aircraft_info;

	std::vector<PeerPtr> m_peers;
	uint8_t m_aircraft_identifier_counter;
	bool m_waiting_thread_end;

	sf::Time m_last_spawn_time;
	sf::Time m_time_for_next_spawn;

	std::mutex m_lobby_mutex;
	bool m_client_start_requested = false;

	std::deque<std::tuple<int, int, bool>> m_lobby_binding_events;//playerIndex, color, ready
	std::deque<int> m_lobby_leave_events;

	std::map<int, std::tuple<int, bool, bool>> m_lobby_state;
};
