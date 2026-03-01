#ifndef CHANNEL_H
#define CHANNEL_H
#include "util.h"
#include <event2/event.h>

class EventLoop;

// Channel wraps a file descriptor and its I/O event callbacks.
// Each Channel owns at most one read event and one write event registered
// with the EventLoop's underlying event_base.
class Channel {
public:
    DISALLOW_COPY(Channel);

    Channel(EventLoop* loop, int fd);
    ~Channel();

    // Enable read monitoring.  If a read callback was set via
    // set_read_callback(), that callback fires whenever fd is readable.
    void enableRead();

    // Enable write monitoring.
    void enableWrite();

    // Stop monitoring writes (but keep read monitoring active).
    void disableWrite();

    // Stop all monitoring.
    void disableAll();

    // Remove this channel from the event loop entirely (calls disableAll()).
    void remove();

    // Store fd so callers can retrieve it for socket operations.
    int getFd();

    // Set edge-triggered mode.  Must be called before enableRead/enableWrite.
    // If events are already registered they are recreated with EV_ET.
    void set_et();

    // Register a read callback and immediately enable read monitoring.
    void set_read_callback(std::function<void()> func);

    // Register a write callback (does NOT automatically enable write monitoring).
    void set_write_callback(std::function<void()> func);

private:
    int fd_;
    EventLoop* loop_;
    bool et_mode_{false};

    struct event* read_ev_{nullptr};
    struct event* write_ev_{nullptr};

    std::function<void()> read_callback_;
    std::function<void()> write_callback_;

    // libevent static shims — receive Channel* as arg
    static void read_shim(evutil_socket_t fd, short what, void* arg);
    static void write_shim(evutil_socket_t fd, short what, void* arg);
};

#endif
