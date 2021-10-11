#include "server.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

Server::Server(int argc, char *argv[]) {
    /* Parsing arguments */
    uint32_t port = DEFAULT_PORT, seed = static_cast<uint32_t>(time(NULL) % Generator::MOD);
    int opt;
    while ((opt = getopt(argc, argv, "p:s:t:v:w:h:")) != -1) {
        uint32_t parsed;
        if (optarg == NULL) {
            std::cerr << "No argument given\n";
            exit(EXIT_FAILURE);
        }
        try {
            parsed = string_to_uint32_t(optarg);
        } catch (UtilityError const &e) {
            std::cerr << static_cast<char>(opt) << ": " << e.what() << " '" << optarg << "'\n";
            exit(EXIT_FAILURE);
        }
        switch (opt) {
            case 'p':
                port = parsed;
                break;
            case 's':
                seed = parsed;
                break;
            case 't':
                turning_speed = parsed;
                break;
            case 'v':
                rounds_per_sec = parsed;
                break;
            case 'w':
                maxx = parsed;
                break;
            case 'h':
                maxy = parsed;
                break;
            default:
                std::cerr << "Usage " << argv[0] << " [-p n] [-s n] [-t n] [-v n] [-w n] [-h n]\n";
                exit(EXIT_FAILURE);
                break;
        }
    }

    if (optind < argc) {
        std::cerr << "Usage " << argv[0] << " [-p n] [-s n] [-t n] [-v n] [-w n] [-h n]\n";
        exit(EXIT_FAILURE);
    }

    /* Purpose specific validation of arguments */

    if (port > MAX_PORT) {
        std::cerr << "Incorrect port number\n";
        exit(EXIT_FAILURE);
    }

    if (rounds_per_sec < MIN_ROUNDS_PER_SEC || rounds_per_sec > MAX_ROUNDS_PER_SEC) {
        std::cerr << "Incorrect game speed rate\n";
        exit(EXIT_FAILURE);
    }

    if (turning_speed >= 360 || turning_speed == 0) {
        std::cerr << "Turning speed should be in [1, 360) range\n";
        exit(EXIT_FAILURE);
    }

    if (maxx > MAX_WIDTH || maxx < MIN_WIDTH || maxy > MAX_HEIGHT || maxy < MIN_HEIGHT) {
        std::cerr << "Incorrect game size\n";
        exit(EXIT_FAILURE);
    }

    gen.set_seed(seed);

    /* Setting up connections. */
    fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd == -1) {
        std::cout << "couldn't create socket\n";
        exit(EXIT_FAILURE);
    }

    sockaddr_in6 ip;
    memset(&ip, 0, sizeof(sockaddr_in6));
    ip.sin6_family = AF_INET6;
    ip.sin6_port = htobe16(port);
    ip.sin6_addr = in6addr_any;

    if (bind(fd, (sockaddr *)&ip, sizeof(ip)) < 0) {
        std::cout << "cannot bind on port \n";
        exit(EXIT_FAILURE);
    }

    polls[0].fd = fd;
    polls[0].events = POLLIN | POLLERR;
    polls[0].revents = 0;

    last_game_tick = 0;
    last_check_timestamp = 0;
}

void Server::run() {
    while (true) {
        polls[0].events = (!active_game && events_to_send.empty()) ? POLLIN : (POLLIN | POLLOUT);
        polls[0].revents = 0;
        int ret = poll(polls, 1, -1);
        if (ret <= 0) {
            continue;
        }

        /* Check for inactive players and disconnect them if possible. */
        if ((!active_game && (get_timestamp() - last_check_timestamp >= INACTIVE_CHECK_USEC)) ||
            (active_game && (get_timestamp() - last_game_tick >= SEC_TO_USEC / rounds_per_sec))) {
            auto now = get_timestamp();
            for (auto itr = clients.begin(); itr != clients.end();) {
                if (now - (*itr).second->get_last_message_time() > INACTIVE_TIMEOUT_USEC) {
                    lobby.remove(itr->second);

                    itr->second->set_turn_direction(0);
                    itr->second->set_disconnected(true);
                    itr = clients.erase(itr);
                } else
                    ++itr;
            }
            last_check_timestamp = now;
        }

        /* Process message from client. */
        if (polls[0].revents & (POLLIN | POLLERR)) {
            Connection client_conn;
            Codec p(128);
            ssize_t dglen =
                recvfrom(polls[0].fd, p.get_data(), 128, 0, (sockaddr *)client_conn.addr(), client_conn.len_ptr());
            if (dglen == -1) {
                std::cout << "Read error\n";
            } else {
                process_message_from_client(client_conn, p, dglen);
            }

            if (!active_game && lobby.size() >= MIN_PLAYERS_REQUIRED) {
                if (players_are_ready()) {
                    restart_game();
                }
            }

            polls[0].revents &= ~(POLLIN | POLLERR);
        }

        /* Send message to clients if needed. */
        if (!events_to_send.empty()) {
            auto len =
                sendto(polls[0].fd, events_to_send.front().second->get_pos(), events_to_send.front().second->get_len(),
                       0, (sockaddr *)events_to_send.front().first.addr(), events_to_send.front().first.len());
            if (len >= 0 || (errno != EWOULDBLOCK && errno != EAGAIN)) {
                events_to_send.pop();
            }
        }

        /* Perform round tick */
        if (active_game && get_timestamp() - last_game_tick >= SEC_TO_USEC / rounds_per_sec) {
            round_tick();
            last_game_tick = get_timestamp();
        }
    }
}

