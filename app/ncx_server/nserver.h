#ifndef NSERVER_H
#define NSERVER_H

#include "util.h"
#include "scontrolchannel.h"
#include "server.h"
#include "connection.h"
#include "EventLoop.h"
#include "protocol.pb.h"
#include "protoMsgUtil.h"
#include "uuid.h"
#include "engine_proxy_config.h"

typedef std::unordered_map<std::string, std::unique_ptr<SControlChannel>> SControlChannelMap;

struct ServerArgs{
    InetAddress server_addr_;
    int backlog_ {256};
};

// Forward declaration — avoids pulling grpc headers into every file that includes nserver.h
class GrpcEngineClient;

/// Thread-safe snapshot of active bridge count, shared between EventLoop thread
/// (writer) and GrpcEngineClient threads (reader).
struct EngineProxySnapshot {
    int active_proxy_count = 0;
    std::mutex mu;
};

class NServer {
public:
    DISALLOW_COPY_AND_MOVE(NServer);
    explicit NServer(ServerArgs args, std::optional<EngineProxyConfig> grpc_cfg = std::nullopt);
    ~NServer();

    /// control channel 建立连接回调函数集
    void handle_new_connection(std::shared_ptr<Connection> conn, Buffer* conn_input_buf);

    /// @brief 处理接收到的control channel hello
    void handle_control_channel_hello(std::shared_ptr<Connection> conn, const std::string& service_name, const u_int16_t& proxy_port);

    /// @brief 处理接受到的data channel hello
    void handle_data_channel_hello(std::shared_ptr<Connection> conn, std::string nonce);

    void run_server();

    /// Called from EventLoop thread when a bridge is registered.
    void on_bridge_opened();

    /// Called from EventLoop thread when a bridge is removed.
    void on_bridge_closed();

    /// Thread-safe read of active bridge count (called from gRPC thread).
    int get_active_proxy_count();

private:
    std::unique_ptr<EventLoop> loop_;
    ServerArgs args_;

    std::unique_ptr<Server> ncx_acceptor_;
    std::unique_ptr<SControlChannelMap> sc_map_;

    EngineProxySnapshot snapshot_;                  // active bridge count snapshot
    std::unique_ptr<GrpcEngineClient> grpc_client_; // null when [engine_proxy] config is absent
};

#endif //NSERVER_H
