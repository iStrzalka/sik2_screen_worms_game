#include "server.h"

int main(int argc, char *argv[]) {
    build_crc32_table();
    
    Server server(argc, argv);
    server.run();
}