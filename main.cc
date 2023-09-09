#include "server.hh"

int main(int argc, char * argv[]) {
    Server server(9090,8,-1);
    server.start();

    return 0;
}