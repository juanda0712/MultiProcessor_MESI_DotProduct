// Este archivo implementa una memoria principal simulada basada en un array
// de palabras. Provee funciones para leer y escribir una línea completa
#ifndef MESI_MEMORY_CPP
#define MESI_MEMORY_CPP

#include <array>

// Número de palabras del banco de memoria simulado. 
static constexpr size_t MEM_WORDS = 1024;

// Estructura que simula la memoria principal. Contiene un array de palabras
// y métodos para leer/escribir líneas completas.
struct MainMemory {
    std::array<uint64_t, MEM_WORDS> mem{};

    // Constructor: inicializa la memoria con valores 
    MainMemory(){ for (size_t i=0;i<MEM_WORDS;i++) mem[i] = i*1111ULL; }

    // Lee una línea completa a partir de la dirección base. 
    std::array<uint64_t,WORDS_PER_LINE> read_line(uint64_t base){ std::array<uint64_t,WORDS_PER_LINE> out{}; size_t w0 = (base/WORD_SIZE) % MEM_WORDS; for (size_t i=0;i<WORDS_PER_LINE;i++) out[i] = mem[(w0+i)%MEM_WORDS]; return out; }

    // Escribe una línea completa en memoria a partir de la dirección base.
    void write_line(uint64_t base, const std::array<uint64_t,WORDS_PER_LINE>& ln){ size_t w0 = (base/WORD_SIZE) % MEM_WORDS; for (size_t i=0;i<WORDS_PER_LINE;i++) mem[(w0+i)%MEM_WORDS] = ln[i]; }
};

#endif // MESI_MEMORY_CPP
