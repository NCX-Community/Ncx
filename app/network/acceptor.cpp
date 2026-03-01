#include "acceptor.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include <cstdio>

// ── Static accept callback ────────────────────────────────────────────────────

void Acceptor::accept_cb(struct evconnlistener* /*listener*/,
                         evutil_socket_t fd,
                         struct sockaddr* /*addr*/,
                         int /*addrlen*/,
                         void* ctx) {
    std::printf("accept new connection\n");
    Acceptor* self = static_cast<Acceptor*>(ctx);
    if (self->new_connection_callback_) {
        self->new_connection_callback_(static_cast<int>(fd));
    } else {
        std::puts("new connection callback not init");
    }
}

// ── Internal init ─────────────────────────────────────────────────────────────

void Acceptor::init(const InetAddress& addr, int backlog, EventLoop* loop) {
    loop_ = loop;
    listener_ = evconnlistener_new_bind(
        loop_->base(),
        accept_cb,
        this,
        LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE |
        LEV_OPT_CLOSE_ON_EXEC | LEV_OPT_NONBLOCK,
        backlog,
        reinterpret_cast<const struct sockaddr*>(&addr.addr),
        static_cast<int>(sizeof(addr.addr)));

    if (!listener_) {
        std::perror("evconnlistener_new_bind failed");
    }
}

// ── Constructors / Destructor ─────────────────────────────────────────────────

Acceptor::Acceptor(const char* IP, uint16_t PORT, int BACKLOG, EventLoop* loop) {
    InetAddress addr(IP, PORT);
    init(addr, BACKLOG, loop);
}

Acceptor::Acceptor(const InetAddress& addr, int BACKLOG, EventLoop* loop) {
    init(addr, BACKLOG, loop);
}

Acceptor::~Acceptor() {
    if (listener_) {
        evconnlistener_free(listener_);
        listener_ = nullptr;
    }
}

// ── Public interface ──────────────────────────────────────────────────────────

void Acceptor::set_new_connection_callback(std::function<void(int)> handle) {
    new_connection_callback_ = std::move(handle);
}
