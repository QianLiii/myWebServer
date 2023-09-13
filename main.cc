#include "server.hh"

int main(int argc, char * argv[]) {
    try {
        Server server(9090,8, -1);
        server.start();
    } catch(Serv_Exception e) {
        std::cout<<e.what()<<std::endl;
    }

    return 0;
}