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

    // Inicializa los vectores A, B y la suma parcial en memoria
    void initialize_dot_product_data(size_t baseA, size_t baseB, size_t partialAddr, size_t count);

private:
    std::array<uint64_t, MEM_WORDS> mem_;
};
