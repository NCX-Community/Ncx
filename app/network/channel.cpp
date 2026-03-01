#include "channel.h"
#include "EventLoop.h"

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop), fd_(fd) {}

Channel::~Channel() {
    disableAll();
}

// ── Static shims ─────────────────────────────────────────────────────────────

void Channel::read_shim(evutil_socket_t, short, void* arg) {
    Channel* ch = static_cast<Channel*>(arg);
    if (ch->read_callback_) ch->read_callback_();
}

void Channel::write_shim(evutil_socket_t, short, void* arg) {
    Channel* ch = static_cast<Channel*>(arg);
    if (ch->write_callback_) ch->write_callback_();
}

// ── Event management ──────────────────────────────────────────────────────────

void Channel::enableRead() {
    if (read_ev_) return;  // already registered
    short flags = EV_READ | EV_PERSIST;
    if (et_mode_) flags |= EV_ET;
    read_ev_ = event_new(loop_->base(), fd_, flags, read_shim, this);
    event_add(read_ev_, nullptr);
}

void Channel::enableWrite() {
    if (write_ev_) return;  // already registered
    short flags = EV_WRITE | EV_PERSIST;
    if (et_mode_) flags |= EV_ET;
    write_ev_ = event_new(loop_->base(), fd_, flags, write_shim, this);
    event_add(write_ev_, nullptr);
}

void Channel::disableWrite() {
    if (!write_ev_) return;
    event_del(write_ev_);
    event_free(write_ev_);
    write_ev_ = nullptr;
}

void Channel::disableAll() {
    if (read_ev_) {
        event_del(read_ev_);
        event_free(read_ev_);
        read_ev_ = nullptr;
    }
    if (write_ev_) {
        event_del(write_ev_);
        event_free(write_ev_);
        write_ev_ = nullptr;
    }
}

void Channel::remove() {
    disableAll();
}

int Channel::getFd() {
    return fd_;
}

void Channel::set_et() {
    et_mode_ = true;
    // Recreate any already-registered events to include EV_ET
    if (read_ev_) {
        event_del(read_ev_);
        event_free(read_ev_);
        read_ev_ = event_new(loop_->base(), fd_, EV_READ | EV_PERSIST | EV_ET,
                             read_shim, this);
        event_add(read_ev_, nullptr);
    }
    if (write_ev_) {
        event_del(write_ev_);
        event_free(write_ev_);
        write_ev_ = event_new(loop_->base(), fd_, EV_WRITE | EV_PERSIST | EV_ET,
                              write_shim, this);
        event_add(write_ev_, nullptr);
    }
}

void Channel::set_read_callback(std::function<void()> func) {
    read_callback_ = std::move(func);
    enableRead();
}

void Channel::set_write_callback(std::function<void()> func) {
    write_callback_ = std::move(func);
}
