#include "server.hh"

int main(int argc, char * argv[]) {
    try {
        Thread_Pool::get_pool();
        Server server(9090,4, 2000);
        server.start();
    } catch(Serv_Exception e) {
        std::cout<<e.what()<<std::endl;
    }

    return 0;
}