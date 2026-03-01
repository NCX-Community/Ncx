#ifndef EPOLL_RUN_H
#define EPOLL_RUN_H
#include "util.h"

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    void run();
    void stop();

    // Schedule cb to run on the event-loop thread.
    // If already on the owner thread, cb() is called directly.
    // Otherwise it is posted via event_base_once (thread-safe).
    // NOTE: preserves the original typo in the method name for ABI compatibility.
    void run_on_onwer_thread(std::function<void()> cb);

    // Returns true when called from the thread that owns this event loop.
    bool isInEpollLoop();

    // No-op kept for backward compatibility: callers (e.g. server.cpp) may
    // still call this, but with libevent the wakeup is handled internally.
    void wakeup_loop() {}

    // Exposes the underlying event_base for use by Channel, Acceptor, Connector.
    struct event_base* base() const { return base_; }

private:
    pid_t tid_;
    struct event_base* base_;
};

#endif
