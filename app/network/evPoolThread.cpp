#include "evPoolThread.h"
#include "EventLoop.h"

EvPoolThread::EvPoolThread() {};
EvPoolThread::~EvPoolThread() {};

void EvPoolThread::ThreadFunc()
{
    // 线程执行体
    EventLoop loop;
    {
        std::unique_lock<std::mutex> lock(mtx);
        loop_ = &loop;
        // 锁将被释放、唤醒主线程
        cv.notify_one();
    }
    loop.run();

    {
        // 析构
        std::unique_lock<std::mutex> lock(mtx);
        loop_ = nullptr;
    }
}

EventLoop *EvPoolThread::StartLoop()
{
    thread = std::thread(&EvPoolThread::ThreadFunc, this);
    EventLoop *res = nullptr;

    {
        // wait for loop created
        std::unique_lock<std::mutex> lock(mtx);
        while (loop_ == nullptr)
        {
            cv.wait(lock); //sleep until notify
        }
        res = loop_;
    }

    return res;
}