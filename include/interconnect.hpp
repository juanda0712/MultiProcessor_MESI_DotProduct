#pragma once
#include <vector>
#include <array>
#include "core.hpp"
#include "memory.hpp"

class Cache; // Forward declaration

// =====================================================
// Clase Interconnect: simula el bus de coherencia MESI
// =====================================================
class Interconnect {
public:
    explicit Interconnect(MainMemory* mem);

    void attach(Cache* cache);
    BusResponse process(const BusRequest& req);
    void writeback_from_evict(uint64_t base, const std::array<uint64_t, WORDS_PER_LINE>& line);

private:
    MainMemory* mem_;
    std::vector<Cache*> caches_;

    BusResponse handle_BusRd(int src, uint64_t base);
    BusResponse handle_BusRdX(int src, uint64_t base);
    BusResponse handle_Upgrade(int src, uint64_t base);
};
