#include "interconnect.h"
#include <iostream>
#include <sstream>
#include <chrono>

static std::string msgtype_to_string(MsgType t) {
    switch (t) {
        case MsgType::READ: return "READ";
        case MsgType::READX: return "READX";
        case MsgType::WRITE: return "WRITE";
        case MsgType::INVALIDATE: return "INVALIDATE";
        case MsgType::WRITEBACK: return "WRITEBACK";
        case MsgType::RESPONSE: return "RESPONSE";
        default: return "UNKNOWN";
    }
}

Interconnect::Interconnect(MainMemory &mem, uint8_t num_pes)
    : mem_(mem), num_pes_(num_pes),
      queues_(num_pes), queue_mtx_(num_pes), queue_cv_(num_pes),
      snoop_cbs_(num_pes)
{}

Interconnect::~Interconnect() {
    stop();
}

void Interconnect::start() {
    stop_flag_ = false;
    arb_thread_ = std::thread(&Interconnect::arbitrator_loop, this);
}

void Interconnect::stop() {
    stop_flag_ = true;
    // notify all queues so thread can exit
    for (auto &cv : queue_cv_) cv.notify_all();
    if (arb_thread_.joinable()) arb_thread_.join();
}

void Interconnect::log(const std::string &line) {
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now().time_since_epoch()).count();
    std::unique_lock<std::mutex> lk(log_mtx_);
    std::cout << "[" << ts << " ms] INTERCONNECT: " << line << "\n";
}

void Interconnect::send_request(const BusMessage &msg) {
    if (msg.src >= num_pes_) {
        log("send_request: invalid src");
        return;
    }
    {
        std::unique_lock<std::mutex> lk(queue_mtx_[msg.src]);
        queues_[msg.src].push_back(msg);
    }
    queue_cv_[msg.src].notify_one();
    std::ostringstream oss;
    oss << "Enqueued request from PE" << int(msg.src) << " type=" << msgtype_to_string(msg.type)
        << " addr=0x" << std::hex << msg.address;
    log(oss.str());
}

void Interconnect::register_snoop_callback(uint8_t pe_id, std::function<void(const BusMessage&)> cb) {
    if (pe_id < num_pes_) snoop_cbs_[pe_id] = cb;
}

void Interconnect::arbitrator_loop() {
    log("Arbitrator started");
    while (!stop_flag_) {
        bool processed = false;
        for (uint8_t i = 0; i < num_pes_; ++i) {
            uint8_t idx = (rr_ptr_ + i) % num_pes_;
            std::unique_lock<std::mutex> lk(queue_mtx_[idx]);
            if (!queues_[idx].empty()) {
                BusMessage req = queues_[idx].front();
                queues_[idx].pop_front();
                lk.unlock();
                rr_ptr_ = (idx + 1) % num_pes_;
                process_request(req);
                processed = true;
                break; // process one request per arbitration cycle
            }
            // else continue to next
        }
        if (!processed) {
            // no requests, brief sleep to avoid busy wait
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    }
    log("Arbitrator stopped");
}

void Interconnect::broadcast_snoop(const BusMessage &snoop) {
    for (uint8_t pid = 0; pid < num_pes_; ++pid) {
        if (snoop_cbs_[pid]) {
            // synchronous call for simplicity
            snoop_cbs_[pid](snoop);
        }
    }
}

void Interconnect::process_request(const BusMessage &req) {
    total_transactions_.fetch_add(1, std::memory_order_relaxed);

    {
        std::ostringstream oss;
        oss << "Processing req msg_id=" << req.msg_id
            << " from PE" << int(req.src)
            << " type=" << msgtype_to_string(req.type)
            << " addr=0x" << std::hex << req.address;
        log(oss.str());
    }

    if (req.type == MsgType::READ || req.type == MsgType::READX) {
        // broadcast snoop (BUS_READ / BUS_READX)
        BusMessage snoop = req;
        snoop.msg_id = next_msg_id_++;
        snoop.type = req.type; // keep READ or READX
        broadcast_snoop(snoop);

        // For basic implementation: read from memory
        uint64_t data = mem_.read(req.address, req.src);

        // send RESPONSE back to requester via its callback
        BusMessage resp;
        resp.msg_id = next_msg_id_++;
        resp.src = 0xFF; // interconnect/memory
        resp.type = MsgType::RESPONSE;
        resp.address = req.address;
        resp.size = req.size;
        resp.data = data;
        resp.timestamp = 0;
        if (req.src < snoop_cbs_.size() && snoop_cbs_[req.src]) {
            snoop_cbs_[req.src](resp);
        } else {
            log("No callback registered for requester to receive response");
        }
    } else if (req.type == MsgType::WRITE) {
        // Broadcast invalidation to caches
        BusMessage inval = req;
        inval.msg_id = next_msg_id_++;
        inval.type = MsgType::INVALIDATE;
        broadcast_snoop(inval);

        // write to memory (simplified model)
        mem_.write(req.address, req.data, req.src);

        // send ack/response to requester
        BusMessage resp;
        resp.msg_id = next_msg_id_++;
        resp.src = 0xFF;
        resp.type = MsgType::RESPONSE;
        resp.address = req.address;
        resp.size = req.size;
        resp.data = req.data;
        if (req.src < snoop_cbs_.size() && snoop_cbs_[req.src]) {
            snoop_cbs_[req.src](resp);
        }
    } else {
        log("Request type not fully handled (type=" + msgtype_to_string(req.type) + ")");
    }
}
