#ifndef CONNECTOR_H
#define CONNECTOR_H
#include "util.h"
#include "InetAddress.h"
#include <event2/bufferevent.h>

/// Connector — initiates an outbound non-blocking TCP connection.
/// Uses libevent bufferevent_socket_connect internally.
/// On success, delivers the connected socket fd via the callback registered
/// with setNewConnectionCb(); ownership of the fd then transfers to the caller.
class Connector {
public:
    Connector(EventLoop* loop, const InetAddress& server_addr);
    ~Connector();

    void setNewConnectionCb(const std::function<void(int sockfd)>& cb) {
        new_conn_cb_ = cb;
    }

    void start();
    void restart();  // TODO: implement reconnect with timer
    void stop();

    const InetAddress& serverAddr() const { return server_addr_; }

private:
    enum States { kDisconnected, kConnecting, kConnected };

    void setState(States s) { state_ = s; }
    void startInLoop();
    void stopInLoop();

    EventLoop* loop_;
    InetAddress server_addr_;
    bool connect_{false};
    struct bufferevent* bev_{nullptr};
    std::function<void(int sockfd)> new_conn_cb_;
    States state_{kDisconnected};

    // libevent event callback shim
    static void event_cb(struct bufferevent* bev, short events, void* arg);
};

#endif
