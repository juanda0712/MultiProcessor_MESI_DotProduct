#include "memory.hpp"
#include <cstddef>
#include <iostream>
#include <fstream>

void MainMemory::initialize_dot_product_data(size_t baseA, size_t baseB, size_t partialAddr, size_t count) {
    // Inicializa A y B con valores simples y la suma parcial en 0
    for (size_t i = 0; i < count; ++i) {
        write_word(baseA + i * WORD_SIZE, 1 + i); // A[i] = 1, 2, 3, ...
        write_word(baseB + i * WORD_SIZE, 10 + i); // B[i] = 10, 11, 12, ...
    }
    write_word(partialAddr, 0); // suma parcial en 0
    
    std::cout << "[Memory] Initialized dot product data: baseA=0x" << std::hex << baseA 
              << ", baseB=0x" << baseB << ", partialAddr=0x" << partialAddr 
              << ", count=" << std::dec << count << std::endl;
}

MainMemory::MainMemory() : read_count_(0), write_count_(0) {
    for (size_t i = 0; i < MEM_WORDS; ++i)
        mem_[i] = 0; // Inicializar a 0 en lugar de valores arbitrarios
}

std::array<uint64_t, WORDS_PER_LINE> MainMemory::read_line(uint64_t base) {
    read_count_++;
    std::array<uint64_t, WORDS_PER_LINE> out{};
    size_t w0 = (base / WORD_SIZE) % MEM_WORDS;
    
    if (base % LINE_SIZE != 0) {
        std::cout << "[Memory] Warning: Unaligned line read at address 0x" 
                  << std::hex << base << std::dec << std::endl;
    }
    
    for (size_t i = 0; i < WORDS_PER_LINE; ++i)
        out[i] = mem_[(w0 + i) % MEM_WORDS];
    
    return out;
}

void MainMemory::write_line(uint64_t base, const std::array<uint64_t, WORDS_PER_LINE>& ln) {
    write_count_++;
    size_t w0 = (base / WORD_SIZE) % MEM_WORDS;
    
    if (base % LINE_SIZE != 0) {
        std::cout << "[Memory] Warning: Unaligned line write at address 0x" 
                  << std::hex << base << std::dec << std::endl;
    }
    
    for (size_t i = 0; i < WORDS_PER_LINE; ++i)
        mem_[(w0 + i) % MEM_WORDS] = ln[i];
}

uint64_t MainMemory::read_word(uint64_t addr) {
    read_count_++;
    size_t index = (addr / WORD_SIZE) % MEM_WORDS;
    return mem_[index];
}

void MainMemory::write_word(uint64_t addr, uint64_t data) {
    write_count_++;
    size_t index = (addr / WORD_SIZE) % MEM_WORDS;
    mem_[index] = data;
}

void MainMemory::dump_stats() const {
    std::cout << "=== Memory Statistics ===" << std::endl;
    std::cout << "Total reads: " << read_count_ << std::endl;
    std::cout << "Total writes: " << write_count_ << std::endl;
    std::cout << "Total accesses: " << (read_count_ + write_count_) << std::endl;
}

void MainMemory::dump_memory_range(uint64_t start_addr, uint64_t end_addr) const {
    std::cout << "=== Memory Dump (0x" << std::hex << start_addr 
              << " - 0x" << end_addr << ") ===" << std::dec << std::endl;
    
    for (uint64_t addr = start_addr; addr <= end_addr; addr += WORD_SIZE) {
        size_t index = (addr / WORD_SIZE) % MEM_WORDS;
        std::cout << "0x" << std::hex << addr << ": " << std::dec << mem_[index] << std::endl;
    }
}