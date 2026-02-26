#include "BindingState.hpp"
#include "ResourceHolder.hpp"
#include "Utility.hpp"
#include "PlayerBindingConfig.hpp"
#include <SFML/Graphics/RenderWindow.hpp>
#include <iostream>

/*
 * Code implementation assisted by Claude Sonnet 4.5
 * Used for: "HandleEvent functionality"
 * Original implementation, modified/adapted by Michal Becmer (D00256088) for project requirements
 */

BindingState::BindingState(StateStack& stack, Context context)
	: State(stack, context)
	, m_background_sprite(context.textures->Get(TextureID::kTitleScreen))
	, m_device_detector()
	, m_joined_players()
	, m_elapsed_time(sf::Time::Zero)
{
	sf::RenderWindow& window = *GetContext().window;
	sf::Vector2f windowSize(window.getSize());

	// Title Text
	m_title_text.emplace(context.fonts->Get(Font::kMain), "PLAYER LOBBY");
	m_title_text->setCharacterSize(60);
	m_title_text->setFillColor(sf::Color::White);
	m_title_text->setOutlineColor(sf::Color::Black);
	m_title_text->setOutlineThickness(3.f);
	Utility::CentreOrigin(*m_title_text);
	m_title_text->setPosition({ windowSize.x / 2.f, 40.f });

	// Instructions Text
	m_instructions_text.emplace(context.fonts->Get(Font::kMain), "Press any input to join");
	m_instructions_text->setCharacterSize(28);
	m_instructions_text->setFillColor(sf::Color::Cyan);
	m_instructions_text->setOutlineColor(sf::Color::Black);
	m_instructions_text->setOutlineThickness(2.f);
	Utility::CentreOrigin(*m_instructions_text);
	m_instructions_text->setPosition({ windowSize.x / 2.f, 110.f });

	// Ready Text (initially hidden)
	m_ready_text.emplace(context.fonts->Get(Font::kMain), "");
	m_ready_text->setCharacterSize(24);
	m_ready_text->setFillColor(sf::Color::White);
	m_ready_text->setOutlineColor(sf::Color::Black);
	m_ready_text->setOutlineThickness(2.f);
	Utility::CentreOrigin(*m_ready_text);
	m_ready_text->setPosition({ windowSize.x / 2.f, windowSize.y - 60.f });

	// Setup grid layout for player slots
	const float slotWidth = 180.f;
	const float slotHeight = 140.f;
	const float gridSpacingX = 200.f;
	const float gridSpacingY = 160.f;
	const float gridStartX = (windowSize.x - (kGridColumns * gridSpacingX - 20.f)) / 2.f;
	const float gridStartY = 160.f;

	// Initialize player slots
	for (int i = 0; i < kMaxPlayers; ++i)
	{
		m_player_slots.emplace_back(*context.fonts, *context.textures);

		int row = i / kGridColumns;
		int col = i % kGridColumns;

		sf::Vector2f slotPos(
			gridStartX + (col * gridSpacingX),
			gridStartY + (row * gridSpacingY)
		);

		m_player_slots[i].SetPosition(slotPos);
		m_player_slots[i].SetSize({ slotWidth, slotHeight });
	}

	std::cout << "[BindingState] Grid-based player lobby initialized (max " << kMaxPlayers << " players)\n";
}

void BindingState::Draw()
{
	sf::RenderWindow& window = *GetContext().window;
	window.setView(window.getDefaultView());

	window.draw(m_background_sprite);

	if (m_title_text)
		window.draw(*m_title_text);

	if (m_instructions_text)
		window.draw(*m_instructions_text);

	for (int i = 0; i < kMaxPlayers; ++i)
	{
		window.draw(m_player_slots[i]);
	}

	//Show ready text when we have players
	if (GetJoinedPlayerCount() >= 2 && m_ready_text)
	{
		std::string hostDevice = "ENTER";
		if (!m_joined_players.empty() && m_joined_players[0].device.type == InputDeviceType::kController)
		{
			hostDevice = "A button";
		}

		std::string readyMsg = "Player 1 press " + hostDevice + " to start (" + std::to_string(GetJoinedPlayerCount()) + " players)";
		readyMsg += "\nKeyboard: Press key again to leave | Controller: B to leave";
		m_ready_text->setString(readyMsg);
		m_ready_text->setFillColor(sf::Color::Green);
		Utility::CentreOrigin(*m_ready_text);
		window.draw(*m_ready_text);
	}
	else if (GetJoinedPlayerCount() == 1 && m_ready_text)
	{
		m_ready_text->setString("Need at least 2 players to start\nKeyboard: Press key again to leave | Controller: B to leave");
		m_ready_text->setFillColor(sf::Color::Yellow);
		Utility::CentreOrigin(*m_ready_text);
		window.draw(*m_ready_text);
	}
}

