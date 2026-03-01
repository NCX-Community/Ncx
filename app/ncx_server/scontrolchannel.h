#ifndef SCONTROLCHANNEL_H
#define SCONTROLCHANNEL_H

#include "util.h"
#include "connection.h"
#include "EventLoop.h"
#include "server.h"
#include "protocol.pb.h"
#include "protoMsgUtil.h"
#include "bridgecloser.h"

typedef std::queue<std::shared_ptr<Connection>> VisitorsQueue;
typedef std::unordered_map<std::string, std::shared_ptr<BridgeCloser>> BridgeCloserMap;

/// @brief 控制通道参数集
struct SControlChannelArgs 
{
    InetAddress proxy_addr_;    // 监听外部连接地址
    int backlog_ {256};
};

class SControlChannel 
{
public:
    SControlChannel() = delete;

    /// @brief 服务端控制通道构造函数
    /// @param loop 事件循环核心
    /// @param ncx_client_conn 与ncx client 对应的control channel的连接
    /// @param args 配置参数
    SControlChannel(EventLoop* loop, std::shared_ptr<Connection> ncx_client_conn, SControlChannelArgs args);
    ~SControlChannel() = default;

    /// proxy服务事件回调函数集合

    /// @brief 处理新到来的外部连接
    /// @param outside_conn 外部连接
    /// @param outside_conn_input_buf 外部连接输入缓冲区 
    void handle_new_outside_connection(std::shared_ptr<Connection> outside_conn);

    std::shared_ptr<Connection> pop_visitor();
    
    void send_cmd(std::string cmd);

    void register_bridge_closer(std::string bridge_id, std::shared_ptr<BridgeCloser> bridge_closer) 
        { bridge_closers_.emplace(bridge_id, bridge_closer); }
    
    void erase_bridge_closer(std::string bridge_id) 
    { 
        std::printf("erase bridge closer %s \n", bridge_id.c_str());
        if(bridge_closers_.find(bridge_id) != bridge_closers_.end())  
        { bridge_closers_.erase(bridge_id); }
    }

private:
    EventLoop* loop_;
    std::shared_ptr<Connection> ncx_client_conn_;    // 与ncx client 对应的control channel的连接
    std::unique_ptr<Server> proxy_server_;           // proxy server 负责处理外部连接
    VisitorsQueue wait_handle_visitors_;             // 等待处理的外部连接
    BridgeCloserMap bridge_closers_;                 // bridge closer 集合
};


#endif //SCONTROLCHANNEL_H