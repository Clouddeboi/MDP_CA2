#pragma once
#include "State.hpp"
#include "World.hpp"
#include "Player.hpp"
#include "SoundPlayer.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <SFML/Network/Packet.hpp>

class MultiplayerGameState : public State
{
public:
	MultiplayerGameState(StateStack& stack, Context context);
	virtual void Draw() override;
	virtual bool Update(sf::Time dt) override;
	virtual bool HandleEvent(const sf::Event& event) override;

private:
	void PollNetworkGameplay();
	void HandleServerPacket(sf::Packet& packet);
	void RebuildNetworkPlayerMap();
	void OnRemotePlayerConnected(std::uint8_t networkId, float x, float y);
	void OnRemotePlayerDisconnected(std::uint8_t networkId);
	bool IsKnownLocalNetworkId(std::uint8_t networkId) const;

	struct NetActorState
	{
		std::uint8_t id = 0;
		float x = 0.f;
		float y = 0.f;
		float vx = 0.f;
		float vy = 0.f;
		std::uint8_t hp = 0;
		std::uint8_t ammo = 0;
		// Animation: bit 0 = facing right, bit 1 = is running
		std::uint8_t anim = 0;
	};

	struct RemoteInterpState
	{
		sf::Vector2f current{ 0.f, 0.f };
		sf::Vector2f target{ 0.f, 0.f };
		sf::Vector2f velocity{ 0.f, 0.f };
		sf::Time     time_since_snap = sf::Time::Zero;
		std::uint8_t hp = 0;
		std::uint8_t ammo = 0;
		std::uint8_t anim = 0;
		bool initialized = false;
	};

	struct LastSentLocalState
	{
		sf::Vector2f position{ 0.f, 0.f };
		std::uint8_t hp = 0;
		std::uint8_t ammo = 0;
		bool initialized = false;
	};

	struct PerfCounters
	{
		sf::Time sample_window = sf::Time::Zero;
		sf::Time update_time_total = sf::Time::Zero;
		sf::Time net_poll_time_total = sf::Time::Zero;
		sf::Time world_update_time_total = sf::Time::Zero;
		sf::Time snapshot_apply_time_total = sf::Time::Zero;
		sf::Time interp_time_total = sf::Time::Zero;

		std::size_t frames = 0;
		std::size_t rx_packets = 0;
		std::size_t rx_bytes = 0;
		std::size_t tx_packets = 0;
		std::size_t tx_bytes = 0;
		std::size_t snapshot_packets = 0;
		std::size_t snapshot_actors = 0;
		std::size_t poll_cap_hits = 0;
		std::size_t remote_connects = 0;
		std::size_t remote_disconnects = 0;
	};

	float m_latest_world_scroll = 0.f;
	std::vector<NetActorState> m_latest_snapshot;
	bool m_has_new_snapshot = false;

	World m_world;
	std::vector<Player> m_players;
	SoundPlayer& m_sounds;

	std::unordered_map<std::uint8_t, int> m_net_to_local_player_index;
	std::unordered_set<std::uint8_t> m_known_remote_network_ids;
	std::unordered_map<int, std::uint8_t> m_local_player_to_aircraft_id;

	sf::Time m_state_send_timer = sf::Time::Zero;
	sf::Time m_state_send_interval = sf::milliseconds(100);//10Hz
	sf::Time m_state_force_send_timer = sf::Time::Zero;
	sf::Time m_state_force_send_interval = sf::milliseconds(500);

	PerfCounters m_perf;

	std::unordered_map<std::uint8_t, LastSentLocalState> m_last_sent_local_states;

	std::unordered_map<std::uint8_t, RemoteInterpState> m_remote_interp;
};