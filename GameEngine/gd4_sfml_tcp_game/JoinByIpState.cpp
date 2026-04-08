#include "JoinByIpState.hpp"
#include "ResourceHolder.hpp"
#include "Utility.hpp"
#include "NetworkSession.hpp"
#include "SoundEffect.hpp"
#include <SFML/Graphics/RenderWindow.hpp>

JoinByIpState::JoinByIpState(StateStack& stack, Context context)
    : State(stack, context)
    , m_background_sprite(context.textures->Get(TextureID::kTitleScreen))
    , m_ip_input("127.0.0.1")
    , m_status_message("Type host IP and press ENTER.")
{
    sf::Vector2f windowSize(context.window->getSize());

    m_title_text.emplace(context.fonts->Get(Font::kMain), "JOIN BY IP", 56);
    m_title_text->setFillColor(sf::Color::White);
    m_title_text->setOutlineColor(sf::Color::Black);
    m_title_text->setOutlineThickness(3.f);
    Utility::CentreOrigin(*m_title_text);
    m_title_text->setPosition({ windowSize.x * 0.5f, 120.f });

    m_info_text.emplace(context.fonts->Get(Font::kMain), "", 26);
    m_info_text->setFillColor(sf::Color::Yellow);
    m_info_text->setOutlineColor(sf::Color::Black);
    m_info_text->setOutlineThickness(2.f);
    m_info_text->setPosition({ windowSize.x * 0.5f, windowSize.y * 0.5f });

    RefreshInfoText();
}

void JoinByIpState::RefreshInfoText()
{
    if (!m_info_text)
        return;

    std::string ui =
        "Host IP: " + m_ip_input +
        "\n\nENTER = Connect"
        "\nBACKSPACE = Delete"
        "\nESC = Cancel"
        "\n\n" + m_status_message;

    m_info_text->setString(ui);
    Utility::CentreOrigin(*m_info_text);
}

void JoinByIpState::Draw()
{
    sf::RenderWindow& window = *GetContext().window;
    window.setView(window.getDefaultView());
    window.draw(m_background_sprite);

    if (m_title_text) window.draw(*m_title_text);
    if (m_info_text) window.draw(*m_info_text);
}

bool JoinByIpState::Update(sf::Time)
{
    return true;
}

bool JoinByIpState::HandleEvent(const sf::Event& event)
{
    if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>())
    {
        if (keyPressed->code == sf::Keyboard::Key::Escape ||
            keyPressed->code == sf::Keyboard::Key::Backspace)
        {
            if (keyPressed->code == sf::Keyboard::Key::Escape)
            {
                if (GetContext().network)
                {
                    GetContext().network->Reset();
                }
                RequestStackPop();
                return false;
            }

            //Backspace on input
            if (!m_ip_input.empty())
            {
                m_ip_input.pop_back();
                RefreshInfoText();
            }
            return false;
        }

        if (keyPressed->code == sf::Keyboard::Key::Enter)
        {
            if (!GetContext().network)
                return false;

            if (m_ip_input.empty())
            {
                m_status_message = "IP is empty.";
                GetContext().sounds->Play(SoundEffect::kError);
                RefreshInfoText();
                return false;
            }

            m_status_message = "Connecting...";
            RefreshInfoText();

            const bool connected = GetContext().network->ConnectToHost(m_ip_input, sf::seconds(2.f));
            if (connected)
            {
                GetContext().sounds->Play(SoundEffect::kPairedPlayer);

                //Pop JoinByIp + Menu, then enter Binding
                RequestStackPop();
                RequestStackPop();
                RequestStackPush(StateID::kBinding);
            }
            else
            {
                m_status_message = "Connect failed: " + GetContext().network->GetLastError();
                GetContext().sounds->Play(SoundEffect::kError);
                RefreshInfoText();
            }

            return false;
        }
    }

    if (const auto* textEntered = event.getIf<sf::Event::TextEntered>())
    {
        const char32_t c = textEntered->unicode;

        //Allow IPv4 characters only
        if ((c >= U'0' && c <= U'9') || c == U'.')
        {
            if (m_ip_input.size() < 21)//Enough for IPv4 text
            {
                m_ip_input.push_back(static_cast<char>(c));
                RefreshInfoText();
            }
            return false;
        }
    }

    return true;
}