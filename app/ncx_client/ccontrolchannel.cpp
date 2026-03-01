#include "ccontrolchannel.h"

CControlChannel::CControlChannel(EventLoop* loop, std::string id, CControlChannelArgs args) :
    loop_(loop),
    id_(id),
    args_(args)
{
    control_client_ = std::make_unique<Client>(loop_, args_.server_addr_);
}

void CControlChannel::do_control_channel_handshake(std::shared_ptr<Connection> server_conn)
{
    // set wait control channel hello
    control_client_->set_on_message_cb(
        [this](std::shared_ptr<Connection> conn, Buffer* buf) {
            wait_control_channel_hello(conn, buf);
        }
    );
    // send control channel hello
    protocol::Hello hello;
    hello.set_hello_type(protocol::Hello::CONTROL_CHANNEL_HELLO);

    // set control channel hello content 
    protocol::Hello_Control_Channel_Hello_Content* content = hello.mutable_control_channel_hello_content();
    content->set_service_name(args_.service_name_);
    content->set_proxy_port(args_.proxy_port_);
    
    std::string send_msg = hello.SerializeAsString();
    std::string send_msg_with_header = PROTOMSGUTIL::HeaderInstaller(send_msg);
    server_conn->Send(send_msg_with_header);
}

void CControlChannel::wait_control_channel_hello(std::shared_ptr<Connection> server_conn, Buffer* conn_input_buf)
{
    uint32_t msg_len = PROTOMSGUTIL::CanReadMsg(conn_input_buf);
    // 没有完整的消息可读
    if(!msg_len) return;
    // read hello
    protocol::Hello hello;
    std::string recv_hello = conn_input_buf->RetrieveAsString(msg_len);
    if(!hello.ParseFromString(recv_hello)) {
        std::cerr << "ParseFromString failed" << std::endl;
    }
    // std::printf("RECV NONCE: %s\n", hello.digest().c_str());
    // switch hello type
    switch (hello.hello_type()) {
        case protocol::Hello::CONTROL_CHANNEL_HELLO :
            std::puts("RECV CONTROL_CHANNEL_HELLO");
            handle_control_channel_hello(hello.digest());
            break;
        case protocol::Hello::DATA_CHANNEL_HELLO :
            std::puts("RECV DATA_CHANNEL_HELLO");
            break;
    }

    // 服务端验证完成，此控制通道现在合法，可以开始接受来自服务端的命令
    control_client_->set_on_message_cb(
        [this](std::shared_ptr<Connection> conn, Buffer* buf) {
            wait_server_cmd(conn, buf);
        }
    );
}

/// @brief 接受到来自服务端的control channel hello消息后进行处理
/// @param server_conn 连接服务端的连接体
void CControlChannel::handle_control_channel_hello(std::string nonce)
{
    // 向NCX Client注册合法的control channel
    if(register_control_channel_) {
        register_control_channel_(id_, nonce);
    }
    else std::puts("REGISTER CONTROL CHANNEL CALLBACK NOT SET!");
    // renew control channel id
    id_ = nonce;
}

void CControlChannel::wait_ack(std::shared_ptr<Connection> server_conn, Buffer* conn_input_buf) 
{
    std::puts("WAIT ACK CALL!");
    uint32_t msg_len = PROTOMSGUTIL::CanReadMsg(conn_input_buf);
    // 没有完整的消息可读
    if(!msg_len) return;
    // read Ack
    protocol::Ack ack;
    std::string recv_ack = conn_input_buf->RetrieveAsString(msg_len);
    if(!ack.ParseFromString(recv_ack)) {
        std::cerr << "ParseFromString failed" << std::endl;
    }

    // handle ack
    switch (ack.ack_content()) {
        case protocol::Ack::OK :
            std::puts("RECV ACK OK");
            break;
        case protocol::Ack::AUTH_ERROR :
            break;
    }
}

void CControlChannel::wait_server_cmd(std::shared_ptr<Connection> server_conn, Buffer* conn_input_buf)
{
    std::puts("WAIT SERVER CMD CALL!");
    uint32_t msg_len = PROTOMSGUTIL::CanReadMsg(conn_input_buf);
    // 没有完整的消息可读
    if(!msg_len) return;
    // read server cmd
    protocol::ControlChannelCmd server_cmd;
    std::string recv_server_cmd = conn_input_buf->RetrieveAsString(msg_len);
    if(!server_cmd.ParseFromString(recv_server_cmd)) {
        std::cerr << "ParseFromString failed" << std::endl;
    }

    // handle server cmd
    switch (server_cmd.control_channel_cmd()) {
        case protocol::ControlChannelCmd::CREATE_DATA_CHANNEL :
            std::puts("RECV CREATE_DATA_CHANNEL");
            handle_cmd_create_datachannel();
            break;
        case protocol::ControlChannelCmd::HEARTBEAT :
            std::puts("RECV HEARTBEAT");
            break;
    }
}

void CControlChannel::handle_cmd_create_datachannel()
{
    std::unique_ptr<DataChannel> data_channel = std::make_unique<DataChannel>(loop_, args_.server_addr_, args_.service_addr_, id_);
    data_channel->set_data_channel_delete_cb(
        [this](std::string id){ 
            if(data_pool_.find(id) != data_pool_.end()) {
                data_pool_.erase(id);
            }
        }
    );
    std::string id = data_channel->get_id();
    data_pool_.emplace(id, std::move(data_channel));
    std::printf("CREATE DATA CHANNEL ID: %s\n", id.c_str());
}

void CControlChannel::start()
{
    // 连接前设置连接成功回调函数
    // std::puts("CONTROL CHANNEL CALLBACK CALL!");
    control_client_->set_on_connect_cb(
        std::bind(&CControlChannel::do_control_channel_handshake, this, std::placeholders::_1)
    );
    // 连接
    control_client_->connect();
}