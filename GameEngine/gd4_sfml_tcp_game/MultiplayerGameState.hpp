#pragma once
#include "State.hpp"
#include "World.hpp"
#include "Player.hpp"
#include "SoundPlayer.hpp"
#include <vector>
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

	World m_world;
	std::vector<Player> m_players;
	SoundPlayer& m_sounds;
};