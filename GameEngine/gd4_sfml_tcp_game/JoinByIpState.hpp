#pragma once
#include "State.hpp"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include <optional>
#include <string>

class JoinByIpState : public State
{
public:
    JoinByIpState(StateStack& stack, Context context);
    virtual void Draw() override;
    virtual bool Update(sf::Time dt) override;
    virtual bool HandleEvent(const sf::Event& event) override;

private:
    void RefreshInfoText();

private:
    sf::Sprite m_background_sprite;
    std::optional<sf::Text> m_title_text;
    std::optional<sf::Text> m_info_text;

    std::string m_ip_input;
    std::string m_status_message;
};