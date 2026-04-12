#include "BindingState.hpp"
#include "ResourceHolder.hpp"
#include "Utility.hpp"
#include "PlayerBindingConfig.hpp"
#include "NetworkSession.hpp" 
#include "NetworkSlotColor.hpp"
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
	, m_network_sync_timer(sf::Time::Zero)
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

	m_network_mode = (GetContext().network && GetContext().network->IsActive());
	if (m_network_mode)
	{
		m_joined_players.clear();
		//Slots are now created by authoritative network updates/snapshots.
		//Local player index defaults by role:
		//Host = 0, clients will be updated once authoritative snapshot events arrive.
		m_local_player_index = GetContext().network->IsHosting() ? 0 : 1;

		//Ensure local host slot exists immediately for host UX
		if (GetContext().network->IsHosting())
		{
			EnsurePlayerSlotExists(m_local_player_index);

			//Deterministic host default
			m_player_slots[m_local_player_index].SelectColorAtIndex(0);
			m_player_slots[m_local_player_index].SetReady(false);
			m_player_slots[m_local_player_index].ShowColorPicker(true);
			RebuildColorTakenFromSlots();
		}

		//Force first outbound sync
		m_last_sent_color = -999;
		m_last_sent_ready = false;

		m_instructions_text->setString("Network lobby: choose color and ready up");
	}
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

	if (!m_ready_text)
		return;

	if (GetJoinedPlayerCount() >= 2)
	{
		if (AreAllPlayersReady())
		{
			m_ready_text->setString("ALL READY! Host press ENTER to START");
			m_ready_text->setFillColor(sf::Color::Green);
			m_ready_text->setCharacterSize(28);
		}
		else
		{
			if (m_network_mode)
			{
				m_ready_text->setString("Choose color and ready up");
			}
			else
			{
				m_ready_text->setString("Keyboard: SPACE to ready | ESC to leave\nController: A to ready | B to leave");
			}
			m_ready_text->setFillColor(sf::Color::White);
			m_ready_text->setCharacterSize(20);
		}
	}
	else
	{
		m_ready_text->setString("Need at least 2 players to start");
		m_ready_text->setFillColor(sf::Color::Yellow);
		m_ready_text->setCharacterSize(24);
	}

	Utility::CentreOrigin(*m_ready_text);
	window.draw(*m_ready_text);
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
		if (m_network_mode)
		{
			m_instructions_text->setString("Network lobby: choose color and ready up");
			m_instructions_text->setFillColor(sf::Color::Cyan);
		}
		else if (GetJoinedPlayerCount() >= kMaxPlayers)
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

	//Network communication for lobby binding state
	if (m_network_mode && GetContext().network)
	{
		GetContext().network->PollLobbyPackets();

		//int assignedLocalIndex = -1;
		//if (GetContext().network->ConsumeAssignedLocalPlayerIndex(assignedLocalIndex))
		//{
		//	m_local_player_index = assignedLocalIndex;
		//	EnsurePlayerSlotExists(m_local_player_index);
		//	m_player_slots[m_local_player_index].ShowColorPicker(true);
		//}

		int player = -1;
		int color = -1;
		bool ready = false;

		//Consume all queued updates, not just one
		while (GetContext().network->ConsumeRemoteBindingState(player, color, ready))
		{
			EnsurePlayerSlotExists(player);

			ApplyRemoteSlotState(player, color, ready);
			RebuildColorTakenFromSlots();

			//Host authoritative color conflict resolution for remote players
			if (GetContext().network->IsHosting() && player != m_local_player_index)
			{
				const int hostColor = m_player_slots[m_local_player_index].GetSelectedColorIndex();
				int remoteColor = m_player_slots[player].GetSelectedColorIndex();

				if (remoteColor == hostColor && hostColor >= 0)
				{
					const int replacement = FindFirstFreeColorIndex(hostColor);
					if (replacement >= 0)
					{
						m_player_slots[player].SelectColorAtIndex(replacement);
						m_player_slots[player].SetReady(false);
						remoteColor = replacement;
					}
					else
					{
						m_player_slots[player].SetReady(false);
					}
				}

				GetContext().network->HostBroadcastLobbyBindingState(
					player,
					remoteColor,
					m_player_slots[player].IsReady()
				);

				RebuildColorTakenFromSlots();
			}
		}

		int leftIndex = -1;

		while (GetContext().network->ConsumeRemotePlayerLeft(leftIndex))
		{
			//Host sees client leave -> remove that slot from lobby, keep lobby open
			if (GetContext().network->IsHosting() && leftIndex != m_local_player_index)
			{
				if (leftIndex >= 0 && leftIndex < GetJoinedPlayerCount())
				{
					RemovePlayer(leftIndex);
					if (leftIndex < m_local_player_index)
					{
						//Removal shifted local slot left by one
						m_local_player_index--;
					}
				}

				//Keep host slot interactive
				if (m_local_player_index >= 0 && m_local_player_index < GetJoinedPlayerCount())
				{
					if (!m_network_mode)
						m_player_slots[m_local_player_index].ShowColorPicker(true);
				}
				else
				{
					m_local_player_index = 0;
					if (GetJoinedPlayerCount() > 0 && !m_network_mode)
						m_player_slots[m_local_player_index].ShowColorPicker(true);
				}

				if (m_instructions_text)
				{
					m_instructions_text->setString("A player left. Waiting for player...");
					m_instructions_text->setFillColor(sf::Color::Yellow);
					Utility::CentreOrigin(*m_instructions_text);
				}
			}
			else
			{
				//Client sees host leave, or any critical leave -> close lobby
				GetContext().network->Reset();
				RequestStackClear();
				RequestStackPush(StateID::kMenu);
				return false;
			}
		}

		if (GetContext().network->ConsumeStartGameSignal())
		{
			// Colors are now assigned deterministically in-game by network ID.
			// No need to carry lobby color choices into the game state.
			auto& config = PlayerBindingConfig::GetInstance();
			config.SetPlayerCount(GetJoinedPlayerCount());

			RequestStackPop();
			RequestStackPush(StateID::kMultiplayerGame);
			return false;
		}

		const int localColor = m_player_slots[m_local_player_index].GetSelectedColorIndex();
		const bool localReady = m_player_slots[m_local_player_index].IsReady();

		m_network_sync_timer += dt;
		const bool periodicSync = (m_network_sync_timer >= m_network_sync_interval);

		if (localColor != m_last_sent_color || localReady != m_last_sent_ready || periodicSync)
		{
			GetContext().network->SendLobbyBindingState(m_local_player_index, localColor, localReady);
			m_last_sent_color = localColor;
			m_last_sent_ready = localReady;

			if (periodicSync)
				m_network_sync_timer = sf::Time::Zero;
		}
	}

	return true;
}

