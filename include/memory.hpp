#pragma once
#include <array>
#include <vector>
#include <mutex>
#include <cstddef>
#include <cstdint>
#include <string>
#include <chrono>
#include "core.hpp"

class MainMemory {
public:
    // Crea memoria con tamaño en bytes (debe ser múltiplo de WORD_SIZE).
    // Por defecto, 1 MiB.
    explicit MainMemory(size_t sizeBytes = (1u << 20));

    // Lectura/Escritura por línea (BASE debe estar alineada a LINE_SIZE)
    std::array<uint64_t, WORDS_PER_LINE> read_line(uint64_t base);
    void write_line(uint64_t base, const std::array<uint64_t, WORDS_PER_LINE>& ln);

    // Lectura/Escritura por palabra (addr en bytes)
    uint64_t read_word(uint64_t addr);
    void write_word(uint64_t addr, uint64_t value);

    // Inicialización/contenido
    void clear();                                 // pone todo a 0
    void fill(uint64_t value);                     // pone todas las palabras a 'value'
    void fill_linear(uint64_t mul, uint64_t add);  // mem[i] = i*mul + add
    bool load_from_text(const std::string& path);  // archivo texto: "index value" o "value" secuencial; admite 0x..

    // Configuración de tamaño/expansión
    size_t size_bytes() const;
    size_t size_words() const;
    void set_auto_expand(bool enable);             // si true, resize dinámico ante OOB
    bool auto_expand() const;

    // Latencia simulada (nanosegundos)
    void set_latency_ns(uint64_t ns);
    void enable_latency(bool en);

    // Endianness y alineación
    enum class Endianness { Little, Big };
    void set_endianness(Endianness e) { endianness_ = e; }
    Endianness endianness() const { return endianness_ ; }
    void enable_align_check(bool en) { align_check_ = en; }
    size_t misalign_count() const { return misalign_count_; }

    // IO por bytes y tipos
    bool read_bytes(uint64_t addr, void* dst, size_t len);
    bool write_bytes(uint64_t addr, const void* src, size_t len);
    uint8_t  read_u8(uint64_t addr);
    uint16_t read_u16(uint64_t addr);
    uint32_t read_u32(uint64_t addr);
    uint64_t read_u64(uint64_t addr) { return read_word(addr); }
    void write_u8(uint64_t addr, uint8_t v);
    void write_u16(uint64_t addr, uint16_t v);
    void write_u32(uint64_t addr, uint32_t v);
    void write_u64(uint64_t addr, uint64_t v) { write_word(addr, v); }

    // Carga/volcado binario y dumps
    bool load_from_binary(const std::string& path, uint64_t baseAddr = 0);
    bool dump_to_binary(const std::string& path, uint64_t baseAddr, size_t lenBytes) const;
    bool dump_to_text(const std::string& path, uint64_t baseAddr, size_t lenBytes, size_t bytesPerLine = 16) const;

    // Estadísticas
    struct Stats {
        uint64_t line_reads{0};
        uint64_t line_writes{0};
        uint64_t word_reads{0};
        uint64_t word_writes{0};
        uint64_t bytes_read{0};
        uint64_t bytes_written{0};
    };
    Stats stats() const { return stats_; }
    void reset_stats();

private:
    // Helpers
    size_t index_from_addr_(uint64_t addr) const;              // convierte addr (bytes) -> índice de palabra
    void maybe_sleep_() const;                                  // aplica latencia si está habilitada
    void ensure_capacity_(size_t wordIndex, size_t wordsNeeded); // amplía si auto_expand_

    mutable std::mutex mtx_;
    std::vector<uint64_t> mem_;
    bool auto_expand_{false};
    bool latency_enabled_{false};
    uint64_t latency_ns_{0};
    Endianness endianness_{Endianness::Little};
    bool align_check_{false};
    mutable size_t misalign_count_{0};
    mutable Stats stats_{};
};
