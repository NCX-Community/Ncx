#ifndef CURRENT_THREAD_H
#define CURRENT_THREAD_H
#include "util.h"

namespace CURRENT_THREAD {
    extern __thread int t_cachedTid;
    extern __thread char t_tidString[32];
    extern __thread int t_tidStringLength;
    extern __thread const char* t_threadName;

    void cacheTid();

    inline int tid() {
        if(__builtin_expect(t_cachedTid == 0, 0)) {
            cacheTid();
        }
        return t_cachedTid;
    }

    pid_t gettid();

    inline bool isMainThread() {
        return tid() == gettid();
    }

    inline const char *tidString() { return t_tidString; }
    inline int tidStringLength() { return t_tidStringLength; }
};

#endif