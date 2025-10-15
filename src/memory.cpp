#include "memory.hpp"
#include <cstddef>

void MainMemory::initialize_dot_product_data(size_t baseA, size_t baseB, size_t partialAddr, size_t count) {
    // Inicializa A y B con valores simples y la suma parcial en 0
    for (size_t i = 0; i < count; ++i) {
        mem_[(baseA / WORD_SIZE) + i] = 1 + i; // A[i] = 1, 2, 3, ...
        mem_[(baseB / WORD_SIZE) + i] = 10 + i; // B[i] = 10, 11, 12, ...
    }
    mem_[partialAddr / WORD_SIZE] = 0; // suma parcial en 0
}


MainMemory::MainMemory() {
    for (size_t i = 0; i < MEM_WORDS; ++i)
        mem_[i] = i * 1111ULL;
}

std::array<uint64_t, WORDS_PER_LINE> MainMemory::read_line(uint64_t base) {
    std::array<uint64_t, WORDS_PER_LINE> out{};
    size_t w0 = (base / WORD_SIZE) % MEM_WORDS;
    for (size_t i = 0; i < WORDS_PER_LINE; ++i)
        out[i] = mem_[(w0 + i) % MEM_WORDS];
    return out;
}

void MainMemory::write_line(uint64_t base, const std::array<uint64_t, WORDS_PER_LINE>& ln) {
    size_t w0 = (base / WORD_SIZE) % MEM_WORDS;
    for (size_t i = 0; i < WORDS_PER_LINE; ++i)
        mem_[(w0 + i) % MEM_WORDS] = ln[i];
}
