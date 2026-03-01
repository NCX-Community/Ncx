#include "connector.h"
#include "EventLoop.h"
#include <event2/bufferevent.h>
#include <cstdio>
#include <unistd.h>

// ── Static event callback ─────────────────────────────────────────────────────

void Connector::event_cb(struct bufferevent* bev, short events, void* arg) {
    Connector* self = static_cast<Connector*>(arg);

    if (events & BEV_EVENT_CONNECTED) {
        // Extract the fd and detach it from the bufferevent before freeing,
        // so that bufferevent_free does NOT close the socket.
        int fd = static_cast<int>(bufferevent_getfd(bev));
        bufferevent_setfd(bev, -1);   // detach — fd ownership transfers to caller
        bufferevent_free(bev);
        self->bev_ = nullptr;
        self->setState(kConnected);
        if (self->new_conn_cb_) {
            self->new_conn_cb_(fd);
        }
    } else if (events & (BEV_EVENT_ERROR | BEV_EVENT_EOF)) {
        std::perror("Connector: connection failed");
        // bev_ was created without BEV_OPT_CLOSE_ON_FREE, so free + close fd manually
        int fd = static_cast<int>(bufferevent_getfd(bev));
        bufferevent_free(bev);
        self->bev_ = nullptr;
        if (fd >= 0) ::close(fd);
        self->setState(kDisconnected);
    }
}

// ── Constructor / Destructor ──────────────────────────────────────────────────

Connector::Connector(EventLoop* loop, const InetAddress& server_addr)
    : loop_(loop), server_addr_(server_addr) {}

Connector::~Connector() {
    stopInLoop();
}

// ── Public interface ──────────────────────────────────────────────────────────

void Connector::start() {
    connect_ = true;
    loop_->run_on_onwer_thread([this]() { startInLoop(); });
}

void Connector::restart() {
    // TODO: implement reconnect with timer support
    setState(kDisconnected);
    connect_ = true;
    start();
}

void Connector::stop() {
    connect_ = false;
    loop_->run_on_onwer_thread([this]() { stopInLoop(); });
}

// ── Private helpers ───────────────────────────────────────────────────────────

void Connector::startInLoop() {
    if (!connect_) return;

    // Create a bufferevent with no CLOSE_ON_FREE so that we can transfer
    // fd ownership to Connection after a successful connect.
    bev_ = bufferevent_socket_new(loop_->base(), -1, 0);
    if (!bev_) {
        std::puts("Connector::startInLoop: bufferevent_socket_new failed");
        return;
    }
    bufferevent_setcb(bev_, nullptr, nullptr, event_cb, this);
    bufferevent_enable(bev_, EV_READ | EV_WRITE);
    setState(kConnecting);

    int ret = bufferevent_socket_connect(
        bev_,
        reinterpret_cast<const struct sockaddr*>(&server_addr_.addr),
        static_cast<int>(sizeof(server_addr_.addr)));

    if (ret < 0) {
        std::perror("Connector::startInLoop: bufferevent_socket_connect failed");
        int fd = static_cast<int>(bufferevent_getfd(bev_));
        bufferevent_free(bev_);
        bev_ = nullptr;
        if (fd >= 0) ::close(fd);
        setState(kDisconnected);
    }
}

void Connector::stopInLoop() {
    if (bev_) {
        int fd = static_cast<int>(bufferevent_getfd(bev_));
        bufferevent_free(bev_);
        bev_ = nullptr;
        if (fd >= 0) ::close(fd);
        setState(kDisconnected);
    }
}
