#pragma once
#include "State.hpp"
#include "World.hpp"
#include "Player.hpp"
#include "SoundPlayer.hpp"
#include <vector>

class GameState : public State
{
public:
	GameState(StateStack& stack, Context context);
	virtual void Draw() override;
	virtual bool Update(sf::Time dt) override;
	virtual bool HandleEvent(const sf::Event& event) override;

private:
	World m_world;
	std::vector<Player> m_players;
	SoundPlayer& m_sounds;
};

