#include "server.h"
#include "epoll_run.h"
#include "socket.h"
#include "channel.h"
#include "endpoint.h"
#include "acceptor.h"
#include "connection.h"
#include "current_thread.h"
#include "epThreadPool.h"

Server::Server(EpollRun* pool, const char* IP, const uint16_t PORT, const int BACKLOG): main_reactor_(pool){

    // create thread pool
    tp = std::make_unique<EpThreadPool>(pool);

    // one loop in a thread
    // create acceptor
    this->acceptor = std::make_unique<Acceptor>(IP, PORT, BACKLOG, main_reactor_.get());
    acceptor->set_new_connection_callback(std::bind(&Server::newConnectionHandle, this, std::placeholders::_1));

}

Server::~Server() {}

void Server::start() {
    tp->Start();
    main_reactor_->run();
}

void Server::newConnectionHandle(int client_fd) {
    // choose a sub reactor
    EpollRun* sub_reactor = tp->NextLoop();

    // create a new connection
    std::shared_ptr<Connection> newConn = std::make_shared<Connection>(std::move(client_fd), sub_reactor);
    newConn->set_disconnect_client_handle(std::bind(&Server::disconnectHandle, this, std::placeholders::_1));
    int newConn_id = newConn->get_conn_id();
    // add connection to connections
    connections[newConn->get_conn_id()] = std::move(newConn);
    //printf("insert new connection handle success\n");
    connections[newConn_id]->ConnectionEstablished();

    //printf("new connection create finish!\n");
}

void Server::disconnectHandle(const std::shared_ptr<Connection>& conn) {
    std::printf("thread %d disconnect connection\n", CURRENT_THREAD::tid());
    main_reactor_->add_to_do(std::bind(&Server::disconnectHandleInLoop, this, conn));
    //唤醒main_reactor_的epoll_wait
    uint64_t one = 1;
    ssize_t n = write(main_reactor_->wakeup_fd(), &one, sizeof one);
    if (n != sizeof one) {
        std::printf("wake up main reactor error\n");
    }
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

    conn->get_epoll_run()->add_to_do(std::bind(&Connection::ConnectionConstructor, conn));
    // printf("disconnect connection finish\n");
    //delete connection
}

void Server::bind_on_connect(std::function<void(std::shared_ptr<Connection>)> func) {
    on_connect_ = std::move(func);
}

void Server::bind_on_message(std::function<void(std::shared_ptr<Connection>)> func) {
    on_message_ = std::move(func);
}
