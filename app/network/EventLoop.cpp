#include "EventLoop.h"
#include "current_thread.h"
#include <event2/event.h>
#include <event2/thread.h>

EventLoop::EventLoop()
    : tid_(0),
      base_(event_base_new())
{
}

EventLoop::~EventLoop() {
    event_base_free(base_);
}

void EventLoop::run() {
    tid_ = CURRENT_THREAD::tid();
    // EVLOOP_NO_EXIT_ON_EMPTY keeps the loop alive even when no events are
    // currently registered (e.g. before the first connection arrives).
    event_base_loop(base_, EVLOOP_NO_EXIT_ON_EMPTY);
}

void EventLoop::stop() {
    event_base_loopbreak(base_);
}

bool EventLoop::isInEpollLoop() {
    return tid_ == CURRENT_THREAD::tid();
}

void EventLoop::run_on_onwer_thread(std::function<void()> cb) {
    if (isInEpollLoop()) {
        // Already on the owner thread — call directly.
        cb();
    } else {
        // Heap-allocate so the lambda can outlive this call frame.
        // The shim deletes it after invocation.
        auto* fn = new std::function<void()>(std::move(cb));
        static const struct timeval zero_tv = {0, 0};
        event_base_once(
            base_, -1, EV_TIMEOUT,
            [](evutil_socket_t, short, void* arg) {
                auto* f = static_cast<std::function<void()>*>(arg);
                (*f)();
                delete f;
            },
            fn, &zero_tv);
    }
}
