#include "cache.hpp"
#include "interconnect.hpp"

// ============================
// Constructor
// ============================
Cache::Cache(int id, Interconnect* ic, MainMemory* mem)
    : id_(id), ic_(ic), mem_(mem), sets_(NUM_SETS), lru_(NUM_SETS) 
{
    for (size_t s = 0; s < NUM_SETS; ++s)
        for (size_t w = 0; w < NUM_WAYS; ++w)
            sets_[s][w] = CacheLine();
}

// ============================
// id
// ============================
int Cache::id() const { return id_; }

// ============================
// lookup
// ============================
Cache::WayRef Cache::lookup(uint64_t base) {
    size_t set = line_index(base) % NUM_SETS;
    for (int w = 0; w < NUM_WAYS; ++w) {
        CacheLine& cl = sets_[set][w];
        if (cl.valid && cl.tag == base && cl.state != MESI::I)
            return WayRef{(int)set, w, &cl};
    }
    return WayRef{};
}

// ============================
// evict_if_M_and_get_line
// ============================
std::optional<std::array<uint64_t, WORDS_PER_LINE>> Cache::evict_if_M_and_get_line(uint64_t base) {
    WayRef wr = lookup(base);
    if (wr.cl && wr.cl->state == MESI::M) {
        auto line = wr.cl->data;
        wr.cl->dirty = false;
        wr.cl->valid = true;
        wr.cl->state = MESI::I;
        return line;
    }
    return std::nullopt;
}

// ============================
// find_free_or_victim
// ============================
Cache::WayRef Cache::find_free_or_victim(uint64_t base,
                                         std::optional<uint64_t>& evict_base,
                                         std::optional<std::array<uint64_t, WORDS_PER_LINE>>& evict_line,
                                         bool& was_M) 
{
    size_t set = line_index(base) % NUM_SETS;

    for (int w = 0; w < NUM_WAYS; ++w) {
        CacheLine& cl = sets_[set][w];
        if (!cl.valid || cl.state == MESI::I)
            return WayRef{(int)set, w, &cl};
    }

    int vic = lru_[set].pick();
    CacheLine& v = sets_[set][vic];
    evict_base.reset();
    evict_line.reset();
    was_M = false;

    if (v.valid && v.state == MESI::M) {
        was_M = true;
        evict_base = v.tag;
        evict_line = v.data;
    }

    v.valid = false;
    v.dirty = false;
    v.state = MESI::I;

    return WayRef{(int)set, vic, &v};
}

// ============================
// install
// ============================
void Cache::install(uint64_t base, const std::array<uint64_t, WORDS_PER_LINE>& line, MESI st, bool dirty) {
    std::optional<uint64_t> ebase;
    std::optional<std::array<uint64_t, WORDS_PER_LINE>> eline;
    bool wasM = false;

    WayRef w = find_free_or_victim(base, ebase, eline, wasM);
    if (w.cl) {
        w.cl->valid = true;
        w.cl->dirty = dirty;
        w.cl->tag = base;
        w.cl->data = line;
        w.cl->state = st;
        lru_[w.set].touch(w.way);
    }
}

// ============================
// cpu_load
// ============================
uint64_t Cache::cpu_load(uint64_t addr) {
    uint64_t base = line_base(addr);
    WayRef w = lookup(base);
    size_t word_off = ((addr - base) / WORD_SIZE) % WORDS_PER_LINE;

    if (w.cl) { hits_++; return w.cl->data[word_off]; }

    misses_++;
    std::optional<uint64_t> ebase;
    std::optional<std::array<uint64_t, WORDS_PER_LINE>> eline;
    bool wasM = false;

    find_free_or_victim(base, ebase, eline, wasM);
    if (ebase && wasM) ic_->writeback_from_evict(*ebase, *eline);

    BusRequest br{BusCmd::BusRd, base, id_};
    BusResponse resp = ic_->process(br);

    install(base, resp.rline, resp.shared ? MESI::S : MESI::E, false);
    trans_++;

    WayRef w2 = lookup(base);
    return w2.cl->data[word_off];
}

// ============================
// cpu_store
// ============================
void Cache::cpu_store(uint64_t addr, uint64_t data) {
    uint64_t base = line_base(addr);
    WayRef w = lookup(base);
    size_t word_off = ((addr - base) / WORD_SIZE) % WORDS_PER_LINE;

    if (w.cl) {
        if (w.cl->state == MESI::S) {
            BusRequest up{BusCmd::Upgrade, base, id_};
            BusResponse r = ic_->process(up);
            (void)r;
            w.cl->state = MESI::M;
            w.cl->dirty = true;
            trans_++;
            w.cl->data[word_off] = data;
            return;
        }
        if (w.cl->state == MESI::E) {
            w.cl->state = MESI::M;
            w.cl->dirty = true;
            trans_++;
            w.cl->data[word_off] = data;
            return;
        }
        if (w.cl->state == MESI::M) {
            w.cl->dirty = true;
            w.cl->data[word_off] = data;
            return;
        }
    }

    std::optional<uint64_t> ebase;
    std::optional<std::array<uint64_t, WORDS_PER_LINE>> eline;
    bool wasM = false;

    find_free_or_victim(base, ebase, eline, wasM);
    if (ebase && wasM) ic_->writeback_from_evict(*ebase, *eline);

    BusRequest rfo{BusCmd::BusRdX, base, id_};
    BusResponse resp = ic_->process(rfo);

    install(base, resp.rline, MESI::M, true);

    WayRef w2 = lookup(base);
    w2.cl->data[word_off] = data;
    trans_++;
}

// ============================
// on_snoop
// ============================
Cache::SnoopRet Cache::on_snoop(const SnoopMessage& sm) {
    SnoopRet out{};
    uint64_t base = line_base(sm.addr);
    WayRef w = lookup(base);
    if (!w.cl) return out;

    CacheLine& cl = *w.cl;
    switch (sm.cmd) {
        case BusCmd::BusRd:
            if (cl.state == MESI::M) {
                out.hit = true; out.hitM = true; out.wb = cl.data;
                cl.dirty = false; cl.state = MESI::S; trans_++;
            } else if (cl.state == MESI::E) {
                out.hit = true; cl.state = MESI::S; trans_++;
            } else if (cl.state == MESI::S) {
                out.hit = true;
            }
            break;

        case BusCmd::BusRdX:
        case BusCmd::Upgrade:
            if (cl.state == MESI::M) {
                out.hit = true; out.hitM = true; out.wb = cl.data;
                cl.dirty = false; cl.state = MESI::I; cl.valid = true; out.invalidated = true; trans_++;
            } else if (cl.state == MESI::E || cl.state == MESI::S) {
                out.hit = true; cl.state = MESI::I; cl.valid = true; out.invalidated = true; trans_++;
            }
            break;

        default:
            break;
    }
    return out;
}

// ============================
// dump_state
// ============================
void Cache::dump_state() {
    std::cout << "PE" << id_ << " dump:\n";
    for (int s = 0; s < (int)NUM_SETS; ++s) {
        for (int w = 0; w < (int)NUM_WAYS; ++w) {
            const CacheLine& cl = sets_[s][w];
            if (cl.valid && cl.state != MESI::I) {
                std::cout << "  set " << s
                          << " way " << w
                          << " base 0x" << std::hex << cl.tag << std::dec
                          << " state " << mesi_str(cl.state)
                          << " dirty " << cl.dirty << "\n";
            }
        }
    }
}