void Server::process_message_from_client(Connection address, Codec p, ssize_t dglen) {
    p.trim(dglen);
    if (dglen >= 8 + 1 + 4) {
        uint64_t session_id = p.read_uint64_t();
        uint8_t turn_direction = p.read_uint8_t();
        uint32_t next_expected_event_no = p.read_uint32_t();
        std::string player_name = p.read_string(false);
        try {
            validate_playername(player_name);
        } catch (UtilityError const &e) {
            return;
        }

        auto itr = clients.find(address);
        if (itr == clients.end()) {
            // New client.
            bool incorrect_nick = false;

            for (auto &player : clients)
                if (player.second->get_name().length() > 0 && player.second->get_name() == player_name)
                    incorrect_nick = true;

            if (!incorrect_nick) {
                auto player =
                    std::make_shared<Player>(address, player_name, session_id, next_expected_event_no, turn_direction);
                player->update_last_message_time(get_timestamp());
                clients.insert(std::make_pair(address, player));

                if (player_name.length() > 0) {
                    lobby.push_back(player);
                }

                send_message_to_player(player);
                if (turn_direction != 0) {
                    player->set_ready(true);
                }
            }
        } else {
            // Already connected client.
            if (itr->second->get_session_id() == session_id) {
                // Update clients information.
                itr->second->set_turn_direction(turn_direction);
                itr->second->set_next_expected_event_no(next_expected_event_no);
                itr->second->update_last_message_time(get_timestamp());
                if (turn_direction != 0 && !itr->second->get_ready()) {
                    itr->second->set_ready(true);
                }
            } else if (itr->second->get_session_id() < session_id) {
                // Incorrect session id for this game, sending player to lobby.
                lobby.remove(itr->second);
                active_players.remove(itr->second);
                clients.erase(itr);

                auto player =
                    std::make_shared<Player>(address, player_name, session_id, next_expected_event_no, turn_direction);
                clients.insert(std::make_pair(address, player));

                if (player_name.length() > 0) {
                    lobby.push_back(player);
                }

                send_message_to_player(player);
                if (turn_direction != 0) player->set_ready(true);
            }
        }
    }
}

void Server::round_tick() {
    for (auto itr = active_players.begin(); itr != active_players.end();) {
        Player &p = **itr;
        if (p.get_disconnected()) {
            player_eliminated(p);
            itr = active_players.erase(itr);
            if (active_players.size() == WINNING_PLAYERS) {
                for (auto player : lobby) {
                    (*player).set_ready(false);
                }
                game_over();
                return;
            }
        } else if (p.move(turning_speed)) {
            if (collision(p) || out_of_map(p)) {
                player_eliminated(p);
                lobby.push_back(*itr);
                itr = active_players.erase(itr);

                if (active_players.size() == WINNING_PLAYERS) {
                    for (auto player : lobby) {
                        (*player).set_ready(false);
                    }
                    game_over();
                    return;
                }
            } else {
                pixel(p);
                itr++;
            }
        } else {
            itr++;
        }
    }
}

bool Server::players_are_ready() {
    for (auto player : lobby) {
        if (!player->get_ready()) {
            return false;
        }
    }
    return true;
}

/* Checks for collision of the player. Returns true on collision, false otherwise. */
bool Server::collision(Player &p) {
    std::pair<uint32_t, uint32_t> position{p.get_x(), p.get_y()};
    return used_pixels.find(position) != used_pixels.end();
}

