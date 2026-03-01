#include "client.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "connector.h"
#include "socketops.h"
#include "connection.h"

void remove_connection(EventLoop* loop, const ConnectionPtr& conn) {
    loop->run_on_onwer_thread(
        std::bind(&Connection::ConnectionConstructor, conn)
    );
}

Client::Client(EventLoop* loop, const InetAddress& server_addr)
    : loop_(loop),
      connector_(new Connector(loop, server_addr)),
      connected_(false),
      next_conn_id_(0),
      on_connect_(nullptr),
      on_message_(nullptr),
      on_disconnect_(nullptr)
{
    connector_->setNewConnectionCb(
        std::bind(&Client::newConnection, this, std::placeholders::_1)
    );
}

Client::~Client() {
    std::puts("Client Destructor");
    ConnectionPtr conn; // 这是最后的ConntionPtr
    bool unique = false;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        unique = conn_.unique();
        conn = conn_;
    }

    if (conn) {
        assert(loop_ == conn->get_epoll_run());
        std::function<void (const ConnectionPtr &)> cb = 
            std::bind(&remove_connection, loop_, std::placeholders::_1);
        
        loop_->run_on_onwer_thread(
            std::bind(&Connection::set_close_handle, conn, cb)
        );
        if(unique) {
            // 说明连接体没有读写动作,可以直接关闭
            conn->force_close();
        }
    }
    else 
    {
        // todo: 在connector还在尝试连接的过程中析构客户端会有问题
        // connector_->stop();
    }
    std::puts("Client Destructor End");
}

// 客户端建立新连接
void Client::newConnection(int sockfd) {
    // sockfd：连接成功后的文件描述符
    assert(loop_->isInEpollLoop());
    InetAddress localAddr(getLocalAddr(sockfd));
    InetAddress peerAddr(getPeerAddr(sockfd));
    ++next_conn_id_;

    ConnectionPtr conn(new Connection(sockfd,
                                     loop_,
                                     localAddr,
                                     peerAddr));
    
    conn->set_nonblocking();
    conn->set_conn_handle(on_connect_);
    conn->set_message_handle(on_message_);
    conn->set_close_handle(
        std::bind(&Client::removeConnection, this, std::placeholders::_1)
    );

    {
        std::lock_guard<std::mutex> lock(mtx_);
        conn_ = conn;
    }
    conn_->ConnectionEstablished();
}

// 客户端关闭连接
void Client::removeConnection(const ConnectionPtr& conn) {
    assert(loop_->isInEpollLoop());
    {
        std::lock_guard<std::mutex> lock(mtx_);
        assert(conn == conn_);
        conn_.reset();  // 重置为空
    }

    loop_->run_on_onwer_thread(
        std::bind(&Connection::ConnectionConstructor, conn)
    );
    connected_ = false;
    if(on_disconnect_) { on_disconnect_(); }
}

void Client::connect() 
{
    if(!connected_) 
    {
        connected_ = true;
        connector_->start();
    }
}

void Client::stop() 
{
    connected_ = false;
    connector_->stop();
}