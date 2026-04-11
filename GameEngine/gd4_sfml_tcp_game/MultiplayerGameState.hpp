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
		std::uint8_t hp = 0;
		std::uint8_t ammo = 0;
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
};