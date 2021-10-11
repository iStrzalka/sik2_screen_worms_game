#ifndef SIK2_PLAYER
#define SIK2_PLAYER

#include "connection.h"
#include "utility.h"

#include <cstdint>
#include <cmath>
#include <string>

class Player {
private:
    Connection conn;
    std::string name;
    uint64_t session_id;
    uint32_t next_expected_event_no;
    int32_t turn_direction;
    double x, y;
    int32_t rotation;
    bool ready;
    uint64_t last_message_time;
    bool disconnected;
    int8_t game_id;
public:
    Player(const Connection &c, const std::string &player_name,
           uint64_t session_id, uint32_t next_expected_event_no, int32_t turn_direction);

    const Connection &get_connection_attr();

    bool move(uint16_t turning_speed);
    void set_new_position(double x, double y);
    double get_x();
    double get_y();
    std::string get_name();

    int8_t get_game_id();
    void set_game_id(int8_t game_id);

    void update_last_message_time(uint64_t new_timestamp);
    uint64_t get_last_message_time();

    bool get_disconnected();
    void set_disconnected(bool disc);

    int8_t get_turn_direction();
    void set_turn_direction(int8_t turn_direction);

    bool get_ready();
    void set_ready(bool ready);

    uint64_t get_session_id();
    void set_session_id(uint64_t session_id);

    uint32_t get_next_expected_event_no();
    void set_next_expected_event_no(uint32_t event_no);

    void set_rotation(uint32_t rotation);
};

#endif //SIK2_PLAYER