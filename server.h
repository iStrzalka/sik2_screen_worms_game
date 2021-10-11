#ifndef SK2_SERVER
#define SK2_SERVER

#include "codec.h"
#include "connection.h"
#include "player.h"
#include "generator.h"

#include <poll.h>
#include <map>
#include <memory>
#include <list>
#include <unordered_set>
#include <queue>
#include <set>
#include <cstring>
#include <sys/socket.h>
#include <algorithm>

#define DEFAULT_PORT 2021
#define DEFAULT_TURNING_SPEED 6
#define DEFAULT_ROUNDS_PER_SEC 50
#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 480

#define MAX_PORT 65535
#define MIN_ROUNDS_PER_SEC 1
#define MAX_ROUNDS_PER_SEC 1000

#define INACTIVE_CHECK_USEC 50000
#define SEC_TO_USEC 1000 * 1000
#define INACTIVE_TIMEOUT_USEC 2 * SEC_TO_USEC

#define MIN_WIDTH 100
#define MAX_WIDTH 4000
#define MIN_HEIGHT 100
#define MAX_HEIGHT 4000

#define MAX_DATAGRAM_SIZE 548

#define MIN_PLAYERS_REQUIRED 2
#define WINNING_PLAYERS 1

class Server {
private:
    uint32_t maxx = DEFAULT_WIDTH, maxy = DEFAULT_HEIGHT;
    std::set<std::pair<uint32_t, uint32_t> > used_pixels;
    uint32_t rounds_per_sec = DEFAULT_ROUNDS_PER_SEC;
    uint32_t turning_speed = DEFAULT_TURNING_SPEED;
    uint32_t game_id;

    int fd;
    pollfd polls[1];

    std::list<std::shared_ptr<Player> > lobby;
    std::list<std::shared_ptr<Player> > active_players;
    std::map<Connection, std::shared_ptr<Player> > clients;
    std::vector<std::shared_ptr<Codec> > events;

    Generator gen;
    
    uint64_t last_game_tick;
    uint64_t last_check_timestamp;
    std::queue<std::pair<Connection, std::shared_ptr<Codec> > > events_to_send;
    bool active_game = false;

    void pixel(Player &p);
    void player_eliminated(Player &p);
    void game_over();
    void round_tick();
    bool out_of_map(Player &p);
    bool collision(Player &p);

    void restart_game();

    void send_message_to_all_players(std::shared_ptr<Codec> event);
    void send_message_to_player(std::shared_ptr<Player> &p);

    void process_message_from_client(Connection address, Codec p, ssize_t dglen);

    bool players_are_ready();
public:
    Server(int argc, char *argv[]);
    void run();
};


#endif //SK2_SERVER