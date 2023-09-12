#ifndef SERV_EXCEPTION_HH
#define SERV_EXCEPTION_HH

#include <exception>
#include <string>

class Serv_Exception : std::exception {
public:
    Serv_Exception(std::string info) : _info(info) {}
    const char * what() const noexcept override {return &(_info[0]);}

private:
    std::string _info;
};



#endif