#ifndef EVPOOLTHREAD_H
#define EVPOOLTHREAD_H
#include"util.h"

class EvPoolThread {
public:
    DISALLOW_COPY_AND_MOVE(EvPoolThread);
    EvPoolThread();
    ~EvPoolThread();

    EventLoop* StartLoop();
private: 
    EventLoop* loop_;   // 指向子线程创建的epoll
    std::thread thread; // 子线程
    std::mutex mtx; // 互斥锁
    std::condition_variable cv;
    void ThreadFunc();
};

#endif