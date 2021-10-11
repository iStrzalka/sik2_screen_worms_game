#ifndef SIK2_CLIENT
#define SIK2_CLIENT

#include "codec.h"
#include "connection.h"
#include "utility.h"

#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <string>
#include <cstring>
#include <poll.h>
#include <queue>

#define DEFAULT_SERVER_PORT 2021
#define DEFAULT_GUI_PORT 20210

#define MAX_DATAGRAM_LEN 548
#define MIN_WIDTH 200
#define MAX_WIDTH 1920
#define MIN_HEIGHT 100
#define MAX_HEIGHT 1080

#define TO_SERVER_TICK 30000 // 30ms


class Client {
public:
    Client(int argc, char *argv[]);
    void connect_to_server();
    void connect_to_gui();
    void run();
private:
    uint32_t maxx;
    uint32_t maxy;
    uint64_t next_expected_event_no = 0;
    uint64_t session_id;
    uint32_t game_id;
    int8_t turn_direction = 0;
    std::string player_name;
    bool active_round = false;

    bool write_to_server = false;
    bool write_to_gui = false;

    int server_fd;
    Connection server;
    int gui_fd;
    Connection gui;
    pollfd polls[2];
    uint64_t last_client_info_send;
    std::string gui_message;

    std::queue<Codec> server_messages;
    std::queue<std::string> gui_commands;
    std::vector<std::string> game_names;

    void process_message_from_server(Codec &c, ssize_t dglen);

    void new_game(Codec &c, uint32_t &event_no, uint32_t &len, uint32_t &server_game_id);
    void pixel(Codec &c, uint32_t &event_no);
    void player_eliminated(Codec &c, uint32_t &event_no);
    void game_over();
};

#endif // SIK2_CLIENT