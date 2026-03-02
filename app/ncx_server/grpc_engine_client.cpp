#include "grpc_engine_client.h"
#include "nserver.h"

using enginecluster::v1::CapacityReport;
using enginecluster::v1::EngineHeartbeat;
using enginecluster::v1::EngineMessage;
using enginecluster::v1::EngineRegistration;
using enginecluster::v1::ProxyMessage;

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

GrpcEngineClient::GrpcEngineClient(Config cfg, NServer* nserver)
    : cfg_(std::move(cfg)), nserver_(nserver)
{
    channel_ = grpc::CreateChannel(cfg_.proxy_endpoint, grpc::InsecureChannelCredentials());
}

GrpcEngineClient::~GrpcEngineClient()
{
    stop();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void GrpcEngineClient::start()
{
    running_ = true;
    write_thread_ = std::thread(&GrpcEngineClient::run_write, this);
    read_thread_  = std::thread(&GrpcEngineClient::run_read,  this);
}

void GrpcEngineClient::stop()
{
    if (!running_.exchange(false)) return;  // already stopped

    // Wake heartbeat sleep so write_thread_ can exit promptly.
    hb_cv_.notify_all();

    // Cancel the active gRPC context to unblock stream->Read() in read_thread_.
    {
        std::lock_guard<std::mutex> lk(ctx_mu_);
        if (ctx_) ctx_->TryCancel();
    }

    // Wake read_thread_ if it is waiting for a stream to become available.
    stream_cv_.notify_all();

    if (write_thread_.joinable()) write_thread_.join();
    if (read_thread_.joinable())  read_thread_.join();
}

// ---------------------------------------------------------------------------
// T013 / T016 / T020 / T021 — run_write()
//   Outer reconnect loop → EngineRegistration → heartbeat loop
// ---------------------------------------------------------------------------

void GrpcEngineClient::run_write()
{
    while (running_) {
        // Create a fresh context for this connection attempt.
        {
            std::lock_guard<std::mutex> lk(ctx_mu_);
            ctx_ = std::make_unique<grpc::ClientContext>();
        }

        auto stub = enginecluster::v1::EngineClusterStream::NewStub(channel_);

        std::printf("[GrpcEngineClient] Connecting to engine proxy at %s\n",
                    cfg_.proxy_endpoint.c_str());

        // Open bidirectional stream (blocks until the RPC is initiated).
        {
            std::lock_guard<std::mutex> lk(stream_mu_);
            stream_ = std::shared_ptr<Stream>(stub->Connect(ctx_.get()));
        }
        stream_cv_.notify_one();  // wake run_read() which is waiting for the stream

        // --- T013: Send EngineRegistration once on stream open (US1) ---
        EngineMessage reg_msg;
        EngineRegistration* reg = reg_msg.mutable_registration();
        reg->set_engine_id(cfg_.engine_id);
        reg->set_address(cfg_.engine_address);
        reg->set_max_capacity(cfg_.max_capacity);

        if (!stream_->Write(reg_msg)) {
            std::fprintf(stderr,
                "[GrpcEngineClient] Registration write failed. Reconnecting in 5s...\n");
            goto cleanup;
        }

        std::printf("[GrpcEngineClient] Stream open. Sending registration: "
                    "engine_id=%s address=%s max_capacity=%d\n",
                    cfg_.engine_id.c_str(),
                    cfg_.engine_address.c_str(),
                    cfg_.max_capacity);

        // --- T016: Heartbeat loop (US2) ---
        while (running_) {
            {
                std::unique_lock<std::mutex> lk(hb_mu_);
                hb_cv_.wait_for(lk,
                    std::chrono::seconds(cfg_.heartbeat_interval_sec),
                    [this] { return !running_.load(); });
            }
            if (!running_) break;

            EngineMessage hb_msg;
            EngineHeartbeat* hb = hb_msg.mutable_heartbeat();
            hb->set_engine_id(cfg_.engine_id);
            hb->set_active_proxy_count(nserver_->get_active_proxy_count());

            if (!stream_->Write(hb_msg)) {
                std::fprintf(stderr, "[GrpcEngineClient] Heartbeat write failed.\n");
                break;
            }
            std::printf("[GrpcEngineClient] Heartbeat sent. active_proxy_count=%d\n",
                        hb->active_proxy_count());
        }

    cleanup:
        // --- T020 / T021: Reconnect cleanup ---
        {
            std::lock_guard<std::mutex> lk(ctx_mu_);
            if (ctx_) {
                ctx_->TryCancel();
                ctx_.reset();
            }
        }
        {
            std::lock_guard<std::mutex> lk(stream_mu_);
            stream_.reset();
        }
        stream_cv_.notify_all();  // wake read_thread_ so it loops back to wait

        if (running_) {
            std::printf("[GrpcEngineClient] Stream error. Reconnecting in 5s...\n");
            std::unique_lock<std::mutex> lk(hb_mu_);
            hb_cv_.wait_for(lk, std::chrono::seconds(5),
                            [this] { return !running_.load(); });
        }
    }

    std::printf("[GrpcEngineClient] write_thread_ exiting.\n");
}

// ---------------------------------------------------------------------------
// T017 / T019 — run_read()
//   Wait for stream, dispatch CapacityRequest / CapacityResponse
// ---------------------------------------------------------------------------

void GrpcEngineClient::run_read()
{
    while (running_) {
        // Wait until write_thread_ has opened a new stream.
        std::shared_ptr<Stream> local_stream;
        {
            std::unique_lock<std::mutex> lk(stream_mu_);
            stream_cv_.wait(lk, [this] {
                return stream_ != nullptr || !running_.load();
            });
            if (!running_) break;
            local_stream = stream_;
        }

        // Block on stream read; dispatch incoming proxy messages.
        ProxyMessage msg;
        while (local_stream->Read(&msg)) {
            switch (msg.payload_case()) {

            // --- T018 / T019: CapacityRequest → send CapacityReport (US3) ---
            case ProxyMessage::kCapacityRequest: {
                const std::string& req_id = msg.capacity_request().request_id();
                handle_capacity_request(local_stream.get(), req_id);
                break;
            }

            // --- T019: CapacityResponse — log only (US3) ---
            case ProxyMessage::kCapacityResponse: {
                std::printf("[GrpcEngineClient] CapacityResponse received: "
                            "request_id=%s available_capacity=%d\n",
                            msg.capacity_response().request_id().c_str(),
                            msg.capacity_response().available_capacity());
                break;
            }

            default:
                std::fprintf(stderr, "[GrpcEngineClient] Unknown ProxyMessage payload type.\n");
                break;
            }
        }
        // stream->Read() returned false: stream closed or cancelled.
    }

    std::printf("[GrpcEngineClient] read_thread_ exiting.\n");
}

// ---------------------------------------------------------------------------
// T018 — handle_capacity_request()   (US3)
// ---------------------------------------------------------------------------

void GrpcEngineClient::handle_capacity_request(Stream* stream,
                                                const std::string& request_id)
{
    int active_count = nserver_->get_active_proxy_count();

    EngineMessage report_msg;
    CapacityReport* report = report_msg.mutable_capacity_report();
    report->set_engine_id(cfg_.engine_id);
    report->set_max_capacity(cfg_.max_capacity);
    report->set_active_proxy_count(active_count);

    if (!stream->Write(report_msg)) {
        std::fprintf(stderr,
            "[GrpcEngineClient] CapacityReport write failed "
            "(request_id=%s).\n", request_id.c_str());
        return;
    }

    std::printf("[GrpcEngineClient] CapacityReport sent: "
                "request_id=%s max=%d active=%d\n",
                request_id.c_str(), cfg_.max_capacity, active_count);
}
