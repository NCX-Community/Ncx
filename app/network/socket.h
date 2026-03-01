// socket.h
#ifndef SOCKET_H
#define SOCKET_H
#include"util.h"
#include "common.h"
// tcp Socket
class TcpSocket
{
private:
    int fd;
    bool is_nonblock_;
    bool connect_on_nonblocking(InetAddress remote_endpoint);
public:
    TcpSocket(bool is_nonblock = true);
    TcpSocket(int _fd);
    ~TcpSocket();
    void bind(InetAddress local_endpoint);
    void connect(InetAddress remote_endpoint);
    void listen(int backlog);
    int accept(InetAddress &client_endpoint);
    int get_fd();

    size_t read(char *buf);
    size_t write(const char *buf);
    
};
#endif