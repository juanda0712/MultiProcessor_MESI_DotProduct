#pragma once
#include <vector>
#include <array>
#include <map>
#include "core.hpp"
#include "memory.hpp"

class Cache; // Forward declaration

// =====================================================
// Estructura para estadísticas por PE
// =====================================================
struct PEStats {
    size_t read_requests = 0;
    size_t write_requests = 0;
    size_t bus_transactions = 0;
    size_t data_transferred = 0; // en bytes
    size_t cache_hits = 0;
    size_t cache_misses = 0;
    size_t invalidations_sent = 0;
    size_t invalidations_received = 0;
    
    void reset() {
        read_requests = 0;
        write_requests = 0;
        bus_transactions = 0;
        data_transferred = 0;
        cache_hits = 0;
        cache_misses = 0;
        invalidations_sent = 0;
        invalidations_received = 0;
    }
};

// =====================================================
// Clase Interconnect: simula el bus de coherencia MESI
// =====================================================
class Interconnect {
public:
    explicit Interconnect(MainMemory* mem);

    void attach(Cache* cache);
    BusResponse process(const BusRequest& req);
    void writeback_from_evict(uint64_t base, const std::array<uint64_t, WORDS_PER_LINE>& line);

    // Métodos para estadísticas
    void dump_stats() const;
    void dump_pe_stats() const;
    PEStats get_pe_stats(int pe_id) const;
    void record_cache_stats(int pe_id, size_t hits, size_t misses);

private:
    MainMemory* mem_;
    std::vector<Cache*> caches_;
    size_t bus_usage_; // Contador de uso del bus
    std::map<int, PEStats> pe_stats_; // Estadísticas por PE

    BusResponse handle_BusRd(int src, uint64_t base);
    BusResponse handle_BusRdX(int src, uint64_t base);
    BusResponse handle_Upgrade(int src, uint64_t base);
    
    void update_pe_stats(int pe_id, BusCmd cmd, size_t data_size = 0);
};