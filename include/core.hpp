#pragma once
#include <array>
#include <cstdint>

// =======================
// Parámetros del sistema
// =======================
static constexpr size_t LINE_SIZE       = 32;  // bytes por línea
static constexpr size_t WORD_SIZE       = 8;   // bytes por palabra
static constexpr size_t WORDS_PER_LINE  = LINE_SIZE / WORD_SIZE;
static constexpr size_t MAX_LINES       = 256; // cantidad de líneas cacheables

// =======================
// Funciones utilitarias
// =======================
inline uint64_t line_base(uint64_t addr) {
    return addr & ~uint64_t(LINE_SIZE - 1);
}

inline size_t line_index(uint64_t base) {
    return (base / LINE_SIZE) % MAX_LINES;
}

// =======================
// Tipos de comandos del bus
// =======================
enum class BusCmd : uint8_t {
    None = 0,
    BusRd,
    BusRdX,
    Upgrade,
    WriteBack
};

// =======================
// Estructuras de mensajes
// =======================
struct BusRequest {
    BusCmd cmd{BusCmd::None};
    uint64_t addr{0};
    int src_id{-1};
    bool has_wline{false};
    std::array<uint64_t, WORDS_PER_LINE> wline{};
};

struct BusResponse {
    bool grant{false};
    bool shared{false};
    uint8_t inv_ack_target{0};
    std::array<uint64_t, WORDS_PER_LINE> rline{};
};

struct SnoopMessage {
    BusCmd cmd{BusCmd::None};
    uint64_t addr{0};
    int src_id{-1};
};

struct SnoopRet {
    bool hit{false};
    bool hitM{false};
    bool invalidated{false};
    std::array<uint64_t, WORDS_PER_LINE> wb{};
};
