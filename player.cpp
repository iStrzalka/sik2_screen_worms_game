#include "player.h"

Player::Player(const Connection &conn, const std::string &player_name, uint64_t session_id,
               uint32_t next_expected_event_no, int32_t turn_direction)
    : conn(conn),
      name(player_name),
      session_id(session_id),
      next_expected_event_no(next_expected_event_no),
      turn_direction(turn_direction) {
    last_message_time = get_timestamp();
    disconnected = false;
}

const Connection &Player::get_connection_attr() { return conn; }

bool Player::move(uint16_t turning_speed) {
    if (turn_direction == 1) {
        rotation = (rotation + turning_speed) % 360;
    } else if (turn_direction == 2) {
        rotation = (rotation - turning_speed) % 360;
    }
    uint32_t old_x = floor(x);
    uint32_t old_y = floor(y);
    x += cos(rotation * M_PI / 180.0);
    y += sin(rotation * M_PI / 180.0);
    return old_x != floor(x) || old_y != floor(y);
}

double Player::get_x() {
    return x;
}

double Player::get_y() {
    return y;
}
std::string Player::get_name() {
    return name;
}

void Player::update_last_message_time(uint64_t new_timestamp) {
    last_message_time = new_timestamp;
}
uint64_t Player::get_last_message_time() {
    return last_message_time;
}

bool Player::get_disconnected() {
    return disconnected;
}
void Player::set_disconnected(bool disc) {
    disconnected = disc;
}

int8_t Player::get_turn_direction() {
    return turn_direction;
}
void Player::set_turn_direction(int8_t turn_direction) {
    this->turn_direction = turn_direction;
}

bool Player::get_ready() {
    return ready;
}
void Player::set_ready(bool ready) {
    this->ready = ready;
}

uint64_t Player::get_session_id() {
    return session_id;
}
void Player::set_session_id(uint64_t session_id) {
    this->session_id = session_id;
}

uint32_t Player::get_next_expected_event_no() {
    return next_expected_event_no;
}
void Player::set_next_expected_event_no(uint32_t event_no) {
    next_expected_event_no = event_no;
}

int8_t Player::get_game_id() {
    return game_id;
}

void Player::set_game_id(int8_t game_id) {
    this->game_id = game_id;
}

void Player::set_new_position(double x, double y) {
    this->x = x;
    this->y = y;
}

void Player::set_rotation(uint32_t rotation) {
    this->rotation = rotation;
}