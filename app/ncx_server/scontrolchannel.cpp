#include "scontrolchannel.h"

SControlChannel::SControlChannel(EventLoop* loop, std::shared_ptr<Connection> ncx_client_conn, SControlChannelArgs args) :
    loop_(loop),
    ncx_client_conn_(ncx_client_conn)
{
    proxy_server_ = std::make_unique<Server>(loop_, args.proxy_addr_, args.backlog_);
    proxy_server_->bind_on_connect(
        [this](std::shared_ptr<Connection> outside_conn) {
            std::puts("SCONTROLCHANNEL ACCEPT NEW OUTSIDE CONNECTION");
            handle_new_outside_connection(outside_conn);
        }
    );
}

void SControlChannel::handle_new_outside_connection(std::shared_ptr<Connection> outside_conn)
{   
    wait_handle_visitors_.emplace(outside_conn);

    // TODO：此处可以将数据通道池化提高性能
    // 接受到外部连接，此时需要向客户端申请数据通道 / send control channel cmd -- create data channel
    protocol::ControlChannelCmd send_cmd;
    send_cmd.set_control_channel_cmd(protocol::ControlChannelCmd::CREATE_DATA_CHANNEL);
    std::string serialized_send_cmd = send_cmd.SerializeAsString();
    std::string send_cmd_with_header = PROTOMSGUTIL::HeaderInstaller(serialized_send_cmd);
    ncx_client_conn_->Send(send_cmd_with_header);
    std::puts("SCONTROLCHANNEL SEND CMD CREATE_DATA_CHANNEL");
}

std::shared_ptr<Connection> SControlChannel::pop_visitor()
{
    std::shared_ptr<Connection> visitor = wait_handle_visitors_.front();
    wait_handle_visitors_.pop();
    return visitor;
}

void SControlChannel::send_cmd(std::string cmd) { ncx_client_conn_->Send(cmd); }