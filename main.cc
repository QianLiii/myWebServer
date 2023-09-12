#include "server.hh"

int main(int argc, char * argv[]) {
    Server server(8080,8, 5000);
    try {
        server.start();
    } catch(Serv_Exception e) {
        std::cout<<e.what()<<std::endl;
    }

    return 0;
}