bool BindingState::HandleEvent(const sf::Event& event)
{
	//NETWORK MODE: handle and return immediately
	if (m_network_mode && GetContext().network)
	{
		const int i = m_local_player_index;

		if (i < 0 || i >= GetJoinedPlayerCount())
		{
			//Wait for authoritative snapshot/updates to create local slot
			return true;
		}

		if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>())
		{
			if (keyPressed->code == sf::Keyboard::Key::Escape ||
				keyPressed->code == sf::Keyboard::Key::Backspace)
			{
				GetContext().network->SendLobbyLeave(m_local_player_index);
				GetContext().network->Reset();
				RequestStackClear();
				RequestStackPush(StateID::kMenu);
				return false;
			}

			//if (m_player_slots[i].IsShowingColorPicker() &&
			//	(keyPressed->code == sf::Keyboard::Key::W ||
			//		keyPressed->code == sf::Keyboard::Key::A ||
			//		keyPressed->code == sf::Keyboard::Key::S ||
			//		keyPressed->code == sf::Keyboard::Key::D ||
			//		keyPressed->code == sf::Keyboard::Key::Up ||
			//		keyPressed->code == sf::Keyboard::Key::Down ||
			//		keyPressed->code == sf::Keyboard::Key::Left ||
			//		keyPressed->code == sf::Keyboard::Key::Right))
			//{
			//	int dx = 0, dy = 0;
			//	if (keyPressed->code == sf::Keyboard::Key::W || keyPressed->code == sf::Keyboard::Key::Up) dy = -1;
			//	if (keyPressed->code == sf::Keyboard::Key::S || keyPressed->code == sf::Keyboard::Key::Down) dy = 1;
			//	if (keyPressed->code == sf::Keyboard::Key::A || keyPressed->code == sf::Keyboard::Key::Left) dx = -1;
			//	if (keyPressed->code == sf::Keyboard::Key::D || keyPressed->code == sf::Keyboard::Key::Right) dx = 1;

			//	m_player_slots[i].NavigateColorGrid(dx, dy);
			//	GetContext().sounds->Play(SoundEffect::kButtonClick);

			//	//Immediate sync
			//	const int localColor = m_player_slots[i].GetSelectedColorIndex();
			//	const bool localReady = m_player_slots[i].IsReady();
			//	GetContext().network->SendLobbyBindingState(m_local_player_index, localColor, localReady);
			//	m_last_sent_color = localColor;
			//	m_last_sent_ready = localReady;

			//	return false;
			//}

			//Confirm/ready/start — no color picker in network mode, just ready up
			if (keyPressed->code == sf::Keyboard::Key::Enter || keyPressed->code == sf::Keyboard::Key::Space)
			{
				//If all ready, host starts, client requests start
				if (AreAllPlayersReady())
				{
					if (GetContext().network->IsHosting())
					{
						GetContext().network->SendLobbyStartGame();
						GetContext().sounds->Play(SoundEffect::kStartGame);
						RequestStackPop();
						RequestStackPush(StateID::kMultiplayerGame);
					}
					else
					{
						GetContext().network->SendLobbyStartRequest();
						GetContext().sounds->Play(SoundEffect::kButtonClick);
					}
					return false;
				}

				//Toggle ready
				const bool currentReady = m_player_slots[i].IsReady();
				m_player_slots[i].SetReady(!currentReady);
				GetContext().sounds->Play(SoundEffect::kButtonClick);

				const int localColor = m_player_slots[i].GetSelectedColorIndex();
				const bool localReady = m_player_slots[i].IsReady();
				GetContext().network->SendLobbyBindingState(m_local_player_index, localColor, localReady);
				m_last_sent_color = localColor;
				m_last_sent_ready = localReady;

				return false;
			}
		}

		return true;
	}

	//LOCAL MODE: existing old logic
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
			keyPressed->code == sf::Keyboard::Key::D ||
			keyPressed->code == sf::Keyboard::Key::Up ||
			keyPressed->code == sf::Keyboard::Key::Down ||
			keyPressed->code == sf::Keyboard::Key::Left ||
			keyPressed->code == sf::Keyboard::Key::Right)
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

						if (keyPressed->code == sf::Keyboard::Key::Up) deltaY = -1;
						if (keyPressed->code == sf::Keyboard::Key::Down) deltaY = 1;
						if (keyPressed->code == sf::Keyboard::Key::Left) deltaX = -1;
						if (keyPressed->code == sf::Keyboard::Key::Right) deltaX = 1;

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
						//Confirm and mark color selection as taken and ready up
						m_player_slots[i].ConfirmColorSelection();

						int selectedColor = m_player_slots[i].GetSelectedColorIndex();
						if (selectedColor >= 0 && selectedColor < static_cast<int>(m_color_taken.size()))
						{
							m_color_taken[selectedColor] = true;
							UpdateColorAvailability();
						}

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

						if (currentReady)
						{
							int currentColor = m_player_slots[i].GetSelectedColorIndex();
							if (currentColor >= 0 && currentColor < static_cast<int>(m_color_taken.size()))
							{
								m_color_taken[currentColor] = false;
								UpdateColorAvailability();
							}
							m_player_slots[i].ShowColorPicker(true);
						}

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

						if (currentReady)
						{
							int currentColor = m_player_slots[i].GetSelectedColorIndex();
							if (currentColor >= 0 && currentColor < static_cast<int>(m_color_taken.size()))
							{
								m_color_taken[currentColor] = false;
								UpdateColorAvailability();
							}
							m_player_slots[i].ShowColorPicker(true);
						}

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

						int selectedColor = m_player_slots[i].GetSelectedColorIndex();
						if (selectedColor >= 0 && selectedColor < static_cast<int>(m_color_taken.size()))
						{
							m_color_taken[selectedColor] = true;
							UpdateColorAvailability();
						}

						m_player_slots[i].SetReady(true);
						GetContext().sounds->Play(SoundEffect::kPairedPlayer);
						std::cout << "[BindingState] Player " << (i + 1) << " confirmed color and readied up\n";
					}
					else
					{
						//Toggle ready state if already has color
						bool currentReady = m_player_slots[i].IsReady();
						m_player_slots[i].SetReady(!currentReady);

						if (currentReady)
						{
							int currentColor = m_player_slots[i].GetSelectedColorIndex();
							if (currentColor >= 0 && currentColor < static_cast<int>(m_color_taken.size()))
							{
								m_color_taken[currentColor] = false;
								UpdateColorAvailability();
							}
							m_player_slots[i].ShowColorPicker(true);
						}

						GetContext().sounds->Play(SoundEffect::kButtonClick);
						std::cout << "[BindingState] Player " << (i + 1) << (currentReady ? " unready" : " ready") << "\n";
					}
					return false;
				}
			}
		}
	}

	if (!m_network_mode && m_device_detector.IsInputEvent(event) && CanAddMorePlayers())
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

	m_player_slots[playerIndex].SetPlayerInfo(playerIndex + 1, device);

	if (m_network_mode)
	{
		// In network mode: mark slot as network-mode (hides picker permanently),
		// then apply the deterministic palette color directly to the sprite.
		m_player_slots[playerIndex].SetNetworkMode(true);
		m_player_slots[playerIndex].SetPlayerColor(NetworkSlotColor(
			static_cast<std::uint8_t>(playerIndex)));
	}
	else
	{
		// Local mode: full color picker as before
		m_player_slots[playerIndex].SetAvailableColors(m_all_colors);

		for (size_t i = 0; i < m_all_colors.size(); ++i)
			m_player_slots[playerIndex].MarkColorAsUnavailable(i, m_color_taken[i]);

		m_player_slots[playerIndex].ShowColorPicker(true);

		for (size_t i = 0; i < m_all_colors.size(); ++i)
		{
			if (!m_color_taken[i])
			{
				m_player_slots[playerIndex].SelectColorAtIndex(static_cast<int>(i));
				break;
			}
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

		RebuildColorTakenFromSlots();

		//In network mode, keep remote slot picker disabled
		if (m_network_mode)
		{
			for (int i = 0; i < GetJoinedPlayerCount(); ++i)
			{
				if (i != m_local_player_index)
				{
					m_player_slots[i].ShowColorPicker(false);
				}
			}
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

void BindingState::UpdateColorAvailability()
{
	//Update all players' color pickers with current availability
	for (int i = 0; i < GetJoinedPlayerCount(); ++i)
	{
		for (size_t colorIdx = 0; colorIdx < m_color_taken.size(); ++colorIdx)
		{
			bool isMyColor = (m_player_slots[i].GetSelectedColorIndex() == static_cast<int>(colorIdx));
			m_player_slots[i].MarkColorAsUnavailable(colorIdx, m_color_taken[colorIdx] && !isMyColor);
		}
	}
}

bool BindingState::IsLocalControllableSlot(int index) const
{
	if (!m_network_mode)
		return true;

	return index == m_local_player_index;
}

int BindingState::FindFirstFreeColorIndex(int blockedIndex) const
{
	for (int i = 0; i < static_cast<int>(m_color_taken.size()); ++i)
	{
		if (i == blockedIndex)
			continue;
		if (!m_color_taken[i])
			return i;
	}
	return -1;
}

void BindingState::RebuildColorTakenFromSlots()
{
	std::fill(m_color_taken.begin(), m_color_taken.end(), false);

	for (int i = 0; i < GetJoinedPlayerCount(); ++i)
	{
		const int c = m_player_slots[i].GetSelectedColorIndex();
		if (c >= 0 && c < static_cast<int>(m_color_taken.size()))
		{
			m_color_taken[c] = true;
		}
	}

	UpdateColorAvailability();
}

void BindingState::ApplyRemoteSlotState(int slotIndex, int colorIndex, bool ready)
{
	if (slotIndex < 0 || slotIndex >= static_cast<int>(m_player_slots.size()))
		return;

	if (colorIndex >= 0)
		m_player_slots[slotIndex].SelectColorAtIndex(colorIndex);

	m_player_slots[slotIndex].SetReady(ready);

	//remote slot should not open picker locally
	if (slotIndex != m_local_player_index)
	{
		m_player_slots[slotIndex].ShowColorPicker(false);
	}
}

void BindingState::EnsurePlayerSlotExists(int playerIndex)
{
	while (GetJoinedPlayerCount() <= playerIndex && CanAddMorePlayers())
	{
		AddPlayer(InputDeviceInfo(InputDeviceType::kKeyboardMouse, -1));
	}

	if (m_network_mode)
	{
		for (int i = 0; i < GetJoinedPlayerCount(); ++i)
		{
			if (i != m_local_player_index)
			{
				m_player_slots[i].ShowColorPicker(false);
			}
		}
	}
}