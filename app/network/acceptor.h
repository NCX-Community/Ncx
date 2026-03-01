#ifndef ACCEPTOR_H
#define ACCEPTOR_H
#include "util.h"
#include <event2/listener.h>

class Acceptor {
public:
    Acceptor() = default;
    Acceptor(const char* IP, uint16_t PORT, int BACKLOG, EventLoop* loop);
    Acceptor(const InetAddress& addr, int BACKLOG, EventLoop* loop);
    ~Acceptor();

    // Register the callback that receives the accepted file descriptor.
    // Called by Server before the acceptor is active.
    void set_new_connection_callback(std::function<void(int)> handle);

private:
    EventLoop* loop_{nullptr};
    struct evconnlistener* listener_{nullptr};
    std::function<void(int)> new_connection_callback_;

    void init(const InetAddress& addr, int backlog, EventLoop* loop);

    // libevent accept shim
    static void accept_cb(struct evconnlistener* listener,
                          evutil_socket_t fd,
                          struct sockaddr* addr,
                          int addrlen,
                          void* ctx);
};

#endif
