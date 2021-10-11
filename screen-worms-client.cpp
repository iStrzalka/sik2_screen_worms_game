#include "client.h"

int main(int argc, char* argv[]) {
    build_crc32_table();

    Client client(argc, argv);
    client.connect_to_gui();
    client.connect_to_server();
    client.run();
}