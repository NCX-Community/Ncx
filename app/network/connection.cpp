#include "connection.h"
#include "buffer.h"
#include "EventLoop.h"
#include <event2/event.h>

// ── Static shims ──────────────────────────────────────────────────────────────

void Connection::read_shim(evutil_socket_t, short, void* arg) {
    static_cast<Connection*>(arg)->handle_data_in();
}

void Connection::write_shim(evutil_socket_t, short, void* arg) {
    static_cast<Connection*>(arg)->handle_data_out();
}

// ── Constructor / Destructor ──────────────────────────────────────────────────

Connection::Connection(int _client_fd, EventLoop* loop,
                       const InetAddress& local, const InetAddress& peer)
    : client_fd(_client_fd),
      loop_(loop),
      state(ConnectionState::CONNECTING),
      local_addr_(local),
      peer_addr_(peer)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 10000);
    conn_id = dis(gen);

    input_buffer  = std::make_unique<Buffer>();
    output_buffer = std::make_unique<Buffer>();
}

Connection::~Connection() {
    // Safety net: ConnectionConstructor() should already have freed events,
    // but guard against any path that skips it.
    if (read_ev_)  { event_del(read_ev_);  event_free(read_ev_);  }
    if (write_ev_) { event_del(write_ev_); event_free(write_ev_); }
    std::printf("connection %d destructed\n", conn_id);
    ::close(client_fd);
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void Connection::ConnectionEstablished() {
    // Register persistent read event so handle_data_in() fires on incoming data.
    read_ev_ = event_new(loop_->base(), client_fd,
                         EV_READ | EV_PERSIST,
                         read_shim, this);
    event_add(read_ev_, nullptr);

    state = ConnectionState::CONNECTED;
    handle_conn();
}

void Connection::ConnectionConstructor() {
    // Cancel and free all pending libevent events before the fd is closed.
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
    state = ConnectionState::DISCONNECTED;
}

// ── Callback setters / dispatchers ───────────────────────────────────────────

void Connection::set_conn_handle(std::function<void(const std::shared_ptr<Connection>&)> on_conn) {
    on_conn_ = std::move(on_conn);
}
void Connection::handle_conn() {
    if (on_conn_) on_conn_(shared_from_this());
}

void Connection::set_message_handle(std::function<void(const std::shared_ptr<Connection>&, Buffer*)> on_message) {
    on_message_ = std::move(on_message);
}
void Connection::handle_message() {
    if (on_message_) on_message_(shared_from_this(), input_buffer.get());
    else std::puts("handle message doesn't init!");
}

// ── I/O event handlers ────────────────────────────────────────────────────────

void Connection::handle_data_in() {
    ReadNonBlocking();
    if (state != ConnectionState::DISCONNECTED) {
        handle_message();
    }
}

void Connection::handle_data_out() {
    WriteNonBlocking();
    // If the output buffer is drained, remove the write event to avoid
    // the loop firing unnecessarily when there is nothing to write.
    if (output_buffer->readAbleBytes() == 0 && write_ev_) {
        event_del(write_ev_);
        event_free(write_ev_);
        write_ev_ = nullptr;
    }
}

// ── Close handling ────────────────────────────────────────────────────────────

void Connection::set_close_handle(std::function<void(const std::shared_ptr<Connection>&)> disconnectClient) {
    on_close_ = std::move(disconnectClient);
}

void Connection::handle_close() {
    if (state != ConnectionState::DISCONNECTED) {
        if (on_close_)        on_close_(shared_from_this());
        if (notice_on_close_) notice_on_close_();
        state = ConnectionState::DISCONNECTED;
    }
}

void Connection::force_close() {
    if (state == ConnectionState::CONNECTED || state == ConnectionState::DISCONNECTING) {
        state = ConnectionState::DISCONNECTING;
        loop_->run_on_onwer_thread(
            std::bind(&Connection::handle_close, this));
    }
}

void Connection::notice_close() {
    if (notice_on_close_) notice_on_close_();
}
void Connection::set_close_notice(std::function<void()> notice_on_close) {
    notice_on_close_ = std::move(notice_on_close);
}

// ── Send / Recv ───────────────────────────────────────────────────────────────

void Connection::Recv(std::string& msg) { Recv(msg, msg.size()); }
void Connection::Recv(std::string& msg, size_t len) { msg = input_buffer->RetrieveAsString(len); }

void Connection::Send(const std::string& msg) {
    output_buffer->Append(msg);
    // Register a write event if one is not already pending.
    // handle_data_out() will drain the buffer and remove the event when done.
    if (!write_ev_) {
        write_ev_ = event_new(loop_->base(), client_fd,
                              EV_WRITE | EV_PERSIST,
                              write_shim, this);
        event_add(write_ev_, nullptr);
    }
}

// ── Non-blocking I/O ──────────────────────────────────────────────────────────

void Connection::ReadNonBlocking() {
    char buf[1024];
    ssize_t read_bytes = 0;
    while (true) {
        read_bytes = read(client_fd, buf, sizeof(buf));
        if (read_bytes > 0) {
            input_buffer->Append(buf, read_bytes);
        } else if (read_bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            else if (errno == EINTR) continue;
            else { perror("read error"); handle_close(); break; }
        } else {
            // read() returned 0 — peer closed the connection
            handle_close();
            break;
        }
    }
}

void Connection::WriteNonBlocking() {
    std::string send_buf = output_buffer->RetrieveAllAsString();
    ssize_t send_bytes = send_buf.size();
    ssize_t send_left  = send_bytes;
    while (send_left > 0) {
        ssize_t sended_bytes = write(client_fd, send_buf.data(), send_bytes);
        if (sended_bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            else if (errno == EINTR) continue;
            else { handle_close(); break; }
        }
        send_left -= sended_bytes;
    }
}

// ── Getters ───────────────────────────────────────────────────────────────────

int Connection::get_conn_id()     const { return conn_id; }
int Connection::get_fd()          const { return client_fd; }
ConnectionState Connection::get_state() const { return state; }
EventLoop* Connection::get_epoll_run()  const { return loop_; }
Buffer* Connection::get_input_buffer()  const { return input_buffer.get(); }
Buffer* Connection::get_output_buffer() const { return output_buffer.get(); }

void Connection::set_nonblocking() {
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
}
