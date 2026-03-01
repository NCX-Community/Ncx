#include "bridgecloser.h"

BridgeCloser::BridgeCloser(
    std::shared_ptr<Connection> bridge_inside, 
    std::shared_ptr<Connection> bridge_outside
) :
    bridge_id_(UUID::GenerateUUIDAsString()),
    bridge_inside_(bridge_inside),
    bridge_outside_(bridge_outside)
{}

BridgeCloser::~BridgeCloser()
{
    // 关闭桥梁两端还未关闭的连接
    if(auto bridge_inside = bridge_inside_.lock()) { bridge_inside->force_close(); std::printf("conn %d force close\n", bridge_inside->get_conn_id()); }
    if(auto bridge_outside = bridge_outside_.lock()) { bridge_outside->force_close(); std::printf("conn %d force close\n", bridge_outside->get_conn_id()); }
    std::printf("BridgeCloser %s destructed\n", bridge_id_.c_str());
}

void BridgeCloser::cut_off_bridge() { delete_bridge_(bridge_id_); }