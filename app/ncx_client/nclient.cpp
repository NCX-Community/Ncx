#include "nclient.h"


NClient::NClient(const std::vector<CControlChannelArgs>& cc_set)
{
    loop_ = std::make_unique<EventLoop>();
    cc_args_ = std::vector<CControlChannelArgs>(std::move(cc_set));
    cc_map_ = CControlChannelMap();

    parse_args();
}

void NClient::parse_args()
{
    for (auto& args : cc_args_) {
        std::string id = UUID::GenerateUUIDAsString();
        auto cchannel = std::make_unique<CControlChannel>(loop_.get(), id, args);
        //std::printf("CREATE NEW CONTROL CHANNEL WITH ID: %s\n", id.c_str());
        // set register controlchannel callback
        cchannel->register_control_channel_ = [this](std::string old_id, std::string new_id)
        {
            this->cc_map_.emplace(new_id, std::move(this->cc_map_[old_id]));
            this->cc_map_.erase(old_id);
            //std::printf("REGISTER CONTROL CHANNEL WITH ID: %s\n", new_id.c_str());
        };

        cchannel->start();
        cc_map_.emplace(id, std::move(cchannel));
    }
}

void NClient::run_client()
{
    loop_->run();
}