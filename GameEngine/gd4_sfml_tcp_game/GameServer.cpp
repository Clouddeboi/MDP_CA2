#include "GameServer.hpp"
#include "NetworkProtocol.hpp"
#include "AircraftType.hpp"
#include "Utility.hpp"
#include <SFML/Network/Packet.hpp>
#include <SFML/System/Sleep.hpp>
#include "PickupType.hpp"
#include <iostream>

GameServer::GameServer(sf::Vector2f battlefield_size)
    : m_thread()
    , m_listening_state(false)
    , m_client_timeout(sf::seconds(120.f))
    , m_max_connected_players(20)
    , m_connected_players(0)
    , m_world_height(5000.f)
    , m_battlefield_rect(sf::Vector2f(0.f, m_world_height - battlefield_size.y), sf::Vector2f(battlefield_size.x, battlefield_size.y))
    , m_battlefield_scrollspeed(-50.f)
    , m_aircraft_count(0)
    , m_peers(1)
    , m_aircraft_identifier_counter(1)
    , m_waiting_thread_end(false)
    , m_last_spawn_time(sf::Time::Zero)
    , m_time_for_next_spawn(sf::seconds(5.f))
{
    m_listener_socket.setBlocking(false);
    m_peers[0].reset(new RemotePeer);

    //Register the host's own aircraft as ID 0
    //m_aircraft_identifier_counter starts at 1, so client IDs won't conflict
    m_aircraft_info[0].m_position = sf::Vector2f(
        m_battlefield_rect.size.x / 2.f,
        m_battlefield_rect.position.y + m_battlefield_rect.size.y / 2.f
    );
    m_aircraft_info[0].m_hitpoints = 100;
    m_aircraft_info[0].m_missile_ammo = 2;
    m_aircraft_count = 1;

    m_thread = std::thread(&GameServer::ExecutionThread, this);
}

