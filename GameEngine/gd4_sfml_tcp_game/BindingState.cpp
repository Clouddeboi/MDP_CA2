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

	m_all_colors = {
	sf::Color::Red,
	sf::Color::Yellow,
	sf::Color::Blue,
	sf::Color::Green,
	sf::Color::Magenta,
	sf::Color::Cyan,
	sf::Color(255, 165, 0),
	sf::Color(128, 0, 128),
	sf::Color(255, 192, 203),
	sf::Color(165, 42, 42),
	sf::Color(255, 215, 0),
	sf::Color(0, 255, 127),
	sf::Color(255, 20, 147),
	sf::Color(0, 191, 255),
	sf::Color(255, 140, 0),
	sf::Color(50, 205, 50),
	sf::Color(218, 112, 214),
	sf::Color(240, 230, 140),
	sf::Color(64, 224, 208),
	sf::Color(255, 99, 71)
	};

	m_color_taken.resize(m_all_colors.size(), false);

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
	if (GetJoinedPlayerCount() >= 2)
	{
		if (AreAllPlayersReady() && m_ready_text)
		{
			std::string hostDevice = "ENTER";
			if (!m_joined_players.empty() && m_joined_players[0].device.type == InputDeviceType::kController)
			{
				hostDevice = "A";
			}

			m_ready_text->setString("ALL READY! Player 1 press " + hostDevice + " to START GAME");
			m_ready_text->setFillColor(sf::Color::Green);
			m_ready_text->setCharacterSize(28);
			Utility::CentreOrigin(*m_ready_text);
			window.draw(*m_ready_text);
		}
		else if (m_ready_text)
		{
			m_ready_text->setString("Keyboard: SPACE to ready | ESC to leave\nController: A to ready | B to leave");
			m_ready_text->setFillColor(sf::Color::White);
			m_ready_text->setCharacterSize(20);
			Utility::CentreOrigin(*m_ready_text);
			window.draw(*m_ready_text);
		}
	}
	else if (GetJoinedPlayerCount() < 2 && m_ready_text)
	{
		m_ready_text->setString("Need at least 2 players to start");
		m_ready_text->setFillColor(sf::Color::Yellow);
		m_ready_text->setCharacterSize(24);
		Utility::CentreOrigin(*m_ready_text);
		window.draw(*m_ready_text);
	}

	// Show ready text based on state
	if (GetJoinedPlayerCount() >= 2)
	{
		if (AreAllPlayersReady() && m_ready_text)
		{
			std::string hostDevice = "ENTER";
			if (!m_joined_players.empty() && m_joined_players[0].device.type == InputDeviceType::kController)
			{
				hostDevice = "A";
			}

			m_ready_text->setString("ALL READY! Player 1 press " + hostDevice + " to START GAME");
			m_ready_text->setFillColor(sf::Color::Green);
			m_ready_text->setCharacterSize(28);
			Utility::CentreOrigin(*m_ready_text);
			window.draw(*m_ready_text);
		}
		else if (m_ready_text)
		{
			m_ready_text->setString("Keyboard: SPACE to ready | ESC to leave\nController: A to ready | B to leave");
			m_ready_text->setFillColor(sf::Color::White);
			m_ready_text->setCharacterSize(20);
			Utility::CentreOrigin(*m_ready_text);
			window.draw(*m_ready_text);
		}
	}
	else if (GetJoinedPlayerCount() == 1 && m_ready_text)
	{
		m_ready_text->setString("Need at least 2 players to start");
		m_ready_text->setFillColor(sf::Color::Yellow);
		m_ready_text->setCharacterSize(24);
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
	// Only Player 1 (host) can start the game with ENTER when all are ready
	if (GetJoinedPlayerCount() >= 2 && AreAllPlayersReady())
	{
		if (!m_joined_players.empty())
		{
			const auto& player1Device = m_joined_players[0].device;
			bool isPlayer1Start = false;

			if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>())
			{
				if (player1Device.type == InputDeviceType::kKeyboardMouse &&
					keyPressed->code == sf::Keyboard::Key::Enter)
				{
					isPlayer1Start = true;
				}
			}

			if (const auto* joyButtonPressed = event.getIf<sf::Event::JoystickButtonPressed>())
			{
				if (player1Device.type == InputDeviceType::kController &&
					player1Device.deviceIndex == static_cast<int>(joyButtonPressed->joystickId) &&
					joyButtonPressed->button == 0)
				{
					isPlayer1Start = true;
				}
			}

			if (isPlayer1Start)
			{
				std::cout << "[BindingState] Host starting game with " << GetJoinedPlayerCount() << " players:\n";
				GetContext().sounds->Play(SoundEffect::kStartGame);

				auto& config = PlayerBindingConfig::GetInstance();
				config.SetPlayerCount(GetJoinedPlayerCount());

				for (size_t i = 0; i < m_joined_players.size(); ++i)
				{
					config.SetPlayerDevice(static_cast<int>(i), m_joined_players[i].device);

					int colorIndex = m_player_slots[i].GetSelectedColorIndex();
					if (colorIndex >= 0 && colorIndex < static_cast<int>(m_all_colors.size()))
					{
						config.SetPlayerColor(static_cast<int>(i), m_all_colors[colorIndex]);
					}

					std::cout << "  Player " << (i + 1) << " -> "
						<< InputDeviceDetector::GetDeviceDescription(m_joined_players[i].device) << "\n";
				}

				RequestStackPop();
				RequestStackPush(StateID::kGame);
				return false;
			}
		}
	}

	if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>())
	{
		if (keyPressed->code == sf::Keyboard::Key::Escape ||
			keyPressed->code == sf::Keyboard::Key::Backspace)
		{
			int keyboardPlayerIndex = -1;
			for (size_t i = 0; i < m_joined_players.size(); ++i)
			{
				if (m_joined_players[i].device.type == InputDeviceType::kKeyboardMouse)
				{
					keyboardPlayerIndex = static_cast<int>(i);
					break;
				}
			}

			if (keyboardPlayerIndex >= 0)
			{
				std::cout << "[BindingState] Player " << (keyboardPlayerIndex + 1) << " left\n";
				RemovePlayer(keyboardPlayerIndex);
				GetContext().sounds->Play(SoundEffect::kError);
			}
			else if (GetJoinedPlayerCount() == 0)
			{
				GetContext().sounds->Play(SoundEffect::kButtonClick);
				RequestStackPop();
				RequestStackPush(StateID::kMenu);
			}
			return false;
		}

		//WASD navigation for keyboard player's color grid
		if (keyPressed->code == sf::Keyboard::Key::W ||
			keyPressed->code == sf::Keyboard::Key::A ||
			keyPressed->code == sf::Keyboard::Key::S ||
			keyPressed->code == sf::Keyboard::Key::D)
		{
			for (size_t i = 0; i < m_joined_players.size(); ++i)
			{
				if (m_joined_players[i].device.type == InputDeviceType::kKeyboardMouse)
				{
					if (m_player_slots[i].IsShowingColorPicker())
					{
						int deltaX = 0, deltaY = 0;
						if (keyPressed->code == sf::Keyboard::Key::W) deltaY = -1;
						if (keyPressed->code == sf::Keyboard::Key::S) deltaY = 1;
						if (keyPressed->code == sf::Keyboard::Key::A) deltaX = -1;
						if (keyPressed->code == sf::Keyboard::Key::D) deltaX = 1;

						m_player_slots[i].NavigateColorGrid(deltaX, deltaY);
						GetContext().sounds->Play(SoundEffect::kButtonClick);
						return false;
					}
				}
			}
		}

		if (keyPressed->code == sf::Keyboard::Key::Enter)
		{
			for (size_t i = 0; i < m_joined_players.size(); ++i)
			{
				if (m_joined_players[i].device.type == InputDeviceType::kKeyboardMouse)
				{
					if (m_player_slots[i].IsShowingColorPicker())
					{
						//Confirm color selection and ready up
						m_player_slots[i].ConfirmColorSelection();
						m_player_slots[i].SetReady(true);
						GetContext().sounds->Play(SoundEffect::kPairedPlayer);
						std::cout << "[BindingState] Player " << (i + 1) << " confirmed color and readied up\n";
						return false;
					}
					else
					{
						//Toggle ready if already selected color
						bool currentReady = m_player_slots[i].IsReady();
						m_player_slots[i].SetReady(!currentReady);
						GetContext().sounds->Play(SoundEffect::kButtonClick);
						std::cout << "[BindingState] Player " << (i + 1) << (currentReady ? " unready" : " ready") << "\n";
						return false;
					}
				}
			}
		}

		if (keyPressed->code == sf::Keyboard::Key::Space)
		{
			for (size_t i = 0; i < m_joined_players.size(); ++i)
			{
				if (m_joined_players[i].device.type == InputDeviceType::kKeyboardMouse)
				{
					if (!m_player_slots[i].IsShowingColorPicker())
					{
						bool currentReady = m_player_slots[i].IsReady();
						m_player_slots[i].SetReady(!currentReady);
						GetContext().sounds->Play(SoundEffect::kButtonClick);
						std::cout << "[BindingState] Player " << (i + 1) << (currentReady ? " unready" : " ready") << "\n";
						return false;
					}
				}
			}
		}
	}

	//Controller joystick axis for color grid navigation
	if (const auto* joyMoved = event.getIf<sf::Event::JoystickMoved>())
	{
		for (size_t i = 0; i < m_joined_players.size(); ++i)
		{
			if (m_joined_players[i].device.type == InputDeviceType::kController &&
				m_joined_players[i].device.deviceIndex == static_cast<int>(joyMoved->joystickId))
			{
				if (m_player_slots[i].IsShowingColorPicker())
				{
					const float deadzone = 50.f;
					static sf::Time lastMoveTime = sf::Time::Zero;
					static int lastJoystickId = -1;

					//Simple cooldown to prevent rapid navigation
					if (lastJoystickId != static_cast<int>(joyMoved->joystickId) || m_elapsed_time - lastMoveTime > sf::milliseconds(200))
					{
						int deltaX = 0, deltaY = 0;

						if (joyMoved->axis == sf::Joystick::Axis::X || joyMoved->axis == sf::Joystick::Axis::PovX)
						{
							if (joyMoved->position > deadzone) deltaX = 1;
							else if (joyMoved->position < -deadzone) deltaX = -1;
						}

						if (joyMoved->axis == sf::Joystick::Axis::Y || joyMoved->axis == sf::Joystick::Axis::PovY)
						{
							if (joyMoved->position > deadzone) deltaY = -1;
							else if (joyMoved->position < -deadzone) deltaY = 1;
						}

						if (deltaX != 0 || deltaY != 0)
						{
							m_player_slots[i].NavigateColorGrid(deltaX, deltaY);
							GetContext().sounds->Play(SoundEffect::kButtonClick);
							lastMoveTime = m_elapsed_time;
							lastJoystickId = static_cast<int>(joyMoved->joystickId);
						}
					}
					return false;
				}
			}
		}
	}

	if (const auto* joyButtonPressed = event.getIf<sf::Event::JoystickButtonPressed>())
	{
		if (joyButtonPressed->button == 1)
		{
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
			}
			else if (GetJoinedPlayerCount() == 0 || !AreAllPlayersReady())
			{
				GetContext().sounds->Play(SoundEffect::kButtonClick);
				RequestStackPop();
				RequestStackPush(StateID::kMenu);
			}
			return false;
		}

		if (joyButtonPressed->button == 0)
		{
			//Find this controller player
			for (size_t i = 0; i < m_joined_players.size(); ++i)
			{
				if (m_joined_players[i].device.type == InputDeviceType::kController &&
					m_joined_players[i].device.deviceIndex == static_cast<int>(joyButtonPressed->joystickId))
				{
					//If showing color picker, confirm selection
					if (m_player_slots[i].IsShowingColorPicker())
					{
						m_player_slots[i].ConfirmColorSelection();
						m_player_slots[i].SetReady(true);
						GetContext().sounds->Play(SoundEffect::kPairedPlayer);
						std::cout << "[BindingState] Player " << (i + 1) << " confirmed color and readied up\n";
					}
					else
					{
						//Toggle ready state if already has color
						bool currentReady = m_player_slots[i].IsReady();
						m_player_slots[i].SetReady(!currentReady);
						GetContext().sounds->Play(SoundEffect::kButtonClick);
						std::cout << "[BindingState] Player " << (i + 1) << (currentReady ? " unready" : " ready") << "\n";
					}
					return false;
				}
			}
		}
	}

	//Detect input events for joining
	if (m_device_detector.IsInputEvent(event) && CanAddMorePlayers())
	{
		auto device = m_device_detector.DetectDeviceFromEvent(event);
		if (device.has_value())
		{
			//Check if device already bound
			bool alreadyBound = false;
			for (const auto& player : m_joined_players)
			{
				if (player.device == device.value())
				{
					alreadyBound = true;
					break;
				}
			}

			if (!alreadyBound)
			{
				AddPlayer(device.value());
				GetContext().sounds->Play(SoundEffect::kPairedPlayer);
				std::cout << "[BindingState] Player " << GetJoinedPlayerCount() << " joined\n";
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

	//Update grid slot
	m_player_slots[playerIndex].SetPlayerInfo(playerIndex + 1, device);

	//Get available colors
	std::vector<sf::Color> available_colors;
	for (size_t i = 0; i < m_all_colors.size(); ++i)
	{
		if (!m_color_taken[i])
		{
			available_colors.push_back(m_all_colors[i]);
		}
	}

	//Set available colors and show picker
	m_player_slots[playerIndex].SetAvailableColors(m_all_colors);
	m_player_slots[playerIndex].ShowColorPicker(true);

	//Auto-select first available color
	for (size_t i = 0; i < m_all_colors.size(); ++i)
	{
		if (!m_color_taken[i])
		{
			m_player_slots[playerIndex].SelectColorAtIndex(static_cast<int>(i));
			m_color_taken[i] = true;
			break;
		}
	}
}

void BindingState::RemovePlayer(int index)
{
	if (index >= 0 && index < static_cast<int>(m_joined_players.size()))
	{
		//Free up the color
		int colorIndex = m_player_slots[index].GetSelectedColorIndex();
		if (colorIndex >= 0 && colorIndex < static_cast<int>(m_color_taken.size()))
		{
			m_color_taken[colorIndex] = false;
		}

		m_joined_players.erase(m_joined_players.begin() + index);

		//Clear all slots
		for (int i = 0; i < kMaxPlayers; ++i)
		{
			m_player_slots[i].Clear();
		}

		//Rebuild slots with remaining players
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

bool BindingState::AreAllPlayersReady() const
{
	if (m_joined_players.empty())
		return false;

	for (const auto& player : m_joined_players)
	{
		//Check if player slot is marked as ready
		int index = player.playerId;
		if (index >= 0 && index < static_cast<int>(m_player_slots.size()))
		{
			if (!m_player_slots[index].IsReady())
				return false;
		}
	}
	return true;
}