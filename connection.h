#ifndef SIK2_ADDRESS_H
#define SIK2_ADDRESS_H
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <netdb.h>
#include <sys/socket.h>
#include <netdb.h>

class Connection {
public:
    Connection();
    Connection(char const *node, uint16_t port, addrinfo hints);
    socklen_t & len();
    sockaddr_storage* addr();
    socklen_t len() const;
    sockaddr_storage const* addr() const;
    int get_family() const;

    friend std::ostream &operator<<(std::ostream &os, Connection const& address) {
        char buffer[INET6_ADDRSTRLEN];
        char port[8];
        getnameinfo((sockaddr const*)&address.address, address.length, buffer, sizeof(buffer), port, sizeof(port), NI_NUMERICHOST|NI_NUMERICSERV);
        os << buffer << ":" << port;
        return os;
    }

    socklen_t *len_ptr();
    addrinfo *info;
private:
    sockaddr_storage address;
    socklen_t length;

public:
    bool operator<(const Connection &rhs) const;

    bool operator>(const Connection &rhs) const;

    bool operator<=(const Connection &rhs) const;

    bool operator>=(const Connection &rhs) const;
};

#endif //SIK2_ADDRESS_H
