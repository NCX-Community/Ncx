#include "server.h"
#include "epoll_run.h"
#include "socket.h"
#include "channel.h"
#include "endpoint.h"
#include "acceptor.h"
#include "connection.h"

Server::Server(Socket* server, EpollRun* er) {
    this->er = er;
    this->acceptor = new Acceptor(server, er);
    acceptor->setNewConnectionHandle(std::bind(&Server::newConnectionHandle, this, std::placeholders::_1));
}

Server::~Server() {}

void Server::newConnectionHandle(Socket* client) {
    // create a new connection
    Connection* newConn = new Connection(client, er);
    newConn->setDisconnectClient(std::bind(&Server::disconnectHandle, this, std::placeholders::_1));

    // add connection to connections
    connections[newConn->get_id()] = std::move(newConn);
}

void Server::disconnectHandle(int conn_id) {
    // remove connection from connections
    std::cout<<"disconnectClient!"<<std::endl;
    //delete connection
}
