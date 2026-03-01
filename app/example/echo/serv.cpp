#include "util.h"
#include "merror.h"
#include "InetAddress.h"
#include "socket.h"
#include "EventLoop.h"
#include "connection.h"
#include "server.h"
#include "common.h"

static const char SERVER_IP[] = "0.0.0.0";
static const uint16_t SERVER_PORT = 6666;
static const int BACKLOG = 10;

class EchoServer {
public:
    EchoServer(EventLoop* main_reactor, const char* IP, const uint16_t PORT, const int BACKLOG);
    ~EchoServer();
    void start();
private:
    EventLoop* main_reactor_;
    std::unique_ptr<Server> server_;
};

EchoServer::EchoServer(EventLoop* main_reactor, const char* IP, const uint16_t PORT, const int BACKLOG) {
    main_reactor_ = main_reactor;
    server_ = std::make_unique<Server>(main_reactor_, IP, PORT, BACKLOG);
    // set message callback
    server_->bind_on_message([this](const std::shared_ptr<Connection>& conn, Buffer* buf) {
        std::string msg = buf->RetrieveAllAsString();
        if(!msg.empty()) {
            std::cout<< conn->get_conn_id() << "echo:" << msg.size() << " bytes" << std::endl;
            conn->Send(msg);
        }
    });
}

EchoServer::~EchoServer() {}

void EchoServer::start() {
    server_->start();
}

int main(void)
{
    // echo server
    EventLoop* main_reactor = new EventLoop();
    EchoServer* echoServer = new EchoServer(main_reactor, SERVER_IP, SERVER_PORT, BACKLOG);
    echoServer->start();

    delete main_reactor;
    return 0;
}