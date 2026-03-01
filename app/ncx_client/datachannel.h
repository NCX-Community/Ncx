#ifndef DATACHANNEL_H
#define DATACHANNEL_H

#include "util.h"
#include "client.h"
#include "InetAddress.h"
#include "EventLoop.h"
#include "connection.h"
#include "uuid.h"

class DataChannel
{
public:
    DataChannel() = delete;
    DataChannel(EventLoop* loop, const InetAddress& server_addr, const InetAddress& service_addr, const std::string& nonce);
    ~DataChannel() { std::puts("DataChannel Destructor"); };

    // 数据通道回调函数集合

    /// @brief 数据通道握手
    /// @param server_conn 连接向服务端连接体
    void do_data_channel_handshake(std::shared_ptr<Connection> server_conn);
    void wait_data_channel_start_cmd(std::shared_ptr<Connection> server_conn, Buffer* conn_input_buf);

    /// @brief 数据通道开始转发
    void do_data_channel_transforward();
    
    std::string get_id() const { return channel_id_; }
    void set_data_channel_delete_cb(std::function<void(std::string id)> cb) { data_channel_delete_ = std::move(cb); }
    void delete_data_channel() { data_channel_delete_(channel_id_); }

private:
    std::string channel_id_;
    std::string nonce_;  // 证明数据通道合法的token 
    std::unique_ptr<Client> server_client_;
    std::unique_ptr<Client> service_client_;

    std::function<void(std::string id)> data_channel_delete_;
};

#endif // DATACHANNEL_H