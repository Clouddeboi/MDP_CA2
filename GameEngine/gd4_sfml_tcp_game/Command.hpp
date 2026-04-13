#pragma once
#include <functional>
#include <SFML/System/Time.hpp>
#include "ReceiverCategories.hpp"
#include <cassert>

class SceneNode;


struct Command
{
	Command();
	std::function<void(SceneNode&, sf::Time)> action;
	unsigned int category;
};

template<typename GameObject, typename Function>
std::function<void(SceneNode&, sf::Time)>
DerivedAction(Function fn)
{
	return [=](SceneNode& node, sf::Time dt)
		{
			if (auto* object = dynamic_cast<GameObject*>(&node))
			{
				fn(*object, dt);
			}
		};
}


