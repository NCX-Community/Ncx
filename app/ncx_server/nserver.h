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

typedef std::unordered_map<std::string, std::unique_ptr<SControlChannel>> SControlChannelMap;

struct ServerArgs{
    InetAddress server_addr_;
    int backlog_ {256};
};

class NServer {
public:
    DISALLOW_COPY_AND_MOVE(NServer);
    NServer(ServerArgs args);

    /// control channel 建立连接回调函数集
    void handle_new_connection(std::shared_ptr<Connection> conn, Buffer* conn_input_buf);

    /// @brief 处理接收到的control channel hello
    /// @param conn 接受到的原始连接体
    void handle_control_channel_hello(std::shared_ptr<Connection> conn, const std::string& service_name, const u_int16_t& proxy_port);
    
    /// @brief 处理接受到的data channel hello
    /// @param conn 接受到的数据通道
    /// @param nonce 控制通道的唯一标识
    void handle_data_channel_hello(std::shared_ptr<Connection> conn, std::string nonce);

    void run_server();

private:
    std::unique_ptr<EventLoop> loop_;
    ServerArgs args_;   // 服务器参数集合

    /// NCX server 负责监听并处理来自客户端的control client, 完成control channel的建立
    std::unique_ptr<Server> ncx_acceptor_;
    std::unique_ptr<SControlChannelMap> sc_map_;    // 控制通道集合
    
};

#endif //NSERVER_H