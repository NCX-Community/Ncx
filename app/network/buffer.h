#ifndef BUFFER_H
#define BUFFER_H
#include"util.h"

static const int kPrePendIndex = 8; // 8字节的预留空间
static const int kInitSize = 1024;  // 初始化开辟缓存空间大小

class Buffer {
public:
    DISALLOW_COPY_AND_MOVE(Buffer);
    
    Buffer();
    ~Buffer();
    
    /// Buffer起始位置
    char* begin();

    /// const的Buffer起始位置，返回的迭代器不可修改
    const char* begin() const;

    /// 从起始位置开始读取
    char* beginRead();
    const char* beginRead() const;

    /// 从起始位置开始写入
    char* beginWrite();
    const char* beginWrite() const;

    /// 添加数据
    void Append(const char* message);
    void Append(const char* message, size_t len);
    void Append(const std::string &message);

    /// 获取可读大小
    size_t readAbleBytes() const;
    size_t writeAbleBytes() const;
    size_t prependAbleBytes() const;

    /// 查看数据，但是不更新'read_index_' 位置
    char* Peek();
    const char* Peek() const;
    std::string PeekAsString(size_t len) const;
    std::string PeekAllAsString() const;

    /// 取数据，取出后更新'read_index_' 位置，相当于取出数据，不可以重复取
    void Retrieve(size_t len);
    std::string RetrieveAsString(size_t len);

    /// 取出全部数据
    void RetrieveAll();
    std::string RetrieveAllAsString();

    /// 某个索引之前 / 左闭右开区间
    void RetrieveUntil(const char* end);
    std::string RetrieveUtilAsString(const char* end);

    /// 扩容，保证消息能够被添加进缓冲区
    void EnsureWriteableBytes(size_t len);


private:
    std::vector<char> buffer_;
    size_t read_index_;
    size_t write_index_;
};

#endif