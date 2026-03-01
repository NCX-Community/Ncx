#ifndef CONNECTION_H
#define CONNECTION_H
#include "util.h"
#include "buffer.h"
#include "InetAddress.h"
#include <event2/event.h>

enum ConnectionState {
    CONNECTING,
    CONNECTED,
    DISCONNECTING,
    DISCONNECTED
};

class Connection : public std::enable_shared_from_this<Connection>{
public:
    DISALLOW_COPY_AND_MOVE(Connection);
    Connection(int client_fd, EventLoop* loop, const InetAddress& local, const InetAddress& peer);
    ~Connection();

    // Called once all callbacks are set; registers the read event and
    // transitions state to CONNECTED.
    void ConnectionEstablished();

    // Called (deferred, on the event-loop thread) when the connection is
    // being torn down.  Cancels all libevent events and releases the fd.
    void ConnectionConstructor();

    void handle_conn();
    void set_conn_handle(std::function<void(const std::shared_ptr<Connection>&)> on_conn);

    void handle_message();
    void set_message_handle(std::function<void(const std::shared_ptr<Connection>&, Buffer*)> on_message);

    /// close
    void handle_close();
    void set_close_handle(std::function<void(const std::shared_ptr<Connection>&)> disconnectClient);
    void force_close();
    void notice_close();
    void set_close_notice(std::function<void()> notice_on_close);

    void handle_data_in();
    void handle_data_out();

    void Recv(std::string& buf);
    void Recv(std::string& buf, size_t len);
    void Send(const std::string& msg);

    int get_fd() const;
    int get_conn_id() const;
    ConnectionState get_state() const;
    EventLoop* get_epoll_run() const;
    Buffer* get_input_buffer() const;
    Buffer* get_output_buffer() const;

    void set_nonblocking();

private:
    int conn_id;
    int client_fd;
    ConnectionState state;

    InetAddress local_addr_;
    InetAddress peer_addr_;

    EventLoop* loop_;

    // libevent events — registered in ConnectionEstablished(), freed in ConnectionConstructor()
    struct event* read_ev_{nullptr};
    struct event* write_ev_{nullptr};

    std::unique_ptr<Buffer> input_buffer;
    std::unique_ptr<Buffer> output_buffer;

    // callbacks
    std::function<void(const std::shared_ptr<Connection>&)> on_conn_;
    std::function<void(const std::shared_ptr<Connection>&, Buffer*)> on_message_;
    std::function<void(const std::shared_ptr<Connection>&)> on_close_;
    std::function<void()> notice_on_close_;

    void ReadNonBlocking();
    void WriteNonBlocking();

    // libevent shims
    static void read_shim(evutil_socket_t fd, short what, void* arg);
    static void write_shim(evutil_socket_t fd, short what, void* arg);
};

#endif
