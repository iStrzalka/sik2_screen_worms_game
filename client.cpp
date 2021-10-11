#include "client.h"

#include "utility.h"

Client::Client(int argc, char *argv[]) {
    // Parsing arguments.
    std::string server_name, gui_server = "localhost";
    uint32_t server_port = DEFAULT_SERVER_PORT, gui_port = DEFAULT_GUI_PORT;
    player_name = "";

    int opt;
    while ((opt = getopt(argc, argv, "n:p:i:r")) != -1) {
        uint32_t parsed;
        if (optarg == NULL) {
            exit(EXIT_FAILURE);
        }

        switch (opt) {
            case 'n':
                player_name = optarg;
                break;
            case 'p':
                try {
                    parsed = string_to_uint32_t(optarg);
                } catch (UtilityError const &e) {
                    std::cerr << static_cast<char>(opt) << ": " << e.what() << std::endl;
                    exit(EXIT_FAILURE);
                }
                server_port = parsed;
                break;
            case 'i':
                gui_server = optarg;
                break;
            case 'r':
                try {
                    parsed = string_to_uint32_t(optarg);
                } catch (UtilityError const &e) {
                    std::cerr << static_cast<char>(opt) << ": " << e.what() << std::endl;
                    exit(EXIT_FAILURE);
                }
                gui_port = parsed;
                break;
            default:
                std::cerr << "Invalid command\n";
                exit(EXIT_FAILURE);
        }
    }
    if (optind < argc) {
        while (optind < argc) server_name = argv[optind++];
    } else {
        std::cerr << "No server address provided";
        exit(EXIT_FAILURE);
    }

    try {
        validate_playername(player_name);
    } catch (UtilityError const &e) {
        std::cerr << static_cast<char>(opt) << ": " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }

    // Setting up clients' variables.
    session_id = get_timestamp();
    turn_direction = 0;
    next_expected_event_no = 0;
    last_client_info_send = get_timestamp() - 30000;

    // Setting up connections for both server and gui.

    struct addrinfo serverhints = {};
    serverhints.ai_family = AF_UNSPEC;
    serverhints.ai_socktype = SOCK_DGRAM;
    server = Connection(server_name.c_str(), server_port, serverhints);

    server_fd = -1;
    std::string last_error;
    for (addrinfo *p = server.info; p != NULL && server_fd == -1; p = p->ai_next) {
        if ((server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            last_error = "Socket: ";
            continue;
        }
        if (connect(server_fd, p->ai_addr, p->ai_addrlen) == -1) {
            last_error = "Connect: ";
            continue;
        }
    }

    int flags = fcntl(server_fd, F_GETFL, 0) | O_NONBLOCK;
    if (fcntl(server_fd, F_SETFL, flags) < 0) {
        std::cerr << "fcntl server\n";
        exit(EXIT_FAILURE);
    }

    struct addrinfo guihints = {};
    guihints.ai_family = AF_UNSPEC;
    guihints.ai_socktype = SOCK_STREAM;
    gui = Connection(gui_server.c_str(), gui_port, guihints);
    gui_message = "";

    gui_fd = socket(gui.get_family(), SOCK_STREAM, 0);
    if (gui_fd == -1) {
        std::cerr << "Couldn't setup gui socket\n";
        exit(EXIT_FAILURE);
    }
    int flag = 1;
    if (setsockopt(gui_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)) == -1) {
        std::cerr << "Setsockopt\n";
        exit(EXIT_FAILURE);
    }

    polls[0].fd = server_fd;
    polls[0].events = POLLIN;
    polls[0].revents = 0;

    polls[1].fd = gui_fd;
    polls[1].events = POLLIN;
    polls[1].revents = 0;
}

void Client::connect_to_gui() {
    if (connect(gui_fd, (sockaddr *)gui.addr(), sizeof(sockaddr_in6)) != 0) {
        std::cerr << "Couldn't connect to gui\n";
        exit(EXIT_FAILURE);
    }
}

void Client::connect_to_server() {
    if ((connect(server_fd, (sockaddr *)server.addr(), server.len())) != 0) {
        std::cout << "Couldn't connect to server\n";
        exit(1);
    }
}

