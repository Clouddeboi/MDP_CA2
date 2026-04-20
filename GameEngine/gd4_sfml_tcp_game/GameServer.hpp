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

/*
 * Code implementation assisted by GitHub Copilot
 * Used for:
 * - Dedicated server execution thread architecture (ExecutionThread)
 * - Client connection handshake sequence (HandleIncomingConnections)
 * - Incoming packet dispatch and routing (HandleIncomingPackets)
 * - 30Hz state snapshot broadcasting (UpdateClientState)
 * - Thread-safe HostEvent queue for main thread communication
 * - Disconnection detection and peer cleanup (HandleDisconnections)
 * - BroadcastNewRound, BroadcastScores, BroadcastProjectileSpawn implementations
 * Original implementation, modified/adapted by Michal Becmer (D00256088) for project requirements
 */

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

	void UpdateHostAircraftState(const sf::Vector2f& pos, uint8_t hp, uint8_t ammo, uint8_t anim = 0);
	void SetAircraftColor(uint8_t id, uint8_t r, uint8_t g, uint8_t b);
	void BroadcastAllColors();
	void BroadcastAllNames();

	void BroadcastProjectileSpawn(uint8_t ownerId, float x, float y, float vx, float vy);
	void BroadcastNewRound(uint8_t levelIndex);

	struct NetAircraftState
	{
		std::uint8_t id;
		sf::Vector2f position;
		sf::Vector2f velocity{ 0.f, 0.f };
		std::uint8_t hp;
		std::uint8_t ammo;
		std::uint8_t anim = 0;
	};

	void CopyAircraftStates(std::vector<NetAircraftState>& outStates) const;

	struct HostEvent
	{
		enum Type { kConnect, kDisconnect, kColorSync, kSpawnProjectile, kNameSync };
		Type type;
		uint8_t aircraft_id;
		float x = 0.f, y = 0.f;
		float vx = 0.f, vy = 0.f;
		uint8_t r = 255, g = 255, b = 255;
		std::string name;
	};

	void UpdateHostAircraftState(const sf::Vector2f& pos, const sf::Vector2f& vel, uint8_t hp, uint8_t ammo, uint8_t anim = 0);
	bool PollHostEvent(HostEvent& outEvent);
	void PushHostEvent(const HostEvent& event);

	void BroadcastScores(const std::vector<int>& scores);

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
		std::string m_player_name = "Player";
		uint8_t m_missile_ammo;
		std::map<uint8_t, bool> m_real_time_actions;

		uint8_t m_color_r = 255;
		uint8_t m_color_g = 255;
		uint8_t m_color_b = 255;

		uint8_t m_anim = 0;
		sf::Vector2f m_velocity = { 0.f, 0.f };
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

	std::mutex m_host_event_mutex;
	std::deque<HostEvent> m_host_events;

	mutable std::mutex m_aircraft_mutex;

	bool m_game_started = false;

};
