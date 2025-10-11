#include "memory.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

MainMemory::MainMemory(size_t slots, uint32_t latency_us, const std::string &logfile)
    : mem_(slots, 0), latency_us_(latency_us)
{
    if (!logfile.empty()) log_.open(logfile, std::ios::out);
}

void MainMemory::log_access(const std::string &line) const {
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now().time_since_epoch()).count();
    std::ostringstream oss;
    oss << "[" << ts << " ms] MEMORY: " << line << "\n";
    std::string s = oss.str();
    if (log_.is_open()) {
        // const-cast to write (log_ is non-const member but method is const)
        const_cast<std::ofstream&>(log_) << s;
    }
    std::cout << s;
}

uint64_t MainMemory::read(uint32_t byte_address, uint8_t requester) {
    if (byte_address % 8 != 0) {
        std::ostringstream tmp;
        tmp << "UNALIGNED READ addr=0x" << std::hex << byte_address;
        log_access(tmp.str());
    }

    std::unique_lock<std::mutex> lk(mtx_);
    // simulate latency
    std::this_thread::sleep_for(std::chrono::microseconds(latency_us_));

    size_t idx = static_cast<size_t>(byte_address / 8);
    if (idx >= mem_.size()) {
        std::ostringstream tmp;
        tmp << "READ OUT-OF-RANGE addr=0x" << std::hex << byte_address;
        log_access(tmp.str());
        return 0;
    }
    uint64_t data = mem_[idx];
    stats_.total_reads.fetch_add(1, std::memory_order_relaxed);
    stats_.total_latency_us.fetch_add(latency_us_, std::memory_order_relaxed);
    lk.unlock();

    std::ostringstream l;
    l << "READ by PE" << std::dec << int(requester)
      << " addr=0x" << std::hex << byte_address
      << " -> data=0x" << std::hex << data;
    log_access(l.str());
    return data;
}

void MainMemory::write(uint32_t byte_address, uint64_t data, uint8_t requester) {
    if (byte_address % 8 != 0) {
        std::ostringstream tmp;
        tmp << "UNALIGNED WRITE addr=0x" << std::hex << byte_address;
        log_access(tmp.str());
    }

    std::unique_lock<std::mutex> lk(mtx_);
    std::this_thread::sleep_for(std::chrono::microseconds(latency_us_));

    size_t idx = static_cast<size_t>(byte_address / 8);
    if (idx >= mem_.size()) {
        std::ostringstream tmp;
        tmp << "WRITE OUT-OF-RANGE addr=0x" << std::hex << byte_address;
        log_access(tmp.str());
        return;
    }
    mem_[idx] = data;
    stats_.total_writes.fetch_add(1, std::memory_order_relaxed);
    stats_.total_latency_us.fetch_add(latency_us_, std::memory_order_relaxed);
    lk.unlock();

    std::ostringstream l;
    l << "WRITE by PE" << std::dec << int(requester)
      << " addr=0x" << std::hex << byte_address
      << " <- data=0x" << std::hex << data;
    log_access(l.str());
}

void MainMemory::load_initial_data(const std::vector<uint64_t> &data, uint32_t base_address) {
    std::unique_lock<std::mutex> lk(mtx_);
    size_t start = static_cast<size_t>(base_address / 8);
    for (size_t i = 0; i < data.size(); ++i) {
        if (start + i < mem_.size()) mem_[start + i] = data[i];
    }
    lk.unlock();
    std::ostringstream l;
    l << "Loaded " << data.size() << " words at base addr=0x" << std::hex << base_address;
    log_access(l.str());
}

MemStatsSnapshot MainMemory::get_stats() const {
    MemStatsSnapshot snap{};
    snap.total_reads      = stats_.total_reads.load(std::memory_order_relaxed);
    snap.total_writes     = stats_.total_writes.load(std::memory_order_relaxed);
    snap.total_latency_us = stats_.total_latency_us.load(std::memory_order_relaxed);
    return snap;
}


void MainMemory::dump_region(uint32_t addr_start, uint32_t addr_end) const {
    std::unique_lock<std::mutex> lk(mtx_);
    size_t s = static_cast<size_t>(addr_start / 8);
    size_t e = static_cast<size_t>(addr_end / 8);
    if (s >= mem_.size()) return;
    e = std::min(e, mem_.size()-1);
    std::cout << "MEM DUMP 0x" << std::hex << addr_start << " - 0x" << addr_end << "\n";
    for (size_t i = s; i <= e; ++i) {
        std::cout << "  [" << std::dec << i << "] 0x" << std::hex << mem_[i] << "\n";
    }
}
