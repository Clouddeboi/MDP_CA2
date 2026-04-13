#include "HostLobbyState.hpp"
#include "ResourceHolder.hpp"
#include "Utility.hpp"
#include "NetworkSession.hpp"
#include "SoundEffect.hpp"
#include <SFML/Graphics/RenderWindow.hpp>

HostLobbyState::HostLobbyState(StateStack& stack, Context context)
    : State(stack, context)
    , m_background_sprite(context.textures->Get(TextureID::kTitleScreen))
    , m_transitioned(false)
{
    sf::Vector2f windowSize(context.window->getSize());

    m_title_text.emplace(context.fonts->Get(Font::kMain), "HOST LOBBY", 56);
    m_title_text->setFillColor(sf::Color::White);
    m_title_text->setOutlineColor(sf::Color::Black);
    m_title_text->setOutlineThickness(3.f);
    Utility::CentreOrigin(*m_title_text);
    m_title_text->setPosition({ windowSize.x * 0.5f, 120.f });

    m_info_text.emplace(context.fonts->Get(Font::kMain),
        "Server is running.\nWaiting for a client to connect...\n\nPress ESC to cancel.",
        26);
    m_info_text->setFillColor(sf::Color::Cyan);
    m_info_text->setOutlineColor(sf::Color::Black);
    m_info_text->setOutlineThickness(2.f);
    Utility::CentreOrigin(*m_info_text);
    m_info_text->setPosition({ windowSize.x * 0.5f, windowSize.y * 0.5f });
}

void HostLobbyState::Draw()
{
    sf::RenderWindow& window = *GetContext().window;
    window.setView(window.getDefaultView());
    window.draw(m_background_sprite);

    if (m_title_text) window.draw(*m_title_text);
    if (m_info_text) window.draw(*m_info_text);
}

bool HostLobbyState::Update(sf::Time)
{
    if (!m_transitioned && GetContext().network && GetContext().network->HasHostClientConnected())
    {
        m_transitioned = true;
        GetContext().sounds->Play(SoundEffect::kPairedPlayer);

        //Pop HostLobby + Menu, then enter Binding
        RequestStackPop();
        RequestStackPop();
        RequestStackPush(StateID::kBinding);
        return false;
    }

    return true;
}

bool HostLobbyState::HandleEvent(const sf::Event& event)
{
    if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>())
    {
        if (keyPressed->code == sf::Keyboard::Key::Escape ||
            keyPressed->code == sf::Keyboard::Key::Backspace)
        {
            if (GetContext().network)
            {
                GetContext().network->Reset();
            }

            RequestStackPop();
            return false;
        }
    }

    return true;
}