#include "memory.hpp"

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
