#ifndef BRIDGE_CLOSER_H
#define BRIDGE_CLOSER_H

#include "util.h"
#include "connection.h"
#include "uuid.h"

class BridgeCloser
{
public:
    BridgeCloser() = delete;
    BridgeCloser(std::shared_ptr<Connection> bridge_inside, std::shared_ptr<Connection> bridge_outside);
    ~BridgeCloser();

    std::string get_bridge_id() { return bridge_id_; }
    void cut_off_bridge();
    void set_delete_bridge(std::function<void(std::string)> delete_bridge) { delete_bridge_ = delete_bridge; }
    
private:
    std::string bridge_id_;
    std::weak_ptr<Connection> bridge_inside_;
    std::weak_ptr<Connection> bridge_outside_;

    std::function<void(std::string)> delete_bridge_;
};

#endif  // BRIDGE_CLOSER_H