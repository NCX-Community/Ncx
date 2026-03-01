#include"buffer.h"
#include<iostream>
#include<string>

const char* test_message = "hello ncx";
const std::string test_message_s = "hello ncx";

int main() {
    Buffer buffer;
    buffer.Append(test_message);
    std::string recv_buf = buffer.RetrieveAllAsString();
    std::cout << "test1: " << recv_buf << std::endl;

    buffer.Append(test_message_s);
    size_t len = test_message_s.size();
    recv_buf = buffer.RetrieveAsString(len);
    std::cout << "test2: " << recv_buf << std::endl;

    return 0;
}