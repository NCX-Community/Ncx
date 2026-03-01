#include "datachannel.h"

DataChannel::DataChannel(EventLoop* loop, const InetAddress& server_addr, const InetAddress& service_addr, const std::string& nonce) :
    channel_id_(UUID::GenerateUUIDAsString()),
    nonce_(nonce),
    server_client_(std::make_unique<Client>(loop, server_addr)),
    service_client_(std::make_unique<Client>(loop, service_addr))
{
    // 向ncx服务端发送data channel Hello
    server_client_->set_on_connect_cb(
        [this](std::shared_ptr<Connection> conn) {
            do_data_channel_handshake(conn);
        }
    );

    // 等待开始转发命令
    server_client_->set_on_message_cb(
        [this](std::shared_ptr<Connection> conn, Buffer* buf) {
            wait_data_channel_start_cmd(conn, buf);
        }
    );

    // 设置通道关闭回调
    server_client_->set_on_disconnect_cb(
        [this]() { delete_data_channel(); }
    );

    server_client_->connect();
}

void DataChannel::do_data_channel_handshake(std::shared_ptr<Connection> server_conn)
{
    // send data channel hello
    protocol::Hello hello;
    hello.set_hello_type(protocol::Hello::DATA_CHANNEL_HELLO);
    hello.set_digest(nonce_);
    std::string send_msg = hello.SerializeAsString();
    std::string send_msg_with_header = PROTOMSGUTIL::HeaderInstaller(send_msg);
    server_conn->Send(send_msg_with_header);
    std::puts("DATA CHANNEL HELLO SENDED!");

    // set wait start data channel cmd
    server_conn->set_message_handle(
        [this](std::shared_ptr<Connection> conn, Buffer* buf) {
            wait_data_channel_start_cmd(conn, buf);
        }
    );
}

void DataChannel::wait_data_channel_start_cmd(std::shared_ptr<Connection> server_conn, Buffer* conn_input_buf)
{
    uint32_t msg_len = PROTOMSGUTIL::CanReadMsg(conn_input_buf);
    // 没有完整的消息可读
    if(!msg_len) return;
    protocol::DataChannelCmd data_channel_cmd;
    std::string recv_cmd = conn_input_buf->RetrieveAsString(msg_len);
    if(!data_channel_cmd.ParseFromString(recv_cmd)) {
        std::cerr << "PARSE DATA CHANNEL CMD FAILED!" << std::endl;
    }

    switch (data_channel_cmd.data_channel_cmd())
    {
        case protocol::DataChannelCmd::START_FORWARD_TCP:
            std::puts("DATA CHANNEL START FORWARD TCP");
            do_data_channel_transforward();
            break;
        case protocol::DataChannelCmd::START_FORWARD_UDP:
            std::puts("DATA CHANNEL START FORWARD UDP");
            break;
    }
}

void DataChannel::do_data_channel_transforward()
{
    // 设置好转发回调然后和内网服务发起connect
    server_client_->set_on_message_cb(
        [this](std::shared_ptr<Connection> conn, Buffer* buf)
        {
            // service_conn -> server_conn
            if(buf->readAbleBytes() > 0)
            this->service_client_->get_connection()->Send(buf->RetrieveAllAsString());
        }
    );

    service_client_->set_on_message_cb(
        [this](std::shared_ptr<Connection> conn, Buffer* buf)
        {
            // server_conn -> service_conn
            if(buf->readAbleBytes() > 0)
            this->server_client_->get_connection()->Send(buf->RetrieveAllAsString());
        }
    );

    service_client_->connect();

    // 注意：此时server_client_可能已经接受到数据，所以要尝试转发一次
    if(server_client_->get_connection()->get_input_buffer()->readAbleBytes() > 0)
    {
        server_client_->get_connection()->handle_message();
    }
}