#include "interconnect.hpp"
#include "cache.hpp"
#include <iostream>

Interconnect::Interconnect(MainMemory* mem) : mem_(mem), bus_usage_(0) {}

void Interconnect::attach(Cache* cache) {
    caches_.push_back(cache);
}

void Interconnect::writeback_from_evict(uint64_t base, const std::array<uint64_t, WORDS_PER_LINE>& line) {
    mem_->write_line(base, line);
    bus_usage_ += LINE_SIZE; // Tráfico de writeback
}

BusResponse Interconnect::process(const BusRequest& req) {
    bus_usage_++;
    
    BusResponse rsp{};
    uint64_t base = line_base(req.addr);

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
            rsp.grant = true;
            return rsp;
            
        default:
            std::cerr << "[Interconnect] Unknown bus command" << std::endl;
            return rsp;
    }
}

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
        }
        if (ret.hitM) {
            any_hitm = true;
            owner_line = ret.wb;
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
        if (ret.invalidated) invacks++;
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
        if (ret.invalidated) invacks++;
    }

    rsp.inv_ack_target = invacks;
    return rsp;
}

void Interconnect::dump_stats() const {
    std::cout << "=== Interconnect Statistics ===" << std::endl;
    std::cout << "Total bus transactions: " << bus_usage_ << std::endl;
    std::cout << "Attached caches: " << caches_.size() << std::endl;
}