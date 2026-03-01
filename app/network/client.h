#ifndef CLIENT_H
#define CLIENT_H

#include"util.h"
#include"connection.h"
#include "ccontrolchannel.h"
#include "protocol.pb.h"
#include "protoMsgUtil.h"

typedef std::shared_ptr<Connector> ConnectorPtr;
typedef std::shared_ptr<Connection> ConnectionPtr;

// 用于向eventloop删除通道
void remove_connection(EventLoop* loop, const ConnectionPtr& conn);

// 普通客户端
class Client {
public:
    DISALLOW_COPY_AND_MOVE(Client);
    Client(EventLoop* loop, const InetAddress& server_addr_);
    ~Client();

    ///op
    void connect();
    void disconnect(); // TODO: 关闭写端，但仍可接受数据
    void stop();

    std::shared_ptr<Connection> connection() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return conn_;
    }

    EventLoop* getLoop() const { return loop_; }

    // callback function set
    void set_on_connect_cb(std::function<void(std::shared_ptr<Connection>)> cb) {
        on_connect_ = std::move(cb);
        if(conn_)
        {
            // 连接体存在后修改回调函数
            conn_->set_conn_handle(on_connect_);
        }
    }

    void set_on_message_cb(std::function<void(std::shared_ptr<Connection>, Buffer*)> cb) {
        on_message_ = std::move(cb);
        if(conn_)
        {
            // 连接体存在后修改回调函数
            conn_->set_message_handle(on_message_);
        }
    }

    void set_on_disconnect_cb(std::function<void()> cb) {
        on_disconnect_ = std::move(cb);
    }

    std::shared_ptr<Connection> get_connection() const { return conn_; }

private:
    /// Not thread safe, but in loop
    void newConnection(int sockfd);
    void removeConnection(const ConnectionPtr& conn);

    EventLoop* loop_;
    ConnectorPtr connector_;    // 向服务端建立连接
    // const std::string client_name_;

    // 连接握手阶段，用于对连接的预处理（鉴权、判断连接请求类型）
    std::function<void(std::shared_ptr<Connection>)> on_connect_;
    std::function<void(std::shared_ptr<Connection>, Buffer*)> on_message_;
    std::function<void()> on_disconnect_;

    bool connected_;    // 客户端是否连接成功
    mutable std::mutex mtx_;
    std::shared_ptr<Connection> conn_;
    int next_conn_id_;

};

#endif // CLIENT_H


