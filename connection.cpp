#include <algorithm>
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include "connection.h"

Connection::Connection(char const *node, uint16_t port, addrinfo hints) {
    if(getaddrinfo(node, std::to_string(port).c_str(), &hints, &info) != 0) {
        std::cout << "Couldn't find host.\n";
        exit(1);
    }

    memcpy(&address, info->ai_addr, info->ai_addrlen);
    length = info->ai_addrlen;
}

Connection::Connection() {
    length = sizeof(address);
}

sockaddr_storage *Connection::addr() {
    return &address;
}

socklen_t &Connection::len() {
    return length;
}

int Connection::get_family() const {
    return address.ss_family;
}

socklen_t *Connection::len_ptr() {
    return &length;
}

bool Connection::operator<(const Connection &rhs) const {
    return memcmp(rhs.addr(), addr(), std::max(len(), rhs.len())) < 0;
}

bool Connection::operator>(const Connection &rhs) const {
    return rhs < *this;
}

bool Connection::operator<=(const Connection &rhs) const {
    return !(rhs < *this);
}

bool Connection::operator>=(const Connection &rhs) const {
    return !(*this < rhs);
}

socklen_t Connection::len() const {
    return length;
}

sockaddr_storage const *Connection::addr() const {
    return &address;
}

