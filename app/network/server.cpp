#include "server.h"
#include "EventLoop.h"
#include "socket.h"
#include "channel.h"
#include "InetAddress.h"
#include "acceptor.h"
#include "connection.h"
#include "current_thread.h"
#include "socketops.h"

Server::Server(EventLoop* loop, const char* IP, const uint16_t PORT, const int BACKLOG): loop_(loop){

    // create acceptor
    server_acceptor_= std::make_unique<Acceptor>(IP, PORT, BACKLOG, loop_);
    server_acceptor_->set_new_connection_callback(std::bind(&Server::newConnectionHandle, this, std::placeholders::_1));

}

Server::Server(EventLoop* loop, const InetAddress& addr, const int BACKLOG): loop_(loop) {
    // create acceptor
    server_acceptor_= std::make_unique<Acceptor>(addr, BACKLOG, loop_);
    server_acceptor_->set_new_connection_callback(std::bind(&Server::newConnectionHandle, this, std::placeholders::_1));
}

Server::~Server() {}

void Server::start() {
    loop_->run();
}

void Server::newConnectionHandle(int client_fd) {
    // create a new connection
    InetAddress localAddr(getLocalAddr(client_fd));
    InetAddress peerAddr(getPeerAddr(client_fd));
    std::shared_ptr<Connection> newConn = std::make_shared<Connection>(client_fd, 
                                                                       loop_,
                                                                       localAddr,
                                                                       peerAddr

    );

    // set connection nonblocking
    newConn->set_nonblocking();

    // set connection handle
    newConn->set_conn_handle(on_connect_);
    newConn->set_message_handle(on_message_);
    newConn->set_close_handle(std::bind(&Server::disconnectHandle, this, std::placeholders::_1));

    int newConn_id = newConn->get_conn_id();
    //printf("insert new connection handle success\n");

    newConn->ConnectionEstablished();
    // add connection to connections
    connections[newConn->get_conn_id()] = std::move(newConn);
}

void Server::disconnectHandle(const std::shared_ptr<Connection>& conn) {
    // std::printf("thread %d disconnect connection\n", CURRENT_THREAD::tid());
    loop_->run_on_onwer_thread(std::bind(&Server::disconnectHandleInLoop, this, conn));
    //唤醒main_reactor_的epoll_wait
    loop_->wakeup_loop();
}

void Server::disconnectHandleInLoop(const std::shared_ptr<Connection>& conn) {
    // remove connection from connections
    // std::printf("disconnect %d connection\n", conn_id);

    /// 在多线程开发中，由于bind function的存在，存在类对象的成员函数被其他类对象调用，该类对象和其他类对象可能不处于同一个线程。
    /// 由于unordered_map的线程不安全性，可能会导致在删除连接时，其他线程正在访问该连接，导致程序崩溃。所以需要将子线程对map的操作转移到
    /// 主线程中进行，这样就不会出现多线程同时访问map的情况。
    int conn_id = conn->get_conn_id();
    if (connections.find(conn_id) != connections.end()) {
        connections.erase(conn_id);
    }

    // 此时conn的引用计数可能还为1,保证handle_message执行过程conn不会被释放
    // 调用addtodo后,conn的引用计数为2,等待当前wait分发完handle后,执行删除channel连接
    // 等connectionconstructor执行完毕后,conn的引用计数为0,conn析构

    // todo: 等待当前wait分发完handle时,如果wait一直没有时间,那么channel的删除连接操作无法执行,epoll需要监听的事件无法减少,导致服务器性能下降.需要修复

    conn->get_epoll_run()->run_on_onwer_thread(std::bind(&Connection::ConnectionConstructor, conn));
    // printf("disconnect connection finish\n");
}

void Server::bind_on_connect(std::function<void(std::shared_ptr<Connection>)> func) { on_connect_ = std::move(func); }
void Server::bind_on_message(std::function<void(std::shared_ptr<Connection>, Buffer*)> func) { on_message_ = std::move(func);}
void Server::bind_on_disconnect(std::function<void()> func) { on_disconnect_ = std::move(func); }
void Server::update_on_message(std::shared_ptr<Connection> conn, std::function<void(std::shared_ptr<Connection>, Buffer*)> func) { conn->set_message_handle(func);}

std::shared_ptr<Connection> Server::getConnection(int conn_id) {
    if(connections.find(conn_id) != connections.end()) {
        return connections[conn_id];
    }
    else {
        return nullptr;
    }
}
