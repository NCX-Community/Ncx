#include "buffer.h"
#include <assert.h>

Buffer::Buffer()
    : buffer_(kInitSize),
      read_index_(kPrePendIndex),
      write_index_(kPrePendIndex) {}

Buffer::~Buffer() {}

/// begin
char* Buffer::begin() { return &*buffer_.begin(); }
const char* Buffer::begin() const { return &*buffer_.begin(); }
char* Buffer::beginRead() { return begin() + read_index_; }
const char* Buffer::beginRead() const { return begin() + read_index_; }
char* Buffer::beginWrite() { return begin() + write_index_; }
const char* Buffer::beginWrite() const { return begin() + write_index_; }

/// Append
void Buffer::Append(const char* message) {
    Append(message, static_cast<size_t>(strlen(message)));
}

void Buffer::Append(const char* message, size_t len) {
    // 确保有足够的空间
    EnsureWriteableBytes(len);
    std::copy(message, message + len, beginWrite());
    write_index_ += len;
}

void Buffer::Append(const std::string &message) {
    Append(message.data(), static_cast<size_t>(message.size()));
}

/// left bytes
size_t Buffer::readAbleBytes() const { return write_index_ - read_index_;}
size_t Buffer::writeAbleBytes() const { return static_cast<size_t>(buffer_.size() - write_index_); }
size_t Buffer::prependAbleBytes() const { return read_index_; }

/// Peek
char* Buffer::Peek() { return beginRead(); }
const char* Buffer::Peek() const { return beginRead(); }
std::string Buffer::PeekAsString(size_t len) const { return std::string(beginRead(), beginRead() + len); }
std::string Buffer::PeekAllAsString() const { return std::string(beginRead(), beginWrite()); }

/// Retrieve
void Buffer::Retrieve(size_t len) {
    assert(readAbleBytes() >= len);
    if(len + read_index_ < write_index_) {
        // 读不超过可读空间，只用更新read_index_即可
        read_index_ += len;
    }
    else {
        // 读超过可读空间，直接将read_index_置为write_index_即可
        RetrieveAll();
    }
}

void Buffer::RetrieveAll() {
    write_index_ = kPrePendIndex;
    read_index_ = write_index_;
}

void Buffer::RetrieveUntil(const char* end) {
    assert(beginWrite() >= end);
    read_index_ += static_cast<size_t>(end - beginRead());
}

std::string Buffer::RetrieveAsString(size_t len) {
    assert(read_index_ + len <= write_index_);
    std::string ret = std::move(PeekAsString(len));
    Retrieve(len);
    return ret;
}

std::string Buffer::RetrieveUtilAsString(const char* end) {
    assert(beginWrite() >= end);
    std::string ret = std::move(PeekAsString((static_cast<size_t>(end - beginRead()))));
    RetrieveUntil(end);
    return ret;
}

std::string Buffer::RetrieveAllAsString() {
    assert(readAbleBytes() >= 0);
    std::string ret = std::move(PeekAllAsString());
    RetrieveAll();
    return ret;
}

void Buffer::EnsureWriteableBytes(size_t len) {
    if(writeAbleBytes() >= len) return;
    if(writeAbleBytes() + prependAbleBytes() >= kPrePendIndex + len) {
        // 此时writeable和prepenable的剩余空间超过写的空间，则将已有数据复制到初始位置
        // 将不断后退的read_index_后移导致的前面没有利用的空间利用上；
        // 相当于做一次缓冲区的整理
        std::copy(beginRead(), beginWrite(), begin() + kPrePendIndex);
        write_index_ = kPrePendIndex + readAbleBytes();
        read_index_ = kPrePendIndex; 
    }
    else {
        // 所有剩余的空间也不够写入了，需要扩容
        buffer_.resize(write_index_ + len);
    }
}

