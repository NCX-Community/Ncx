// 动态分配socket接口
#include"socket.h"
#include "merror.h"
#include "endpoint.h"

TcpSocket::TcpSocket(bool is_nonblock): is_nonblock_(is_nonblock) {
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(is_nonblock_) {
        int flags = fcntl(fd, F_GETFL, 0);
        errif(flags == -1, "fcntl error");
        flags |= O_NONBLOCK;
        errif(fcntl(fd, F_SETFL, flags) == -1, "fcntl error");
    }
    //std::cout<<"serv fd: "<<fd<<std::endl;
}

TcpSocket::TcpSocket(int _fd): fd(_fd) {}

TcpSocket::~TcpSocket() {
    printf("close fd: %d\n", fd);
    ::close(fd);
};

void TcpSocket::bind(Endpoint local) {
    errif(::bind(fd, (sockaddr*)&local.addr, sizeof(local.addr)), "tcp bind error");
}

void TcpSocket::connect(Endpoint remote) {
    errif(::connect(fd, (sockaddr*)&remote.addr, sizeof(remote.addr)), "tcp connect error");
}

void TcpSocket::listen(int backlog) {
    errif(::listen(fd, backlog), "tcp listen error");
}

int TcpSocket::accept(Endpoint &client) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    bzero(&client_addr, client_addr_len);

    int client_fd = ::accept(fd, (struct sockaddr*)&client_addr, &client_addr_len);
    // 将客户端地址信息存储到 Endpoint 对象中
    client.addr = client_addr;
    return client_fd;
}

int TcpSocket::get_fd() {return fd;}
