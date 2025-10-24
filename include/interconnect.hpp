#pragma once
#include <vector>
#include <array>
#include <map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <future>  // NUEVO: necesario para std::promise
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
    size_t bus_wait_cycles = 0; // Nuevo: ciclos esperando por el bus
    
    void reset() {
        read_requests = 0;
        write_requests = 0;
        bus_transactions = 0;
        data_transferred = 0;
        cache_hits = 0;
        cache_misses = 0;
        invalidations_sent = 0;
        invalidations_received = 0;
        bus_wait_cycles = 0;
    }
};

// =====================================================
// Estructura para requests pendientes
// =====================================================
struct PendingRequest {
    BusRequest request;
    std::promise<BusResponse> promise;
    
    PendingRequest(BusRequest req) : request(req) {}
};

// =====================================================
// Clase Interconnect: simula el bus de coherencia MESI con arbitración real
// =====================================================
class Interconnect {
public:
    explicit Interconnect(MainMemory* mem);
    ~Interconnect();

    void attach(Cache* cache);
    BusResponse send_request(const BusRequest& req); // NUEVO: con arbitración
    void writeback_from_evict(uint64_t base, const std::array<uint64_t, WORDS_PER_LINE>& line);

    // Métodos para estadísticas
    void dump_stats() const;
    void dump_pe_stats() const;
    PEStats get_pe_stats(int pe_id) const;
    void record_cache_stats(int pe_id, size_t hits, size_t misses);

    // Control del sistema
    void stop_arbitration();

private:
    MainMemory* mem_;
    std::vector<Cache*> caches_;
    size_t bus_usage_; // Contador de uso del bus
    std::map<int, PEStats> pe_stats_; // Estadísticas por PE

    // Mecanismo de arbitración
    //std::queue<PendingRequest> request_queue_;
    std::deque<PendingRequest> request_queue_;
    std::mutex queue_mutex_;
    std::condition_variable arbitration_cv_;
    std::atomic<int> current_owner_{-1}; // -1 = bus libre
    std::atomic<bool> bus_busy_{false};
    std::atomic<bool> running_{false};
    
    // Hilo de arbitración
    std::thread arbitration_thread_;
    
    // Política de arbitración
    int last_granted_pe_{-1};
    size_t arbitration_cycles_{0};
    
    void arbitration_loop();
    BusResponse process_request(const BusRequest& req); // Renombrado de 'process'

    BusResponse handle_BusRd(int src, uint64_t base);
    BusResponse handle_BusRdX(int src, uint64_t base);
    BusResponse handle_Upgrade(int src, uint64_t base);
    
    void update_pe_stats(int pe_id, BusCmd cmd, size_t data_size = 0);
    
    // Políticas de arbitración
    int select_next_pe();
    void simulate_bus_latency();
};