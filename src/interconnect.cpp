#include "interconnect.hpp"
#include "cache.hpp"
#include <iostream>
#include <chrono>
#include <random>

// Función helper para convertir BusCmd a string
static const char* bus_cmd_str(BusCmd cmd) {
    switch (cmd) {
        case BusCmd::BusRd: return "BusRd";
        case BusCmd::BusRdX: return "BusRdX";
        case BusCmd::Upgrade: return "Upgrade";
        case BusCmd::WriteBack: return "WriteBack";
        default: return "Unknown";
    }
}

Interconnect::Interconnect(MainMemory* mem) : mem_(mem), bus_usage_(0) {
    // Inicializar estadísticas para PEs 0-3
    for (int i = 0; i < 4; ++i) {
        pe_stats_[i] = PEStats();
    }
    
    // Iniciar hilo de arbitración
    running_ = true;
    arbitration_thread_ = std::thread(&Interconnect::arbitration_loop, this);
    
    std::cout << "[Interconnect] Arbitrador iniciado" << std::endl;
}

Interconnect::~Interconnect() {
    stop_arbitration();
}

void Interconnect::stop_arbitration() {
    running_ = false;
    arbitration_cv_.notify_all();
    if (arbitration_thread_.joinable()) {
        arbitration_thread_.join();
    }
    std::cout << "[Interconnect] Arbitrador detenido" << std::endl;
}

void Interconnect::arbitration_loop() {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    while (running_) {
        std::unique_ptr<PendingRequest> pending_req = nullptr;
        bool has_request = false;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            // Esperar por requests o señal de terminación
            arbitration_cv_.wait_for(lock, std::chrono::milliseconds(50), 
                [this]() { return !request_queue_.empty() || !running_; });
            
            if (!running_) break;
            
            if (!request_queue_.empty()) {
                pending_req = std::make_unique<PendingRequest>(std::move(request_queue_.front()));
                request_queue_.pop();
                has_request = true;
            }
        }
        
        if (has_request && pending_req) {
            arbitration_cycles_++;
            
            // POLÍTICA DE ARBITRACIÓN: Round Robin
            int selected_pe = select_next_pe();
            
            // Si hay conflicto, el PE seleccionado puede no ser el solicitante
            // En este caso, el solicitante original debe esperar
            if (selected_pe != pending_req->request.src_id) {
                std::cout << "[Arbitration] Conflict: PE" << pending_req->request.src_id 
                          << " must wait, granting bus to PE" << selected_pe << std::endl;
                
                // Re-encolar la request del PE que no fue seleccionado
                {
                    std::lock_guard<std::mutex> lock(queue_mutex_);
                    request_queue_.push(std::move(*pending_req));
                }
                continue;
            }
            
            // Adquirir el bus
            bus_busy_.store(true);
            current_owner_.store(pending_req->request.src_id);
            
            std::cout << "[Arbitration] PE" << pending_req->request.src_id 
                      << " granted bus for " << bus_cmd_str(pending_req->request.cmd) 
                      << " @ 0x" << std::hex << pending_req->request.addr << std::dec << std::endl;
            
            // Simular latencia del bus
            simulate_bus_latency();
            
            // Procesar la request
            BusResponse response = process_request(pending_req->request);
            
            // Liberar el bus
            current_owner_.store(-1);
            bus_busy_.store(false);
            
            // Cumplir la promesa
            pending_req->promise.set_value(response);
            
            std::cout << "[Arbitration] PE" << pending_req->request.src_id 
                      << " released bus" << std::endl;
            
            // Pequeña pausa entre transacciones
            std::this_thread::sleep_for(std::chrono::microseconds(5));
        }
    }
}

int Interconnect::select_next_pe() {
    // Política Round Robin simple
    // En un sistema real, esto podría ser más complejo (prioridades, etc.)
    
    if (last_granted_pe_ == -1) {
        // Primera concesión - comenzar con PE0
        last_granted_pe_ = 0;
        return 0;
    }
    
    // Siguiente PE en round-robin
    last_granted_pe_ = (last_granted_pe_ + 1) % 4;
    return last_granted_pe_;
}

void Interconnect::simulate_bus_latency() {
    // Simular latencia variable del bus (2-5 ciclos)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(2, 5);
    int latency_cycles = dist(gen);
    
    // Registrar ciclos de espera para estadísticas
    if (pe_stats_.find(current_owner_.load()) != pe_stats_.end()) {
        pe_stats_[current_owner_.load()].bus_wait_cycles += latency_cycles;
    }
    
    // Simular la latencia
    std::this_thread::sleep_for(std::chrono::microseconds(latency_cycles));
}

