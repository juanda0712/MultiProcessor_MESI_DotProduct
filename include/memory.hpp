#pragma once
#include <array>
#include <cstddef>
#include "core.hpp"

static constexpr size_t MEM_WORDS = 1024;

// =====================================================
// Clase MainMemory: memoria principal simulada
// =====================================================
class MainMemory {
public:
    MainMemory();

    std::array<uint64_t, WORDS_PER_LINE> read_line(uint64_t base);
    void write_line(uint64_t base, const std::array<uint64_t, WORDS_PER_LINE>& ln);

private:
    std::array<uint64_t, MEM_WORDS> mem_;
};
