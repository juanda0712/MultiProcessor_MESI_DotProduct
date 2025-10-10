#pragma once
#include "message.h"
#include "memory.h"
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <atomic>
#include <cstdint>

class Interconnect {
public:
    Interconnect(MainMemory &mem, uint8_t num_pes = 4);
    ~Interconnect();

    // API for PEs to send requests
    void send_request(const BusMessage &msg);

    // Register a snoop callback for each PE (to notify their caches)
    // callback signature: void(const BusMessage&)
    void register_snoop_callback(uint8_t pe_id, std::function<void(const BusMessage&)> cb);

    // Start/stop threads
    void start();
    void stop();

    // metrics
    uint64_t get_total_transactions() const { return total_transactions_.load(); }

private:
    void arbitrator_loop();
    void process_request(const BusMessage &req);
    void broadcast_snoop(const BusMessage &snoop);
    void log(const std::string &line);

    MainMemory &mem_;
    uint8_t num_pes_;

    std::vector<std::deque<BusMessage>> queues_;
    std::vector<std::mutex> queue_mtx_;
    std::vector<std::condition_variable> queue_cv_;
    std::vector<std::function<void(const BusMessage&)>> snoop_cbs_;

    std::thread arb_thread_;
    std::atomic<bool> stop_flag_{false};
    std::atomic<uint64_t> total_transactions_{0};
    std::mutex log_mtx_;
    uint32_t next_msg_id_{1};
    uint32_t rr_ptr_{0}; // round-robin pointer
};