/* Checks whether player is out of the map. Returns true if player is out of the map, false otherwise */
bool Server::out_of_map(Player &p) {
    return floor(p.get_x()) >= maxx || floor(p.get_x()) < 0 || floor(p.get_y()) >= maxy || floor(p.get_y()) < 0;
    ;
}

void Server::pixel(Player &p) {
    auto event = std::make_shared<Codec>();
    event->add_uint32_t(4 + 1 + 1 + 4 + 4);  // len
    event->add_uint32_t(events.size());
    event->add_uint8_t(1);
    event->add_uint8_t(p.get_game_id());
    event->add_uint32_t(floor(p.get_x()));
    event->add_uint32_t(floor(p.get_y()));
    event->add_uint32_t(get_crc32(event->get_pos(), 4 + 4 + 1 + 1 + 4 + 4));

    std::pair<uint32_t, uint32_t> position{floor(p.get_x()), floor(p.get_y())};
    used_pixels.insert(position);

    send_message_to_all_players(event);
}

void Server::player_eliminated(Player &p) {
    auto event = std::make_shared<Codec>();
    event->add_uint32_t(4 + 1 + 1);  // len
    event->add_uint32_t(events.size());
    event->add_uint8_t(2);
    event->add_uint8_t(p.get_game_id());
    event->add_uint32_t(get_crc32(event->get_pos(), 4 + 4 + 1 + 1));

    send_message_to_all_players(event);
}

void Server::game_over() {
    auto event = std::make_shared<Codec>();
    event->add_uint32_t(4 + 1);  // len
    event->add_uint32_t(events.size());
    event->add_uint8_t(3);
    event->add_uint32_t(get_crc32(event->get_pos(), 4 + 4 + 1));

    active_game = false;
    send_message_to_all_players(event);
}

void Server::send_message_to_all_players(std::shared_ptr<Codec> event) {
    events.push_back(event);

    for (auto client : clients) {
        send_message_to_player(client.second);
    }
}

/* Tries to pack as many possible events in one datagram and proceeds to send it to player */
void Server::send_message_to_player(std::shared_ptr<Player> &p) {
    uint32_t client_next_expected_no = p->get_next_expected_event_no();
    while (client_next_expected_no < events.size()) {
        auto event = std::make_shared<Codec>();
        event->add_uint32_t(game_id);
        while (client_next_expected_no < events.size() &&
               event->get_len() + events[client_next_expected_no]->get_len() <= MAX_DATAGRAM_SIZE) {
            event->add(events[client_next_expected_no]->get_pos(), events[client_next_expected_no]->get_len());
            client_next_expected_no++;
        }
        events_to_send.push(std::make_pair(p->get_connection_attr(), event));
        polls[0].revents |= POLLOUT;
    }
}

void Server::restart_game() {
    game_id = gen.next();

    events.clear();
    used_pixels.clear();
    active_players.clear();
    lobby.remove_if([](std::shared_ptr<Player> const &p) { return p->get_disconnected(); });
    lobby.sort([](const std::shared_ptr<Player> &p, const std::shared_ptr<Player> &p2) {
        return p->get_name() < p2->get_name();
    });

    uint32_t len = 0;
    for (auto &p : lobby) {
        len += p->get_name().length() + 1;
    }

    auto event = std::make_shared<Codec>();
    event->add_uint32_t(4 + 1 + 4 + 4 + len);
    event->add_uint32_t(events.size());
    event->add_uint8_t(0);
    event->add_uint32_t(maxx);
    event->add_uint32_t(maxy);

    uint8_t player_id = 0;
    for (auto &p : lobby) {
        uint32_t new_x = gen.next() % maxx;
        uint32_t new_y = gen.next() % maxy;
        uint32_t new_rotation = gen.next() % 360;
        p->set_new_position(new_x + 0.5, new_y + 0.5);
        p->set_rotation(new_rotation);
        p->set_game_id(player_id++);
        active_players.push_back(p);
        event->add_string(p->get_name(), true);
    }

    for (auto client : clients) {
        client.second->set_next_expected_event_no(0);
    }
    event->add_uint32_t(get_crc32(event->get_pos(), 4 + 4 + 1 + 4 + 4 + len));

    send_message_to_all_players(event);

    for (auto &p : lobby) {
        if (collision(*p)) {
            player_eliminated(*p);
        } else {
            pixel(*p);
        }
    }

    lobby.clear();
    active_game = true;
    last_game_tick = get_timestamp();
}