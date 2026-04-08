#include "MenuState.hpp"
#include "ResourceHolder.hpp"
#include "Utility.hpp"
#include "Button.hpp"
#include "NetworkSession.hpp"

MenuState::MenuState(StateStack& stack, Context context)
    :State(stack, context)
    , m_background_sprite(context.textures->Get(TextureID::kTitleScreen))
{
    auto play_button = std::make_shared<gui::Button>(context);
    play_button->setPosition({ 100, 230 });
    play_button->SetText("Play (Local)");
    play_button->SetCallback([this]()
        {
            RequestStackPop();
            RequestStackPush(StateID::kBinding);
        });

    auto host_button = std::make_shared<gui::Button>(context);
    host_button->setPosition({ 100, 280 });
    host_button->SetText("Host Game");
    host_button->SetCallback([this]()
        {
            auto* network = GetContext().network;
            if (!network)
                return;

            network->Reset();

            const sf::Vector2u windowSize = GetContext().window->getSize();
            const bool started = network->StartHosting(
                { static_cast<float>(windowSize.x), static_cast<float>(windowSize.y) });

            if (started)
            {
                GetContext().sounds->Play(SoundEffect::kButtonClick);
                RequestStackPush(StateID::kHostLobby);
            }
            else
            {
                GetContext().sounds->Play(SoundEffect::kError);
            }
        });

    auto join_button = std::make_shared<gui::Button>(context);
    join_button->setPosition({ 100, 330 });
    join_button->SetText("Join Game");
    join_button->SetCallback([this]()
        {
            auto* network = GetContext().network;
            if (!network)
                return;

            network->Reset();
            network->StartClientMode();

            GetContext().sounds->Play(SoundEffect::kButtonClick);
            RequestStackPush(StateID::kJoinByIp);
        });

    auto settings_button = std::make_shared<gui::Button>(context);
    settings_button->setPosition({ 100, 380 });
    settings_button->SetText("Settings");
    settings_button->SetCallback([this]()
        {
            RequestStackPush(StateID::kSettings);
        });

    auto editor_button = std::make_shared<gui::Button>(context);
    editor_button->setPosition({ 100, 430 });
    editor_button->SetText("Level Editor");
    editor_button->SetCallback([this]()
        {
            RequestStackPop();
            RequestStackPush(StateID::kEditor);
        });

    auto exit_button = std::make_shared<gui::Button>(context);
    exit_button->setPosition({ 100, 480 });
    exit_button->SetText("Exit");
    exit_button->SetCallback([this]()
        {
            RequestStackPop();
        });

    m_gui_container.Pack(play_button);
    m_gui_container.Pack(host_button);
    m_gui_container.Pack(join_button);
    m_gui_container.Pack(settings_button);
    m_gui_container.Pack(editor_button);
    m_gui_container.Pack(exit_button);

    context.music->Play(MusicThemes::kMenuTheme);
}

void MenuState::Draw()
{
    sf::RenderWindow& window = *GetContext().window;
    window.setView(window.getDefaultView());
    window.draw(m_background_sprite);
    window.draw(m_gui_container);
}

bool MenuState::Update(sf::Time)
{
    return true;
}

bool MenuState::HandleEvent(const sf::Event& event)
{
    m_gui_container.HandleEvent(event);
    return true;
}