GameServer::~GameServer()
{
    m_waiting_thread_end = true;
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

void GameServer::NotifyPlayerSpawn(uint8_t aircraft_identifier)
{
    sf::Packet packet;
    //First thing in every is what type of packet it is
    packet << static_cast<uint8_t>(Server::PacketType::kPlayerConnect);
    packet << aircraft_identifier << m_aircraft_info[aircraft_identifier].m_position.x << m_aircraft_info[aircraft_identifier].m_position.y;
    SendToAll(packet);
}

void GameServer::NotifyPlayerRealtimeChange(uint8_t aircraft_identifier, uint8_t action, bool action_enabled)
{
    sf::Packet packet;
    //First thing in every is what type of packet it is
    packet << static_cast<uint8_t>(Server::PacketType::kPlayerRealtimeChange);
    packet << aircraft_identifier;
    packet << action;
    packet << action_enabled;
    SendToAll(packet);

}

void GameServer::NotifyPlayerEvent(uint8_t aircraft_identifier, uint8_t action)
{
    sf::Packet packet;
    std::cout << "Server: Notify Player Event" << +aircraft_identifier << +action << std::endl;
    //First thing in every is what type of packet it is
    packet << static_cast<uint8_t>(Server::PacketType::kPlayerEvent);
    packet << aircraft_identifier;
    packet << action;
    SendToAll(packet);
}

void GameServer::SetListening(bool enable)
{
    //Check if the server is already listening
    if (enable)
    {
        if (!m_listening_state)
        {
            m_listening_state = (m_listener_socket.listen(SERVER_PORT) == sf::TcpListener::Status::Done);
        }
    }
    else
    {
        m_listener_socket.close();
        m_listening_state = false;
    }
}

void GameServer::ExecutionThread()
{
    //Initialisation
    SetListening(true);

    sf::Time frame_rate = sf::seconds(1.f / 60.f);
    sf::Time frame_time = sf::Time::Zero;
    sf::Time tick_rate = sf::seconds(1.f / 30.f);
    sf::Time tick_time = sf::Time::Zero;
    sf::Clock frame_clock, tick_clock;

    while (!m_waiting_thread_end)
    {
        //This is the game loop
        HandleIncomingConnections();
        HandleIncomingPackets();

        frame_time += frame_clock.getElapsedTime();
        frame_clock.restart();
        tick_time += tick_clock.getElapsedTime();
        tick_clock.restart();

        //Fixed time step
        while (frame_time >= frame_rate)
        {
            m_battlefield_rect.position.y += m_battlefield_scrollspeed * frame_rate.asSeconds();
            frame_time -= frame_rate;
        }

        while (tick_time >= tick_rate)
        {
            Tick();
            tick_time -= tick_rate;
        }

        //sleep to allow me to run the client on this machine as well
        //maybe rethink this if performance is poor
        sf::sleep(sf::milliseconds(1));
    }
}

void GameServer::Tick()
{
    UpdateClientState();

    //Check if the game is over = all planes position.y < offset
    bool all_aircraft_done = true;
    for (const auto& current : m_aircraft_info)
    {
        //As long as one player has not crossed the finish line the game is live
        if (current.second.m_position.y > 0.f)
        {
            all_aircraft_done = false;
            break;
        }
    }
    if (all_aircraft_done)
    {
        sf::Packet mission_success_packet;
        mission_success_packet << static_cast<uint8_t>(Server::PacketType::kMissionSuccess);
        SendToAll(mission_success_packet);
    }

    //Remove aircraft that have been destroyed
    for (auto itr = m_aircraft_info.begin(); itr != m_aircraft_info.end();)
    {
        if (itr->second.m_hitpoints <= 0)
        {
            m_aircraft_info.erase(itr++);
        }
        else
        {
            ++itr;
        }
    }

    //Check if it is time to spawn enemies
    if (Now() >= m_time_for_next_spawn + m_last_spawn_time)
    {
        //Not going to spawn any enemies towards the end of the level
        if (m_battlefield_rect.position.y > 600.f)
        {
            std::size_t enemy_count = 1 + Utility::RandomInt(2);
            float spawn_centre = static_cast<float>(Utility::RandomInt(500) - 250);

            //If there is only one enemy it will spawn in centre
            float plane_distance = 0.f;
            float next_spawn_position = spawn_centre;

            //If there are two enemies they are centred on the spawncentre
            if (enemy_count == 2)
            {
                plane_distance = static_cast<float>(150 + Utility::RandomInt(250));
                next_spawn_position = spawn_centre - plane_distance / 2.f;
            }

            //Send the spawn packets to the clients
            for (std::size_t i = 0; i < enemy_count; ++i)
            {
                sf::Packet packet;
                packet << static_cast<uint8_t>(Server::PacketType::kSpawnEnemy);
                packet << static_cast<uint8_t>(1 + Utility::RandomInt(static_cast<int>(AircraftType::kAircraftCount) - 1));
                packet << m_world_height - m_battlefield_rect.position.y + 500;
                packet << next_spawn_position;

                next_spawn_position += plane_distance / 2.f;
                SendToAll(packet);
            }
            m_last_spawn_time = Now();
            m_time_for_next_spawn = sf::milliseconds(2000 + Utility::RandomInt(4000));
        }
    }
}

sf::Time GameServer::Now() const
{
    return m_clock.getElapsedTime();
}

std::size_t GameServer::GetConnectedPlayerCount() const
{
    return m_connected_players;
}

void GameServer::HandleIncomingPackets()
{
    bool detected_timeout = false;

    for (PeerPtr& peer : m_peers)
    {
        if (peer->m_ready)
        {
            while (true)
            {
                sf::Packet packet;
                const auto status = peer->m_socket.receive(packet);

                if (status == sf::Socket::Status::Done)
                {
                    //Interpret the packet and react to it
                    HandleIncomingPackets(packet, *peer, detected_timeout);
                    peer->m_last_packet_time = Now();
                }
                else if (status == sf::Socket::Status::Disconnected)
                {
                    //Immediate disconnect handling (no timeout wait)
                    peer->m_timed_out = true;
                    detected_timeout = true;
                    break;
                }
                else
                {
                    break;
                }
            }

            if (!peer->m_timed_out && Now() > peer->m_last_packet_time + m_client_timeout)
            {
                peer->m_timed_out = true;
                detected_timeout = true;
            }
        }
    }

    if (detected_timeout)
    {
        HandleDisconnections();
    }
}

void GameServer::HandleIncomingPackets(sf::Packet& packet, RemotePeer& receiving_peer, bool& detected_timeout)
{
    uint8_t packet_type;
    packet >> packet_type;

    switch (static_cast<Client::PacketType>(packet_type))
    {
    case Client::PacketType::kQuit:
    {
        receiving_peer.m_timed_out = true;
        detected_timeout = true;
    }
    break;

    case Client::PacketType::kPlayerEvent:
    {
        uint8_t aircraft_identifier;
        uint8_t action;
        packet >> aircraft_identifier >> action;
        NotifyPlayerEvent(aircraft_identifier, action);
    }
    break;

    case Client::PacketType::kPlayerRealtimeChange:
    {
        uint8_t aircraft_identifier;
        uint8_t action;
        bool action_enabled;
        packet >> aircraft_identifier >> action >> action_enabled;
        NotifyPlayerRealtimeChange(aircraft_identifier, action, action_enabled);
    }
    break;

    case Client::PacketType::kRequestCoopPartner:
    {
        receiving_peer.m_aircraft_identifiers.emplace_back(m_aircraft_identifier_counter);
        m_aircraft_info[m_aircraft_identifier_counter].m_position = sf::Vector2f(m_battlefield_rect.size.x / 2, m_battlefield_rect.position.y + m_battlefield_rect.size.y / 2);
        m_aircraft_info[m_aircraft_identifier_counter].m_hitpoints = 100;
        m_aircraft_info[m_aircraft_identifier_counter].m_missile_ammo = 2;

        sf::Packet request_packet;
        request_packet << static_cast<uint8_t>(Server::PacketType::kAcceptCoopPartner);
        request_packet << m_aircraft_identifier_counter;
        request_packet << m_aircraft_info[m_aircraft_identifier_counter].m_position.x;
        request_packet << m_aircraft_info[m_aircraft_identifier_counter].m_position.y;

        receiving_peer.m_socket.send(request_packet);
        m_aircraft_count++;

        sf::Packet notify_packet;
        notify_packet << static_cast<uint8_t>(Server::PacketType::kPlayerConnect);
        notify_packet << m_aircraft_identifier_counter;
        notify_packet << m_aircraft_info[m_aircraft_identifier_counter].m_position.x;
        notify_packet << m_aircraft_info[m_aircraft_identifier_counter].m_position.y;

        for (PeerPtr& peer : m_peers)
        {
            if (peer.get() != &receiving_peer && peer->m_ready)
            {
                peer->m_socket.send(notify_packet);
            }
        }
        m_aircraft_identifier_counter++;
    }
    break;

    case Client::PacketType::kStateUpdate:
    {
        uint8_t num_aircraft;
        packet >> num_aircraft;

        std::scoped_lock lock(m_aircraft_mutex);
        for (uint8_t i = 0; i < num_aircraft; ++i)
        {
            uint8_t aircraft_identifier;
            sf::Vector2f aircraft_position;
            sf::Vector2f aircraft_velocity;
            uint8_t aircraft_hitpoints;
            uint8_t missile_ammo;
            uint8_t anim;
            packet >> aircraft_identifier
                >> aircraft_position.x >> aircraft_position.y
                >> aircraft_velocity.x >> aircraft_velocity.y
                >> aircraft_hitpoints >> missile_ammo >> anim;

            m_aircraft_info[aircraft_identifier].m_position = aircraft_position;
            m_aircraft_info[aircraft_identifier].m_velocity = aircraft_velocity;
            m_aircraft_info[aircraft_identifier].m_hitpoints = aircraft_hitpoints;
            m_aircraft_info[aircraft_identifier].m_missile_ammo = missile_ammo;
            m_aircraft_info[aircraft_identifier].m_anim = anim;
        }
    }
    break;
    case Client::PacketType::kLobbyBindingState:
    {
        std::uint8_t clientSentIndex = 0;
        std::int32_t color = -1;
        bool ready = false;
        packet >> clientSentIndex >> color >> ready;

        const int playerIndex = (receiving_peer.m_lobby_player_index >= 0) ? receiving_peer.m_lobby_player_index : 1;

        {
            std::scoped_lock lock(m_lobby_mutex);
            m_lobby_binding_events.emplace_back(playerIndex, static_cast<int>(color), ready);
            m_lobby_state[playerIndex] = std::make_tuple(static_cast<int>(color), ready, true);
        }

        BroadcastLobbyBindingState(static_cast<std::uint8_t>(playerIndex), static_cast<int>(color), ready);
        BroadcastLobbySnapshot();
    }
    break;
    case Client::PacketType::kLobbyStartGameRequest:
    {
        std::scoped_lock lock(m_lobby_mutex);
        m_client_start_requested = true;
    }
    break;
    case Client::PacketType::kGameEvent:
    {
        uint8_t action;
        float x;
        float y;

        packet >> action;
        packet >> x;
        packet >> y;

        //Enemy explodes, with a certain probability, drop a pickup
        //To avoid multiple messages only listen to the first peer (host)
        if (action == GameActions::kEnemyExplode && Utility::RandomInt(3) == 0 && &receiving_peer == m_peers[0].get())
        {
            sf::Packet packet;
            packet << static_cast<uint8_t>(Server::PacketType::kSpawnPickup);
            packet << static_cast<uint8_t>(Utility::RandomInt(static_cast<int>(PickupType::kPickupCount)));
            packet << x;
            packet << y;

            SendToAll(packet);
        }
    }
    break;
    case Client::PacketType::kPlayerColorSync:
    {
        std::uint8_t id = 0, r = 255, g = 255, b = 255;
        packet >> id >> r >> g >> b;

        {
            std::scoped_lock lock(m_aircraft_mutex);
            m_aircraft_info[id].m_color_r = r;
            m_aircraft_info[id].m_color_g = g;
            m_aircraft_info[id].m_color_b = b;
        }

        sf::Packet out;
        out << static_cast<uint8_t>(Server::PacketType::kPlayerColorSync)
            << id << r << g << b;
        SendToAll(out);

        HostEvent ev;
        ev.type = HostEvent::kColorSync;
        ev.aircraft_id = id;
        ev.r = r; ev.g = g; ev.b = b;
        PushHostEvent(ev);
    }
    break;
    case Client::PacketType::kPlayerNameSync:
    {
        std::int32_t id = 0;
        std::string name;
        packet >> id >> name;

        HostEvent ev;
        ev.type = HostEvent::kNameSync;
        ev.aircraft_id = static_cast<uint8_t>(id);
        ev.name = name;
        PushHostEvent(ev);

        sf::Packet out;
        out << static_cast<uint8_t>(Server::PacketType::kPlayerNameSync)
            << static_cast<uint8_t>(id) << name;
        SendToAll(out);
    }
    break;
    case Client::PacketType::kLobbyLeave:
    {
        std::uint8_t clientSentIndex = 0;
        packet >> clientSentIndex;

        const int playerIndex = (receiving_peer.m_lobby_player_index >= 0) ? receiving_peer.m_lobby_player_index : 1;

        {
            std::scoped_lock lock(m_lobby_mutex);
            m_lobby_leave_events.push_back(playerIndex);
            m_lobby_state[playerIndex] = std::make_tuple(-1, false, false);
        }

        BroadcastLobbyPlayerLeft(static_cast<std::uint8_t>(playerIndex));
        BroadcastLobbySnapshot();
    }
    break;
    case Client::PacketType::kFireProjectile:
    {
        uint8_t ownerId;
        float x, y, vx, vy;
        packet >> ownerId >> x >> y >> vx >> vy;

        BroadcastProjectileSpawn(ownerId, x, y, vx, vy);
    }
    break;
    }
}

void GameServer::HandleIncomingConnections()
{
    if (!m_listening_state || m_game_started)
        return;

    if (m_listener_socket.accept(m_peers[m_connected_players]->m_socket) == sf::TcpListener::Status::Done)
    {
        EnsureHostLobbyState();

        //Assign stable lobby index for this peer
        int assignedLobbyIndex = FindFreeLobbyPlayerIndex();
        if (assignedLobbyIndex < 0)
        {
            assignedLobbyIndex = static_cast<int>(m_connected_players + 1);
        }
        m_peers[m_connected_players]->m_lobby_player_index = assignedLobbyIndex;

        m_aircraft_info[m_aircraft_identifier_counter].m_position = sf::Vector2f(m_battlefield_rect.size.x / 2, m_battlefield_rect.position.y + m_battlefield_rect.size.y / 2);
        m_aircraft_info[m_aircraft_identifier_counter].m_hitpoints = 100;
        m_aircraft_info[m_aircraft_identifier_counter].m_missile_ammo = 2;

        sf::Packet packet;
        packet << static_cast<uint8_t>(Server::PacketType::kSpawnSelf);
        packet << m_aircraft_identifier_counter;
        packet << m_aircraft_info[m_aircraft_identifier_counter].m_position.x;
        packet << m_aircraft_info[m_aircraft_identifier_counter].m_position.y;

        m_peers[m_connected_players]->m_aircraft_identifiers.emplace_back(m_aircraft_identifier_counter);

        BroadcastMessage("New player");
        InformWorldState(m_peers[m_connected_players]->m_socket);
        NotifyPlayerSpawn(m_aircraft_identifier_counter++);

        m_peers[m_connected_players]->m_socket.send(packet);
        m_peers[m_connected_players]->m_ready = true;
        m_peers[m_connected_players]->m_last_packet_time = Now();

        //Send assigned index
        sf::Packet assignedPacket;
        assignedPacket << static_cast<std::uint8_t>(Server::PacketType::kLobbyAssignedIndex);
        assignedPacket << static_cast<std::uint8_t>(assignedLobbyIndex);
        m_peers[m_connected_players]->m_socket.send(assignedPacket);

        //Send host aircraft info to the new client
        {
            sf::Packet hostInfoPacket;
            hostInfoPacket << static_cast<uint8_t>(Server::PacketType::kPlayerConnect);
            hostInfoPacket << static_cast<uint8_t>(0)
                << m_aircraft_info[0].m_position.x
                << m_aircraft_info[0].m_position.y;
            m_peers[m_connected_players]->m_socket.send(hostInfoPacket);
        }

        {
            sf::Packet sync;
            sync << static_cast<uint8_t>(Server::PacketType::kUpdateClientState);

            sync << m_battlefield_rect.position.y + m_battlefield_rect.size.y;
            sync << static_cast<uint8_t>(1);

            const auto& h = m_aircraft_info[0];

            sync << static_cast<uint8_t>(0)
                << h.m_position.x << h.m_position.y
                << h.m_velocity.x << h.m_velocity.y
                << h.m_hitpoints
                << h.m_missile_ammo
                << h.m_anim;

            m_peers[m_connected_players]->m_socket.send(sync);
        }

        //Notify host about this new client
        {
            uint8_t newClientId = static_cast<uint8_t>(m_aircraft_identifier_counter - 1);
            PushHostEvent({
                HostEvent::kConnect,
                newClientId,
                m_aircraft_info[newClientId].m_position.x,
                m_aircraft_info[newClientId].m_position.y
            });
        }

        {
            std::scoped_lock lock(m_lobby_mutex);
            m_lobby_state[assignedLobbyIndex] = std::make_tuple(assignedLobbyIndex, false, true);
        }
        BroadcastLobbySnapshot();

        {
            std::scoped_lock lock(m_aircraft_mutex);
            for (const auto& kv : m_aircraft_info)
            {
                sf::Packet cp;
                cp << static_cast<uint8_t>(Server::PacketType::kPlayerColorSync)
                   << kv.first
                   << kv.second.m_color_r
                   << kv.second.m_color_g
                   << kv.second.m_color_b;
                m_peers[m_connected_players]->m_socket.send(cp);
            }
        }

        {
            std::scoped_lock lock(m_aircraft_mutex);
            for (const auto& kv : m_aircraft_info)
            {
                if (!kv.second.m_player_name.empty())
                {
                    sf::Packet namePacket;
                    namePacket << static_cast<uint8_t>(Server::PacketType::kPlayerNameSync);
                    namePacket << static_cast<std::int32_t>(kv.first) << kv.second.m_player_name;
                    m_peers[m_connected_players]->m_socket.send(namePacket);
                }
            }
        }

        BroadcastAllNames();
        BroadcastAllColors();

        m_aircraft_count++;
        m_connected_players++;

        if (m_connected_players >= m_max_connected_players)
        {
            SetListening(false);
        }
        else
        {
            m_peers.emplace_back(PeerPtr(new RemotePeer()));
        }
    }
}

void GameServer::HandleDisconnections()
{
    for (auto itr = m_peers.begin(); itr != m_peers.end();)
    {
        if ((*itr)->m_timed_out)
        {
            //Best-effort resolve index from peer aircraft id list, fallback to 1
            int leftIndex = ((*itr)->m_lobby_player_index >= 0) ? (*itr)->m_lobby_player_index : 1;

            //Mark lobby leave + authoritative disconnected state
            {
                std::scoped_lock lock(m_lobby_mutex);
                m_lobby_leave_events.push_back(leftIndex);
                m_lobby_state[leftIndex] = std::make_tuple(-1, false, false);
            }

            //Notify all peers
            BroadcastLobbyPlayerLeft(static_cast<std::uint8_t>(leftIndex));
            BroadcastLobbySnapshot();

            //Inform everyone of gameplay disconnection and erase aircraft
            for (uint8_t identifer : (*itr)->m_aircraft_identifiers)
            {
                SendToAll((sf::Packet() << static_cast<uint8_t>(Server::PacketType::kPlayerDisconnect) << identifer));
                m_aircraft_info.erase(identifer);

                //Notify host about disconnection
                PushHostEvent({ HostEvent::kDisconnect, identifer, 0.f, 0.f });
            }

            m_connected_players--;
            m_aircraft_count -= (*itr)->m_aircraft_identifiers.size();

            itr = m_peers.erase(itr);

            //If number of peers dropped below max connections, open a slot again
            if (m_connected_players < m_max_connected_players)
            {
                m_peers.emplace_back(PeerPtr(new RemotePeer()));
                if (!m_game_started)
                    SetListening(true);
            }

            BroadcastMessage("A player has disconnected");
        }
        else
        {
            ++itr;
        }
    }
}

void GameServer::InformWorldState(sf::TcpSocket& socket)
{
    sf::Packet packet;
    packet << static_cast<uint8_t>(Server::PacketType::kInitialState);
    packet << m_world_height << m_battlefield_rect.position.y + m_battlefield_rect.size.y;
    packet << static_cast<uint8_t>(m_aircraft_count);

    for (std::size_t i = 0; i < m_connected_players; ++i)
    {
        if (m_peers[i]->m_ready)
        {
            for (uint8_t identifier : m_peers[i]->m_aircraft_identifiers)
            {
                packet << identifier << m_aircraft_info[identifier].m_position.x << m_aircraft_info[identifier].m_position.y << m_aircraft_info[identifier].m_hitpoints << m_aircraft_info[identifier].m_missile_ammo;
            }
        }
    }

    socket.send(packet);
}

void GameServer::BroadcastMessage(const std::string& message)
{
    sf::Packet packet;
    packet << static_cast<uint8_t>(Server::PacketType::kBroadcastMessage);
    packet << message;
    for (std::size_t i = 0; i < m_connected_players; ++i)
    {
        if (m_peers[i]->m_ready)
        {
            m_peers[i]->m_socket.send(packet);
        }
    }
}

void GameServer::SendToAll(sf::Packet& packet)
{
    for (std::size_t i = 0; i < m_connected_players; ++i)
    {
        if (m_peers[i]->m_ready)
        {
            m_peers[i]->m_socket.send(packet);
        }
    }
}

void GameServer::UpdateClientState()
{
    sf::Packet p;
    p << static_cast<uint8_t>(Server::PacketType::kUpdateClientState);
    p << static_cast<float>(m_battlefield_rect.position.y + m_battlefield_rect.size.y);

    std::vector<uint8_t> playerIds;

    if (m_aircraft_info.count(0))
        playerIds.push_back(0);

    for (std::size_t i = 0; i < m_connected_players; ++i)
    {
        if (!m_peers[i]->m_ready) continue;
        for (uint8_t id : m_peers[i]->m_aircraft_identifiers)
        {
            if (m_aircraft_info.find(id) != m_aircraft_info.end())
                playerIds.push_back(id);
        }
    }

    p << static_cast<uint8_t>(playerIds.size());
    for (uint8_t id : playerIds)
    {
        const auto& a = m_aircraft_info[id];
        p << id << a.m_position.x << a.m_position.y << a.m_velocity.x << a.m_velocity.y << a.m_hitpoints << a.m_missile_ammo << a.m_anim;
    }

    SendToAll(p);
}

//It is essential to set the sockets to non-blocking - m_socket.setBlocking(false)
//otherwise the server will hang waiting to read input from a connection
GameServer::RemotePeer::RemotePeer()
    : m_ready(false)
    , m_timed_out(false)
{
    m_socket.setBlocking(false);
}

void GameServer::BroadcastLobbyBindingState(uint8_t playerIndex, int colorIndex, bool ready)
{
    sf::Packet p;
    p << static_cast<uint8_t>(Server::PacketType::kLobbyBindingState);
    p << playerIndex << static_cast<int32_t>(colorIndex) << ready;
    SendToAll(p);
}

void GameServer::BroadcastLobbyStartGame()
{
    m_game_started = true;
    SetListening(false);

    for (std::size_t i = 0; i < m_connected_players; ++i)
    {
        if (!m_peers[i]->m_ready)
            continue;

        sf::Packet p;
        p << static_cast<std::uint8_t>(Server::PacketType::kLobbyStartGame);

        auto status = m_peers[i]->m_socket.send(p);
        int retries = 0;
        while ((status == sf::Socket::Status::Partial || status == sf::Socket::Status::NotReady) && retries < 5)
        {
            sf::sleep(sf::milliseconds(5));
            status = m_peers[i]->m_socket.send(p);
            ++retries;
        }
    }
}

bool GameServer::PollClientLobbyBindingState(int& playerIndex, int& colorIndex, bool& ready)
{
    std::scoped_lock lock(m_lobby_mutex);
    if (m_lobby_binding_events.empty())
        return false;

    auto e = m_lobby_binding_events.front();
    m_lobby_binding_events.pop_front();

    playerIndex = std::get<0>(e);
    colorIndex = std::get<1>(e);
    ready = std::get<2>(e);
    return true;
}

bool GameServer::PollClientStartRequest()
{
    std::scoped_lock lock(m_lobby_mutex);
    bool v = m_client_start_requested;
    m_client_start_requested = false;
    return v;
}

void GameServer::BroadcastLobbyPlayerLeft(std::uint8_t playerIndex)
{
    for (std::size_t i = 0; i < m_connected_players; ++i)
    {
        if (!m_peers[i]->m_ready)
            continue;

        sf::Packet p;
        p << static_cast<std::uint8_t>(Server::PacketType::kLobbyPlayerLeft);
        p << playerIndex;

        auto status = m_peers[i]->m_socket.send(p);
        int retries = 0;
        while ((status == sf::Socket::Status::Partial || status == sf::Socket::Status::NotReady) && retries < 5)
        {
            sf::sleep(sf::milliseconds(5));
            status = m_peers[i]->m_socket.send(p);
            ++retries;
        }
    }
}

bool GameServer::PollClientLeave(int& playerIndex)
{
    std::scoped_lock lock(m_lobby_mutex);
    if (m_lobby_leave_events.empty())
        return false;

    playerIndex = m_lobby_leave_events.front();
    m_lobby_leave_events.pop_front();
    return true;
}

void GameServer::BroadcastLobbySnapshot()
{
    sf::Packet p;
    p << static_cast<std::uint8_t>(Server::PacketType::kLobbySnapshot);

    std::vector<std::pair<int, std::tuple<int, bool, bool>>> snapshot;
    {
        std::scoped_lock lock(m_lobby_mutex);
        for (const auto& kv : m_lobby_state)
        {
            snapshot.push_back(kv);
        }
    }

    p << static_cast<std::uint8_t>(snapshot.size());
    for (const auto& kv : snapshot)
    {
        const int playerIndex = kv.first;
        const int color = std::get<0>(kv.second);
        const bool ready = std::get<1>(kv.second);
        const bool connected = std::get<2>(kv.second);

        p << static_cast<std::uint8_t>(playerIndex);
        p << static_cast<std::int32_t>(color);
        p << ready;
        p << connected;
    }

    SendToAll(p);
}

void GameServer::EnsureHostLobbyState()
{
    std::scoped_lock lock(m_lobby_mutex);
    if (m_lobby_state.find(0) == m_lobby_state.end())
    {
        //Host default state
        m_lobby_state[0] = std::make_tuple(0, false, true);
    }
}

int GameServer::FindFreeLobbyPlayerIndex() const
{
    //0 reserved for host, remotes start at 1
    for (int i = 1; i < static_cast<int>(m_max_connected_players); ++i)
    {
        auto it = m_lobby_state.find(i);
        if (it == m_lobby_state.end())
            return i;

        const bool connected = std::get<2>(it->second);
        if (!connected)
            return i;
    }

    return -1;
}

void GameServer::CopyAircraftStates(std::vector<NetAircraftState>& outStates) const
{
    std::scoped_lock lock(m_aircraft_mutex);
    outStates.clear();
    for (const auto& kv : m_aircraft_info)
    {
        NetAircraftState s;
        s.id = kv.first;
        s.position = kv.second.m_position;
        s.velocity = kv.second.m_velocity;
        s.hp = kv.second.m_hitpoints;
        s.ammo = kv.second.m_missile_ammo;
        s.anim = kv.second.m_anim;
        outStates.push_back(s);
    }
}

void GameServer::UpdateHostAircraftState(const sf::Vector2f& pos, const sf::Vector2f& vel, uint8_t hp, uint8_t ammo, uint8_t anim)
{
    std::scoped_lock lock(m_aircraft_mutex);
    m_aircraft_info[0].m_position = pos;
    m_aircraft_info[0].m_velocity = vel;
    m_aircraft_info[0].m_hitpoints = hp;
    m_aircraft_info[0].m_missile_ammo = ammo;
    m_aircraft_info[0].m_anim = anim;
}

void GameServer::PushHostEvent(const HostEvent& event)
{
    std::scoped_lock lock(m_host_event_mutex);
    m_host_events.push_back(event);
}

bool GameServer::PollHostEvent(HostEvent& outEvent)
{
    std::scoped_lock lock(m_host_event_mutex);
    if (m_host_events.empty())
        return false;

    outEvent = m_host_events.front();
    m_host_events.pop_front();
    return true;
}

void GameServer::SetAircraftColor(uint8_t id, uint8_t r, uint8_t g, uint8_t b)
{
    std::scoped_lock lock(m_aircraft_mutex);
    m_aircraft_info[id].m_color_r = r;
    m_aircraft_info[id].m_color_g = g;
    m_aircraft_info[id].m_color_b = b;
}

void GameServer::BroadcastAllColors()
{
    std::scoped_lock lock(m_aircraft_mutex);
    for (const auto& kv : m_aircraft_info)
    {
        sf::Packet p;
        p << static_cast<uint8_t>(Server::PacketType::kPlayerColorSync);
        p << kv.first
            << kv.second.m_color_r
            << kv.second.m_color_g
            << kv.second.m_color_b;
        SendToAll(p);
    }
}

void GameServer::BroadcastAllNames()
{
    std::scoped_lock lock(m_aircraft_mutex);
    for (const auto& kv : m_aircraft_info)
    {
        sf::Packet p;
        p << static_cast<uint8_t>(Server::PacketType::kPlayerNameSync);
        p << kv.first;
        p << kv.second.m_player_name;
        SendToAll(p);
    }
}

void GameServer::BroadcastProjectileSpawn(uint8_t ownerId, float x, float y, float vx, float vy)
{
    //Broadcast to all connected clients
    sf::Packet p;
    p << static_cast<uint8_t>(Server::PacketType::kSpawnProjectile)
        << ownerId << x << y << vx << vy;
    SendToAll(p);

    //Also push to host game-side via HostEvent
    HostEvent ev;
    ev.type = HostEvent::kSpawnProjectile;
    ev.aircraft_id = ownerId;
    ev.x = x; ev.y = y;
    ev.vx = vx; ev.vy = vy;
    PushHostEvent(ev);
}

void GameServer::BroadcastScores(const std::vector<int>& scores)
{
    sf::Packet packet;
    packet << static_cast<uint8_t>(Server::PacketType::kScoreUpdate);
    packet << static_cast<uint8_t>(scores.size());
    for (int s : scores)
        packet << static_cast<int32_t>(s);
    SendToAll(packet);
}

void GameServer::BroadcastNewRound(uint8_t levelIndex)
{
    sf::Packet packet;
    packet << static_cast<uint8_t>(Server::PacketType::kNewRound);
    packet << levelIndex;
    SendToAll(packet);
}