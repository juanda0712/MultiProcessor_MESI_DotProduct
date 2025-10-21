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
    
    // Definir segmentos por defecto para el producto punto
    add_segment(0x0000, 0x0400, "PE0_Data");    // Datos PE0: 1KB
    add_segment(0x0100, 0x0400, "PE1_Data");    // Datos PE1: 1KB  
    add_segment(0x0200, 0x0400, "PE2_Data");    // Datos PE2: 1KB
    add_segment(0x0300, 0x0400, "PE3_Data");    // Datos PE3: 1KB
    add_segment(0x1000, 0x1000, "Code_Segment"); // Segmento de código
}

void MainMemory::add_segment(uint64_t base, uint64_t size, const std::string& name) {
    MemorySegment seg{base, size, name};
    segments_.push_back(seg);
    std::cout << "[Memory] Added segment: " << name 
              << " base=0x" << std::hex << base 
              << " size=0x" << size << std::dec << std::endl;
}

bool MainMemory::is_address_valid(uint64_t addr) const {
    // Verificar límites de memoria física
    if (addr >= MEM_WORDS * WORD_SIZE) {
        return false;
    }
    
    // Verificar segmentos (opcional - para restricciones específicas)
    for (const auto& seg : segments_) {
        if (seg.is_valid(addr)) {
            return true;
        }
    }
    
    // Por defecto, toda la memoria física es accesible
    return true;
}

const MemorySegment* MainMemory::get_segment(uint64_t addr) const {
    for (const auto& seg : segments_) {
        if (seg.is_valid(addr)) {
            return &seg;
        }
    }
    return nullptr;
}

void MainMemory::dump_segments() const {
    std::cout << "=== Memory Segments ===" << std::endl;
    for (const auto& seg : segments_) {
        std::cout << "  " << seg.name << ": 0x" << std::hex << seg.base_addr 
                  << " - 0x" << (seg.base_addr + seg.size - 1) 
                  << " (" << std::dec << seg.size << " bytes)" << std::endl;
    }
}

std::array<uint64_t, WORDS_PER_LINE> MainMemory::read_line(uint64_t base) {
    if (!is_address_valid(base)) {
        std::cerr << "[Memory] Error: Invalid address 0x" << std::hex << base << " for read" << std::endl;
        return std::array<uint64_t, WORDS_PER_LINE>{};
    }
    
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
    if (!is_address_valid(base)) {
        std::cerr << "[Memory] Error: Invalid address 0x" << std::hex << base << " for write" << std::endl;
        return;
    }
    
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
    if (!is_address_valid(addr)) {
        std::cerr << "[Memory] Error: Invalid address 0x" << std::hex << addr << " for word read" << std::endl;
        return 0;
    }
    
    read_count_++;
    size_t index = (addr / WORD_SIZE) % MEM_WORDS;
    return mem_[index];
}

void MainMemory::write_word(uint64_t addr, uint64_t data) {
    if (!is_address_valid(addr)) {
        std::cerr << "[Memory] Error: Invalid address 0x" << std::hex << addr << " for word write" << std::endl;
        return;
    }
    
    write_count_++;
    size_t index = (addr / WORD_SIZE) % MEM_WORDS;
    mem_[index] = data;
}

void MainMemory::dump_stats() const {
    std::cout << "=== Memory Statistics ===" << std::endl;
    std::cout << "Total reads: " << read_count_ << std::endl;
    std::cout << "Total writes: " << write_count_ << std::endl;
    std::cout << "Total accesses: " << (read_count_ + write_count_) << std::endl;
    std::cout << "Physical memory: " << MEM_WORDS << " words (" << (MEM_WORDS * WORD_SIZE) << " bytes)" << std::endl;
}

void MainMemory::dump_memory_range(uint64_t start_addr, uint64_t end_addr) const {
    std::cout << "=== Memory Dump (0x" << std::hex << start_addr 
              << " - 0x" << end_addr << ") ===" << std::dec << std::endl;
    
    for (uint64_t addr = start_addr; addr <= end_addr; addr += WORD_SIZE) {
        size_t index = (addr / WORD_SIZE) % MEM_WORDS;
        const auto* seg = get_segment(addr);
        std::string seg_name = seg ? seg->name : "Unknown";
        std::cout << "0x" << std::hex << addr << " [" << seg_name << "]: " 
                  << std::dec << mem_[index] << std::endl;
    }
}