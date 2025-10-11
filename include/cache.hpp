#pragma once
#include <vector>
#include <array>
#include <optional>
#include <cstdint>
#include <iostream>
#include "core.hpp"
#include "fsm.hpp"

class Interconnect;
struct MainMemory;

// ====================================
// Clase Cache
// ====================================
class Cache {
public:
    explicit Cache(int id, Interconnect* ic, MainMemory* mem);
    int id() const;

    uint64_t cpu_load(uint64_t addr);
    void cpu_store(uint64_t addr, uint64_t data);

    struct SnoopRet {
        bool hit = false;
        bool hitM = false;
        bool invalidated = false;
        std::array<uint64_t, WORDS_PER_LINE> wb{};
    };

    SnoopRet on_snoop(const SnoopMessage& sm);
    std::optional<std::array<uint64_t, WORDS_PER_LINE>> evict_if_M_and_get_line(uint64_t base);
    void dump_state();

private:
    struct CacheLine {
        bool valid{false};
        bool dirty{false};
        uint64_t tag{0};
        MESI state{MESI::I};
        std::array<uint64_t, WORDS_PER_LINE> data{};
    };

    struct PLRU {
        int bit{0};
        int pick() { int v = bit; bit ^= 1; return v; }
        void touch(int way) { bit = 1 - way; }
    };

    struct WayRef {
        int set = -1;
        int way = -1;
        CacheLine* cl = nullptr;
    };

    WayRef lookup(uint64_t base);
    WayRef find_free_or_victim(uint64_t base,
                               std::optional<uint64_t>& evict_base,
                               std::optional<std::array<uint64_t, WORDS_PER_LINE>>& evict_line,
                               bool& was_M);
    void install(uint64_t base, const std::array<uint64_t, WORDS_PER_LINE>& line, MESI st, bool dirty=false);

    static constexpr size_t NUM_SETS = 16;
    static constexpr size_t NUM_WAYS = 2;

    int id_;
    Interconnect* ic_;
    MainMemory* mem_;
    std::vector<std::array<CacheLine, NUM_WAYS>> sets_;
    std::vector<PLRU> lru_;

    uint64_t hits_{0}, misses_{0}, trans_{0};
};
