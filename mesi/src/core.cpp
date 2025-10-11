// Define constantes del sistema (tamaño de línea, palabras por
// línea), utilidades para calcular la base de línea y el índice, y los tipos
// que representan los mensajes de bus y las respuestas de snooping que se usan
// en el interconnect y en las caches.
#ifndef MESI_CORE_CPP
#define MESI_CORE_CPP

#include <array>
#include <cstdint>

// Tamaño de línea (en bytes) y tamaño de palabra (en bytes). 
static constexpr size_t LINE_SIZE = 32;
static constexpr size_t WORD_SIZE = 8;
static constexpr size_t WORDS_PER_LINE = LINE_SIZE / WORD_SIZE;

// Calcular la dirección base de la línea que contiene una dirección dada.
inline uint64_t line_base(uint64_t addr) { return addr & ~uint64_t(LINE_SIZE - 1); }

// Calcular un índice derivado de la base de línea.
inline size_t   line_index(uint64_t base){ return (base / LINE_SIZE) % 256; }

// Comandos básicos que puede solicitar un núcleo al bus.
enum class BusCmd : uint8_t { None=0, BusRd, BusRdX, Upgrade, WriteBack };

// Estructura que representa una petición al bus. 
struct BusRequest {
    BusCmd   cmd{BusCmd::None};
    uint64_t addr{0};
    int      src_id{-1};
    bool     has_wline{false};
    std::array<uint64_t, WORDS_PER_LINE> wline{};
};

// Mensaje de snoop que el interconnect envía a las caches para que inspeccionen
// el estado de una línea y reaccionen.
struct SnoopMessage {
    BusCmd   cmd{BusCmd::None};
    uint64_t addr{0};
    int      src_id{-1};
};

// Respuesta del interconnect hacia el emisor de la petición. 
struct BusResponse {
    bool     grant{false};
    bool     shared{false};
    uint8_t  inv_ack_target{0};
    std::array<uint64_t,WORDS_PER_LINE> rline{};
};

// Respuesta al snoop por parte de una caché: indica
// si hubo acierto, si el estado era M (requiere writeback) y si se invalidó la entrada.
struct SnoopRet { bool hit=false; bool hitM=false; bool invalidated=false; std::array<uint64_t,WORDS_PER_LINE> wb{}; };

#endif 
