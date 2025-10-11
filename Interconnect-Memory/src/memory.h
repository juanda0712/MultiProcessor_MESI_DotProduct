#pragma once
#include <vector>
#include <cstdint>
#include <mutex>
#include <string>
#include <fstream>
#include <atomic>

// Internal atomic counters
struct MemStats {
    std::atomic<uint64_t> total_reads{0};
    std::atomic<uint64_t> total_writes{0};
    std::atomic<uint64_t> total_latency_us{0};
};

// Non-atomic snapshot for reporting
struct MemStatsSnapshot {
    uint64_t total_reads;
    uint64_t total_writes;
    uint64_t total_latency_us;
};

class MainMemory {
public:
    MainMemory(size_t slots = 512, uint32_t latency_us = 50, const std::string &logfile = "");
    uint64_t read(uint32_t byte_address, uint8_t requester);
    void write(uint32_t byte_address, uint64_t data, uint8_t requester);
    void load_initial_data(const std::vector<uint64_t> &data, uint32_t base_address);
    MemStatsSnapshot get_stats() const; // ← new type here
    void dump_region(uint32_t addr_start, uint32_t addr_end) const;

private:
    std::vector<uint64_t> mem_;
    mutable std::mutex mtx_;
    uint32_t latency_us_;
    std::ofstream log_;
    MemStats stats_;
    void log_access(const std::string &line) const;
};
