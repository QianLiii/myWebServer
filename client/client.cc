#include <iostream>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;
int main(){
        //创建套接字
        int sock = socket(AF_INET, SOCK_STREAM, 0); 
        //向服务器（特定的IP和端口）发起请求
        sockaddr_in serv_addr{};
        serv_addr.sin_family = AF_INET;  //使用IPv4地址
        serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  //具体的IP地址
        serv_addr.sin_port = htons(9090);  //端口
        connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr));


            //回声客户端
            string buf;
            cout << "输入字符串: ";
            cin >> buf;
            write(sock, &(buf[0]),buf.size());
            //读取服务器传回的数据
            char buffer[40];
            read(sock, buffer, sizeof(buffer)-1);
            cout << "服务器返回:" << buffer << endl;

        //关闭套接字
        close(sock);
        return 0;
}