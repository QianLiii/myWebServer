#include "server.hh"

int main(int argc, char * argv[]) {
    Server server(9090,8, 5000);
    server.start();

    return 0;
}