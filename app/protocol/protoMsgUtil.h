#ifndef PROTOCOL_PROTOMSGUTIL_H
#define PROTOCOL_PROTOMSGUTIL_H

#include "singleton.h"
#include "protocol.pb.h"
#include "buffer.h"
#include <string>
#include <arpa/inet.h>

const static size_t MSG_HEADER_LENGTH = 5;

class PROTOMSGUTIL : public Singleton<PROTOMSGUTIL>
{
public:
    /// @brief 消息头封装器、注意：消息需要被序列化之后才能封装
    /// @param msg 序列化后的消息
    /// @return 封装后的消息
    static std::string HeaderInstaller(const std::string& serialized_msg) 
    {
        return PROTOMSGUTIL::GetInstance().headerInstaller(serialized_msg);
    }

    /// @brief 检查缓冲区内是否有完整的消息
    /// @param conn_input_buf 输入缓冲区
    /// @return 返回可读的消息大小,0表示不可读
    static uint32_t CanReadMsg(Buffer* conn_input_buf)
    {
        return PROTOMSGUTIL::GetInstance().canReadMsg(conn_input_buf);
    }
private:
    std::string headerInstaller(const std::string& serialized_msg) 
    {
        u_int32_t message_len = serialized_msg.size();
        protocol::MessageHeader header;
        header.set_message_length(message_len);
        std::string header_str = header.SerializeAsString();
        return header_str + serialized_msg;
    }

    u_int32_t canReadMsg(Buffer* conn_input_buf)
    {
        if(conn_input_buf->readAbleBytes() < MSG_HEADER_LENGTH) return 0;
        protocol::MessageHeader msg_header;
        msg_header.ParseFromString(conn_input_buf->PeekAsString(MSG_HEADER_LENGTH));
        if(conn_input_buf->readAbleBytes() < msg_header.message_length() + MSG_HEADER_LENGTH) return 0;

        // 可读，消耗掉消息头
        conn_input_buf->Retrieve(MSG_HEADER_LENGTH);
        return msg_header.message_length();
    }
};

#endif //PROTOCOL_PROTOMSGUTIL_H