bool BindingState::Update(sf::Time dt)
{
	m_elapsed_time += dt;
	
	//Update all player slot animations
	for (auto& slot : m_player_slots)
	{
		slot.Update(dt);
	}
	
	//Update instructions text based on player count
	if (m_instructions_text)
	{
		if (GetJoinedPlayerCount() >= kMaxPlayers)
		{
			m_instructions_text->setString("Maximum players reached!");
			m_instructions_text->setFillColor(sf::Color::Red);
		}
		else if (GetJoinedPlayerCount() > 0)
		{
			std::string msg = "Press any input to join (" + std::to_string(GetJoinedPlayerCount()) + "/" + std::to_string(kMaxPlayers) + ")";
			m_instructions_text->setString(msg);
			m_instructions_text->setFillColor(sf::Color::Cyan);
		}
		else
		{
			m_instructions_text->setString("Press any input to join");
			m_instructions_text->setFillColor(sf::Color::Cyan);
		}
		Utility::CentreOrigin(*m_instructions_text);
	}
	
	return true;
}

bool BindingState::HandleEvent(const sf::Event& event)
{
	//Only Player 1 (host) can start the game with their device
	if (GetJoinedPlayerCount() >= 2)
	{
		if (!m_joined_players.empty())
		{
			const auto& player1Device = m_joined_players[0].device;
			bool isPlayer1Input = false;

			if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>())
			{
				if (player1Device.type == InputDeviceType::kKeyboardMouse &&
					keyPressed->code == sf::Keyboard::Key::Enter)
				{
					isPlayer1Input = true;
				}
			}

			if (const auto* joyButtonPressed = event.getIf<sf::Event::JoystickButtonPressed>())
			{
				if (player1Device.type == InputDeviceType::kController &&
					player1Device.deviceIndex == static_cast<int>(joyButtonPressed->joystickId) &&
					joyButtonPressed->button == 0)
				{
					isPlayer1Input = true;
				}
			}

			if (isPlayer1Input)
			{
				std::cout << "[BindingState] Host (Player 1) starting game with " << GetJoinedPlayerCount() << " players:\n";
				GetContext().sounds->Play(SoundEffect::kStartGame);

				//Save bindings to global config
				auto& config = PlayerBindingConfig::GetInstance();
				config.SetPlayerCount(GetJoinedPlayerCount());

				for (size_t i = 0; i < m_joined_players.size(); ++i)
				{
					config.SetPlayerDevice(static_cast<int>(i), m_joined_players[i].device);
					std::cout << "  Player " << (i + 1) << " -> "
						<< InputDeviceDetector::GetDeviceDescription(m_joined_players[i].device) << "\n";
				}

				//Transition to game state
				RequestStackPop();
				RequestStackPush(StateID::kGame);
				return false;
			}
		}
	}

	if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>())
	{
		if (keyPressed->code == sf::Keyboard::Key::Escape)
		{
			if (GetJoinedPlayerCount() == 0)
			{
				GetContext().sounds->Play(SoundEffect::kButtonClick);
				RequestStackPop();
				RequestStackPush(StateID::kMenu);
				return false;
			}
		}
	}

	if (const auto* joyButtonPressed = event.getIf<sf::Event::JoystickButtonPressed>())
	{
		if (joyButtonPressed->button == 1)
		{
			//Find player with this controller
			int playerIndex = -1;
			for (size_t i = 0; i < m_joined_players.size(); ++i)
			{
				if (m_joined_players[i].device.type == InputDeviceType::kController &&
					m_joined_players[i].device.deviceIndex == static_cast<int>(joyButtonPressed->joystickId))
				{
					playerIndex = static_cast<int>(i);
					break;
				}
			}

			if (playerIndex >= 0)
			{
				std::cout << "[BindingState] Player " << (playerIndex + 1) << " left (B button)\n";
				RemovePlayer(playerIndex);
				GetContext().sounds->Play(SoundEffect::kError);
				return false;
			}
		}
	}

	if (m_device_detector.IsInputEvent(event))
	{
		auto device = m_device_detector.DetectDeviceFromEvent(event);
		if (device.has_value())
		{
			//Check if this device is already bound to a player
			int existingPlayerIndex = -1;
			for (size_t i = 0; i < m_joined_players.size(); ++i)
			{
				if (m_joined_players[i].device.type == device->type &&
					m_joined_players[i].device.deviceIndex == device->deviceIndex)
				{
					existingPlayerIndex = static_cast<int>(i);
					break;
				}
			}

			if (existingPlayerIndex >= 0)
			{
				if (device->type != InputDeviceType::kController)
				{
					std::cout << "[BindingState] Player " << (existingPlayerIndex + 1) << " left\n";
					RemovePlayer(existingPlayerIndex);
					GetContext().sounds->Play(SoundEffect::kError);
				}
			}
			else if (CanAddMorePlayers())
			{
				AddPlayer(device.value());
				GetContext().sounds->Play(SoundEffect::kPairedPlayer);
				std::cout << "[BindingState] Player " << GetJoinedPlayerCount() << " joined with "
					<< InputDeviceDetector::GetDeviceDescription(device.value()) << "\n";
			}
			else
			{
				std::cout << "[BindingState] Maximum players reached!\n";
				GetContext().sounds->Play(SoundEffect::kError);
			}
		}
	}

	return false;
}

