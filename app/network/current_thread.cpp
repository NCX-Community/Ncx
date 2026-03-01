#include "current_thread.h"
#include <sys/syscall.h>

namespace CURRENT_THREAD
{
    __thread int t_cachedTid = 0;
    __thread char t_formattedTid[32];
    __thread int t_formattedTidLength;

    pid_t gettid()
    {
        return static_cast<pid_t>(::syscall(SYS_gettid));
    }

    void cacheTid()
    {
        if (t_cachedTid == 0)
        {
            t_cachedTid = gettid();
            t_formattedTidLength = snprintf(t_formattedTid, sizeof(t_formattedTid), "%5d ", t_cachedTid);
        }
    }
}