BusResponse Interconnect::send_request(const BusRequest& req) {
    // Verificar si el bus está ocupado
    if (bus_busy_.load()) {
        std::cout << "[Interconnect] PE" << req.src_id << " waiting for bus (busy with PE" 
                  << current_owner_.load() << ")" << std::endl;
    }
    
    // Crear promise para la respuesta
    std::promise<BusResponse> promise;
    std::future<BusResponse> future = promise.get_future();
    
    PendingRequest pending_req(req);
    pending_req.promise = std::move(promise);
    
    // Agregar request a la cola
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        request_queue_.push(std::move(pending_req));
    }
    
    // Notificar al arbitrador
    arbitration_cv_.notify_one();
    
    // Esperar y retornar la respuesta (bloqueante)
    return future.get();
}

void Interconnect::attach(Cache* cache) {
    caches_.push_back(cache);
}

BusResponse Interconnect::process_request(const BusRequest& req) {
    bus_usage_++;
    
    BusResponse rsp{};
    uint64_t base = line_base(req.addr);

    // Registrar estadísticas del PE solicitante
    update_pe_stats(req.src_id, req.cmd, LINE_SIZE);

    switch (req.cmd) {
        case BusCmd::BusRd:     
            std::cout << "[Interconnect] BusRd for base 0x" << std::hex << base 
                      << " from PE" << std::dec << req.src_id << std::endl;
            return handle_BusRd(req.src_id, base);
            
        case BusCmd::BusRdX:    
            std::cout << "[Interconnect] BusRdX for base 0x" << std::hex << base 
                      << " from PE" << std::dec << req.src_id << std::endl;
            return handle_BusRdX(req.src_id, base);
            
        case BusCmd::Upgrade:   
            std::cout << "[Interconnect] Upgrade for base 0x" << std::hex << base 
                      << " from PE" << std::dec << req.src_id << std::endl;
            return handle_Upgrade(req.src_id, base);
            
        case BusCmd::WriteBack: 
            mem_->write_line(base, req.wline);
            bus_usage_ += LINE_SIZE; // Tráfico de writeback
            update_pe_stats(req.src_id, req.cmd, LINE_SIZE);
            rsp.grant = true;
            return rsp;
            
        default:
            std::cerr << "[Interconnect] Unknown bus command" << std::endl;
            return rsp;
    }
}

// Los métodos handle_BusRd, handle_BusRdX, handle_Upgrade permanecen igual que antes
BusResponse Interconnect::handle_BusRd(int src, uint64_t base) {
    BusResponse rsp;
    rsp.grant = true;

    SnoopMessage sm{BusCmd::BusRd, base, src};
    bool any_hit = false, any_hitm = false;
    std::array<uint64_t, WORDS_PER_LINE> owner_line{};
    int hit_count = 0;

    for (auto* c : caches_) {
        if (c->id() == src) continue; // No snoopear al solicitante
        
        auto ret = c->on_snoop(sm);
        if (ret.hit) {
            any_hit = true;
            hit_count++;
            // Registrar invalidación recibida para la cache snoopeada
            if (pe_stats_.find(c->id()) != pe_stats_.end()) {
                pe_stats_[c->id()].invalidations_received++;
            }
        }
        if (ret.hitM) {
            any_hitm = true;
            owner_line = ret.wb;
            // Registrar invalidación enviada
            pe_stats_[src].invalidations_sent++;
        }
    }

    if (any_hitm) {
        // Línea Modified encontrada, escribir back a memoria y proporcionar datos
        mem_->write_line(base, owner_line);
        rsp.rline = owner_line;
        rsp.shared = (hit_count > 0);
        bus_usage_ += LINE_SIZE; // Tráfico de transferencia de línea
    } else if (any_hit) {
        // Línea Shared encontrada, leer de memoria
        rsp.rline = mem_->read_line(base);
        rsp.shared = true;
        bus_usage_ += LINE_SIZE; // Tráfico de lectura de memoria
    } else {
        // No hay copias, leer de memoria exclusivamente
        rsp.rline = mem_->read_line(base);
        rsp.shared = false;
        bus_usage_ += LINE_SIZE; // Tráfico de lectura de memoria
    }

    return rsp;
}