void BindingState::AddPlayer(const InputDeviceInfo& device)
{
	if (!CanAddMorePlayers())
		return;

	int playerIndex = static_cast<int>(m_joined_players.size());

	PlayerBinding newPlayer;
	newPlayer.playerId = playerIndex;
	newPlayer.device = device;
	newPlayer.isBound = true;

	m_joined_players.push_back(newPlayer);

	// Update grid slot
	m_player_slots[playerIndex].SetPlayerInfo(playerIndex + 1, device);

	// Set default color (will be customizable in commit 6)
	std::vector<sf::Color> default_colors = {
		sf::Color::Red, sf::Color::Yellow, sf::Color::Blue, sf::Color::Green,
		sf::Color::Magenta, sf::Color::Cyan, sf::Color(255, 165, 0), sf::Color(128, 0, 128),
		sf::Color(255, 192, 203), sf::Color(165, 42, 42), sf::Color(255, 255, 0), sf::Color(0, 255, 127),
		sf::Color(255, 20, 147), sf::Color(0, 191, 255), sf::Color(255, 140, 0), sf::Color(50, 205, 50),
		sf::Color(218, 112, 214), sf::Color(240, 230, 140), sf::Color(64, 224, 208), sf::Color(255, 99, 71)
	};

	if (playerIndex < static_cast<int>(default_colors.size()))
	{
		m_player_slots[playerIndex].SetPlayerColor(default_colors[playerIndex]);
	}
}

void BindingState::RemovePlayer(int index)
{
	if (index >= 0 && index < static_cast<int>(m_joined_players.size()))
	{
		m_joined_players.erase(m_joined_players.begin() + index);

		// Clear and rebuild all slots
		for (int i = 0; i < kMaxPlayers; ++i)
		{
			m_player_slots[i].Clear();
		}

		// Reassign player IDs and update slots
		for (size_t i = 0; i < m_joined_players.size(); ++i)
		{
			m_joined_players[i].playerId = static_cast<int>(i);
			m_player_slots[i].SetPlayerInfo(static_cast<int>(i) + 1, m_joined_players[i].device);
		}
	}
}

int BindingState::GetJoinedPlayerCount() const
{
	return static_cast<int>(m_joined_players.size());
}

bool BindingState::CanAddMorePlayers() const
{
	return GetJoinedPlayerCount() < kMaxPlayers;
}