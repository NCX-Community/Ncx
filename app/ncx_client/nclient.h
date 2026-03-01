#ifndef NCLIENT_H
#define NCLIENT_H

#include "util.h"
#include "ccontrolchannel.h"
#include "EventLoop.h"
#include "uuid.h"

typedef std::unordered_map<std::string, std::unique_ptr<CControlChannel>> CControlChannelMap;

class NClient{
public:
    NClient(const std::vector<CControlChannelArgs>& cc_set);
    ~NClient() = default;

    /// @brief 解析客户端参数集合，创建控制通道, 并启动它们
    void parse_args();

    void run_client();
private:
    std::unique_ptr<EventLoop> loop_;
    std::vector<CControlChannelArgs> cc_args_;
    CControlChannelMap cc_map_;
};

#endif