BusResponse Interconnect::handle_BusRdX(int src, uint64_t base) {
    BusResponse rsp;
    rsp.grant = true;

    SnoopMessage sm{BusCmd::BusRdX, base, src};
    uint8_t invacks = 0;
    bool any_hitm = false;
    std::array<uint64_t, WORDS_PER_LINE> owner_line{};

    for (auto* c : caches_) {
        if (c->id() == src) continue;
        
        auto ret = c->on_snoop(sm);
        if (ret.invalidated) {
            invacks++;
            // Registrar invalidaciones
            if (pe_stats_.find(c->id()) != pe_stats_.end()) {
                pe_stats_[c->id()].invalidations_received++;
            }
            pe_stats_[src].invalidations_sent++;
        }
        if (ret.hitM) {
            any_hitm = true;
            owner_line = ret.wb;
        }
    }

    rsp.inv_ack_target = invacks;
    
    if (any_hitm) {
        rsp.rline = owner_line;
        bus_usage_ += LINE_SIZE; // Tráfico de transferencia de línea
    } else {
        rsp.rline = mem_->read_line(base);
        bus_usage_ += LINE_SIZE; // Tráfico de lectura de memoria
    }
    
    return rsp;
}

BusResponse Interconnect::handle_Upgrade(int src, uint64_t base) {
    BusResponse rsp;
    rsp.grant = true;

    SnoopMessage sm{BusCmd::Upgrade, base, src};
    uint8_t invacks = 0;

    for (auto* c : caches_) {
        if (c->id() == src) continue;
        
        auto ret = c->on_snoop(sm);
        if (ret.invalidated) {
            invacks++;
            // Registrar invalidaciones
            if (pe_stats_.find(c->id()) != pe_stats_.end()) {
                pe_stats_[c->id()].invalidations_received++;
            }
            pe_stats_[src].invalidations_sent++;
        }
    }

    rsp.inv_ack_target = invacks;
    return rsp;
}

void Interconnect::update_pe_stats(int pe_id, BusCmd cmd, size_t data_size) {
    if (pe_stats_.find(pe_id) == pe_stats_.end()) {
        pe_stats_[pe_id] = PEStats();
    }
    
    auto& stats = pe_stats_[pe_id];
    stats.bus_transactions++;
    stats.data_transferred += data_size;
    
    switch (cmd) {
        case BusCmd::BusRd:
            stats.read_requests++;
            break;
        case BusCmd::BusRdX:
        case BusCmd::Upgrade:
            stats.write_requests++;
            break;
        default:
            break;
    }
}

void Interconnect::writeback_from_evict(uint64_t base, const std::array<uint64_t, WORDS_PER_LINE>& line) {
    mem_->write_line(base, line);
    bus_usage_ += LINE_SIZE; // Tráfico de writeback
}

void Interconnect::record_cache_stats(int pe_id, size_t hits, size_t misses) {
    if (pe_stats_.find(pe_id) != pe_stats_.end()) {
        pe_stats_[pe_id].cache_hits = hits;
        pe_stats_[pe_id].cache_misses = misses;
    }
}

void Interconnect::dump_stats() const {
    std::cout << "=== Interconnect Statistics ===" << std::endl;
    std::cout << "Total bus transactions: " << bus_usage_ << std::endl;
    std::cout << "Arbitration cycles: " << arbitration_cycles_ << std::endl;
    std::cout << "Attached caches: " << caches_.size() << std::endl;
    std::cout << "Current bus owner: PE" << current_owner_.load() << std::endl;
    std::cout << "Bus busy: " << (bus_busy_.load() ? "Yes" : "No") << std::endl;
}

void Interconnect::dump_pe_stats() const {
    std::cout << "=== PE Statistics ===" << std::endl;
    for (const auto& [pe_id, stats] : pe_stats_) {
        std::cout << "PE" << pe_id << ":" << std::endl;
        std::cout << "  Read requests: " << stats.read_requests << std::endl;
        std::cout << "  Write requests: " << stats.write_requests << std::endl;
        std::cout << "  Bus transactions: " << stats.bus_transactions << std::endl;
        std::cout << "  Data transferred: " << stats.data_transferred << " bytes" << std::endl;
        std::cout << "  Cache hits: " << stats.cache_hits << std::endl;
        std::cout << "  Cache misses: " << stats.cache_misses << std::endl;
        std::cout << "  Hit ratio: " << (stats.cache_hits + stats.cache_misses > 0 ? 
                  (static_cast<double>(stats.cache_hits) / (stats.cache_hits + stats.cache_misses)) * 100.0 : 0.0) 
                  << "%" << std::endl;
        std::cout << "  Invalidations sent: " << stats.invalidations_sent << std::endl;
        std::cout << "  Invalidations received: " << stats.invalidations_received << std::endl;
        std::cout << "  Bus wait cycles: " << stats.bus_wait_cycles << std::endl;
        std::cout << std::endl;
    }
}

PEStats Interconnect::get_pe_stats(int pe_id) const {
    auto it = pe_stats_.find(pe_id);
    if (it != pe_stats_.end()) {
        return it->second;
    }
    return PEStats();
}