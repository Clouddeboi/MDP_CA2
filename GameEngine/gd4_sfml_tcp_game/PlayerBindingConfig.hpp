#pragma once
#include "InputDevice.hpp"
#include <array>
#include <optional>

/*
 * Code implementation assisted by Claude Sonnet 4.5
 * Used for: "Player Input Binding configs (get device, clear devices etc.)"
 * Original implementation, modified/adapted by Michal Becmer (D00256088) for project requirements
 */

//Singleton to store player binding configuration between states
class PlayerBindingConfig
{
public:
	static PlayerBindingConfig& GetInstance()
	{
		static PlayerBindingConfig instance;
		return instance;
	}

	void SetPlayerDevice(int playerId, const InputDeviceInfo& device)
	{
		if (playerId >= 0 && playerId < kMaxPlayers)
		{
			m_player_devices[playerId] = device;
		}
	}

	std::optional<InputDeviceInfo> GetPlayerDevice(int playerId) const
	{
		if (playerId >= 0 && playerId < kMaxPlayers)
		{
			return m_player_devices[playerId];
		}
		return std::nullopt;
	}

	void Clear()
	{
		for (auto& device : m_player_devices)
		{
			device.reset();
		}
		for (auto& color : m_player_colors)
		{
			color.reset();
		}
		m_player_count = 2;
	}

	bool HasBindings() const
	{
		for (const auto& device : m_player_devices)
		{
			if (device.has_value())
				return true;
		}
		return false;
	}

	void SetPlayerCount(int count)
	{
		m_player_count = count;
	}

	void SetPlayerColor(int playerId, const sf::Color& color)
	{
		if (playerId >= 0 && playerId < kMaxPlayers)
		{
			m_player_colors[playerId] = color;
		}
	}

	std::optional<sf::Color> GetPlayerColor(int playerId) const
	{
		if (playerId >= 0 && playerId < kMaxPlayers)
		{
			return m_player_colors[playerId];
		}
		return std::nullopt;
	}

private:
	static constexpr int kMaxPlayers = 2;
	std::array<std::optional<InputDeviceInfo>, kMaxPlayers> m_player_devices;
	std::array<std::optional<sf::Color>, kMaxPlayers> m_player_colors;
	int m_player_count = 2;

	PlayerBindingConfig() = default;
	~PlayerBindingConfig() = default;
	PlayerBindingConfig(const PlayerBindingConfig&) = delete;
	PlayerBindingConfig& operator=(const PlayerBindingConfig&) = delete;
};