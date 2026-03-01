#include "socketops.h"
#include "util.h"

void Close(int sockfd) 
{
    if (::close(sockfd) < 0)
    {
        std::puts("close error");
    }
}

struct sockaddr_in getLocalAddr(int sockfd)
{
    struct sockaddr_in localaddr;
    std::memset(&localaddr, 0, sizeof(localaddr));
    socklen_t addrlen = static_cast<socklen_t>(sizeof(localaddr));
    if (::getsockname(sockfd, (struct sockaddr*)&localaddr, &addrlen) < 0)
    {
        std::puts("getsockname error");
    }
    return localaddr;
}

struct sockaddr_in getPeerAddr(int sockfd)
{
    struct sockaddr_in peeraddr;
    std::memset(&peeraddr, 0, sizeof(peeraddr));
    socklen_t addrlen = static_cast<socklen_t>(sizeof(peeraddr));
    if (::getpeername(sockfd, (struct sockaddr*)&peeraddr, &addrlen) < 0)
    {
        std::puts("getpeername error");
    }
    return peeraddr;
}

int createNonblockingOrDie()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0)
    {
        std::puts("socket error");
    }
    return sockfd;
}