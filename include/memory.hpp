#pragma once
#include <array>
#include <cstddef>
#include <vector>
#include <string>
#include "core.hpp"

static constexpr size_t MEM_WORDS = 512; // 512 palabras de 64 bits = 4096 bytes

// =====================================================
// Estructura para segmentos de memoria
// =====================================================
struct MemorySegment {
    uint64_t base_addr;
    uint64_t size;
    std::string name;
    bool is_valid(uint64_t addr) const {
        return addr >= base_addr && addr < (base_addr + size);
    }
};

// =====================================================
// Clase MainMemory: memoria principal simulada
// =====================================================
class MainMemory {
public:
    MainMemory();

    std::array<uint64_t, WORDS_PER_LINE> read_line(uint64_t base);
    void write_line(uint64_t base, const std::array<uint64_t, WORDS_PER_LINE>& ln);
    
    // Acceso a palabra individual
    uint64_t read_word(uint64_t addr);
    void write_word(uint64_t addr, uint64_t data);

    // Inicializa los vectores A, B y la suma parcial en memoria
    void initialize_dot_product_data(size_t baseA, size_t baseB, size_t partialAddr, size_t count);

    // Métodos para segmentación
    void add_segment(uint64_t base, uint64_t size, const std::string& name);
    bool is_address_valid(uint64_t addr) const;
    const MemorySegment* get_segment(uint64_t addr) const;
    void dump_segments() const;

    // Métodos para estadísticas y depuración
    void dump_stats() const;
    void dump_memory_range(uint64_t start_addr, uint64_t end_addr) const;

private:
    std::array<uint64_t, MEM_WORDS> mem_;
    std::vector<MemorySegment> segments_;
    size_t read_count_;
    size_t write_count_;
};