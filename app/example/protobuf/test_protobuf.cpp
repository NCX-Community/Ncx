#include<iostream>
#include<string>
#include"buffer.h"
#include"protocol.pb.h"
using namespace protocol;

Buffer buffer;

void test_hello() {
    // test controlchannelhello
    // 序列化
    Hello hello;
    hello.set_hello_type(Hello::CONTROL_CHANNEL_HELLO);
    // test datachannelhello
    std::string send_msg = hello.SerializeAsString();
    std::cout<<"send_msg: "<<send_msg<<std::endl;
    buffer.Append(send_msg);
    
    // 反序列化
    Hello hello_recv;
    std::string recv_msg = buffer.RetrieveAllAsString();
    if(!hello_recv.ParseFromString(recv_msg)) {
        std::cerr << "ParseFromString failed" << std::endl;
    }

    switch (hello_recv.hello_type()) {
        case Hello::CONTROL_CHANNEL_HELLO :
            std::cout << "CONTROL_CHANNEL_HELLO" << std::endl;
            break;
        case Hello::DATA_CHANNEL_HELLO :
            std::cout << "DATA_CHANNEL_HELLO" << std::endl;
            break;
    }

}

void test_msg_header()
{
    // test msgheader
    // 序列化
    MessageHeader msg_header1;
    msg_header1.set_message_length(100);
    std::cout<<"message header length: "<<msg_header1.SerializeAsString().size()<<std::endl;

    MessageHeader msg_header2;
    msg_header2.set_message_length(10000);
    std::cout<<"message header length: "<<msg_header2.SerializeAsString().size()<<std::endl;

}

int 
main() 
{
    test_hello();
    test_msg_header();
    return 0;
}