void Client::run() {
    while (1) {
        for (size_t i = 0; i < 2; i++) polls[i].revents = 0;
        polls[0].events = (!write_to_server) ? POLLIN : (POLLIN | POLLOUT);
        polls[1].events = (!write_to_gui) ? POLLIN : (POLLIN | POLLOUT);

        int ret = poll(polls, 2, -1);
        if (ret <= 0) continue;

        if (polls[0].revents & (POLLIN | POLLERR)) {  // Message from server.
            sockaddr_storage addr;
            socklen_t addr_len = sizeof(sockaddr_storage);
            Codec c(MAX_DATAGRAM_LEN);
            ssize_t dglen = recvfrom(polls[0].fd, c.get_data(), MAX_DATAGRAM_LEN, 0, (sockaddr *)&addr, &addr_len);

            if (dglen == -1) {
                if (errno == 111) {
                    std::cout << "connection with server lost\n";
                    exit(EXIT_FAILURE);
                } else
                    std::cout << "Read error\n";
            } else {
                process_message_from_server(c, dglen);
            }

            polls[0].revents &= ~(POLLIN | POLLERR);
        }
        if ((write_to_server && (polls[0].revents & POLLOUT)) ||
            get_timestamp() - last_client_info_send >= TO_SERVER_TICK) {  // Message to server.
            if (get_timestamp() - last_client_info_send >= TO_SERVER_TICK) {
                last_client_info_send = get_timestamp();
            }
            Codec c;
            c.add_uint64_t(session_id);
            c.add_uint8_t(turn_direction);
            c.add_uint32_t(next_expected_event_no);
            c.add_string(player_name, false);
            write_to_server = false;

            sendto(polls[0].fd, c.get_data(), c.get_len(), 0, (sockaddr *)server.addr(), server.len());
            polls[0].revents &= ~POLLOUT;
        }

        if (polls[1].revents & POLLIN) {  // Message from gui.
            char bufor[500];
            ssize_t len = recv(polls[1].fd, bufor, 500, 0);

            if (len == 0) {
                std::cout << "connection with gui lost\n";
                exit(1);
            }

            for (int i = 0; i < len; ++i) {
                if (bufor[i] == '\n') {
                    if (gui_message == "RIGHT_KEY_DOWN")
                        turn_direction = 1;
                    else if (gui_message == "LEFT_KEY_DOWN")
                        turn_direction = 2;
                    else if (gui_message == "LEFT_KEY_UP" || gui_message == "RIGHT_KEY_UP")
                        turn_direction = 0;

                    gui_message = "";
                } else if (bufor[i] != '\0' && gui_message.length() < 30)
                    gui_message += bufor[i];
            }

            polls[1].revents &= ~(POLLIN | POLLERR);
        }
        if (write_to_gui || !gui_commands.empty()) {  // Message to gui.
            size_t len = send(polls[1].fd, gui_commands.front().c_str(), gui_commands.front().length(), 0);
            write_to_gui = false;
            if (len == 0) {
                std::cerr << "connection to gui lost\n";
                exit(EXIT_FAILURE);
            } else if (len < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    write_to_gui = true;
                } else {
                    std::cerr << "gui write error\n";
                    exit(EXIT_FAILURE);
                }
            } else {
                gui_commands.pop();
                write_to_gui = !gui_commands.empty();
            }
        }
    }
}

void Client::process_message_from_server(Codec &c, ssize_t dglen) {
    c.trim(dglen);
    uint32_t server_game_id = c.read_uint32_t();
    bool no_crc_error;
    if ((active_round && (server_game_id != game_id)) || (!active_round && (server_game_id == game_id))) {
        // wrong game id.
    } else {
        while (c.has_data()) {
            uint32_t len = c.read_uint32_t();

            no_crc_error = check_crc32((char *)c.get_pos() - 4, len + 4, c.read_crc(len));
            if (!no_crc_error) {
                break;
            }
            uint32_t event_no = c.read_uint32_t();

            char event_type = c.read_uint8_t();
            if (next_expected_event_no == event_no) {
                if (event_type == 0) {
                    new_game(c, event_no, len, server_game_id);
                } else if (event_type == 1) {
                    pixel(c, event_no);
                } else if (event_type == 2 && active_round) {
                    player_eliminated(c, event_no);
                } else if (event_type == 3 && active_round) {
                    game_over();
                } else {
                    // pass;
                }
            } else {
                c.skip_pos(len - 5);
            }
            c.read_uint32_t();
        }
    }
}

void Client::new_game(Codec &c, uint32_t &event_no, uint32_t &len, uint32_t &server_game_id) {
    if (event_no == 0 && !active_round) {
        maxx = c.read_uint32_t();
        maxy = c.read_uint32_t();
        if (maxx > MAX_WIDTH || maxx < MIN_WIDTH || maxy > MAX_HEIGHT || maxy < MIN_HEIGHT) {
            std::cerr << "Incorrect game size\n";
            exit(EXIT_FAILURE);
        }
        std::stringstream ss;
        ss << " NEW_GAME " << maxx << " " << maxy;
        uint32_t parsed_data = 13;  // len(event_no + event_type + maxx + maxy)
        while (parsed_data < len) {
            std::string name = c.read_string(true);
            validate_playername(name);
            ss << " " << name;
            game_names.push_back(name);
            parsed_data += name.length() + 1;
        }
        ss << "\n";

        gui_commands.push(ss.str());
        polls[1].events |= POLLOUT;
        game_id = server_game_id;
        next_expected_event_no = event_no + 1;
        active_round = true;
    }
}

void Client::pixel(Codec &c, uint32_t &event_no) {
    uint8_t player_number = c.read_uint8_t();
    uint32_t x = c.read_uint32_t();
    uint32_t y = c.read_uint32_t();
    if (x > maxx || y > maxy) {
        std::cout << "Player out of map";
        exit(EXIT_FAILURE);
    }
    if (player_number >= game_names.size()) {
        std::cout << "Wrong player number";
        exit(EXIT_FAILURE);
    }

    std::stringstream ss;
    ss << " PIXEL " << x << " " << y << " " << game_names[player_number] << "\n";
    gui_commands.push(ss.str());
    polls[1].events |= POLLOUT;
    next_expected_event_no = event_no + 1;
}

void Client::player_eliminated(Codec &c, uint32_t &event_no) {
    uint8_t player_number = c.read_uint8_t();
    if (player_number >= game_names.size()) {
        std::cout << "Wrong player number";
        exit(EXIT_FAILURE);
    }
    std::stringstream ss;
    ss << " PLAYER_ELIMINATED " << game_names[player_number] << "\n";
    gui_commands.push(ss.str());
    polls[1].events |= POLLOUT;
    next_expected_event_no = event_no + 1;
}

void Client::game_over() {
    active_round = false;
    next_expected_event_no = 0;
}