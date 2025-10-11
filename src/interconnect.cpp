#include "interconnect.hpp"
#include "cache.hpp"  // debe existir cache.hpp con on_snoop()

Interconnect::Interconnect(MainMemory* mem) : mem_(mem) {}

void Interconnect::attach(Cache* cache) {
    caches_.push_back(cache);
}

void Interconnect::writeback_from_evict(uint64_t base, const std::array<uint64_t, WORDS_PER_LINE>& line) {
    mem_->write_line(base, line);
}

BusResponse Interconnect::process(const BusRequest& req) {
    BusResponse rsp{};
    uint64_t base = line_base(req.addr);

    switch (req.cmd) {
        case BusCmd::BusRd:     return handle_BusRd(req.src_id, base);
        case BusCmd::BusRdX:    return handle_BusRdX(req.src_id, base);
        case BusCmd::Upgrade:   return handle_Upgrade(req.src_id, base);
        case BusCmd::WriteBack: 
            mem_->write_line(base, req.wline);
            rsp.grant = true;
            return rsp;
        default:
            return rsp;
    }
}

BusResponse Interconnect::handle_BusRd(int src, uint64_t base) {
    BusResponse rsp;
    rsp.grant = true;

    SnoopMessage sm{BusCmd::BusRd, base, src};
    bool any_hit = false, any_hitm = false;
    std::array<uint64_t, WORDS_PER_LINE> owner_line{};

    for (auto* c : caches_) {
        auto r = c->on_snoop(sm);
        if (r.hit) any_hit = true;
        if (r.hitM) {
            any_hitm = true;
            owner_line = r.wb;
        }
    }

    if (any_hitm) {
        mem_->write_line(base, owner_line);
        rsp.rline = owner_line;
        rsp.shared = true;
    } else if (any_hit) {
        rsp.rline = mem_->read_line(base);
        rsp.shared = true;
    } else {
        rsp.rline = mem_->read_line(base);
        rsp.shared = false;
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
        auto r = c->on_snoop(sm);
        if (r.invalidated) invacks++;
        if (r.hitM) {
            any_hitm = true;
            owner_line = r.wb;
        }
    }

    rsp.inv_ack_target = invacks;
    rsp.rline = any_hitm ? owner_line : mem_->read_line(base);
    return rsp;
}

BusResponse Interconnect::handle_Upgrade(int src, uint64_t base) {
    BusResponse rsp;
    rsp.grant = true;

    SnoopMessage sm{BusCmd::Upgrade, base, src};
    uint8_t invacks = 0;

    for (auto* c : caches_) {
        auto r = c->on_snoop(sm);
        if (r.invalidated) invacks++;
    }

    rsp.inv_ack_target = invacks;
    return rsp;
}
