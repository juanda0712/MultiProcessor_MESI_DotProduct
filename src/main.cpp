#include "processing_element.hpp"
#include "loader.hpp"
#include <iostream>
#include "cache.hpp"
#include "interconnect.hpp"

int main() {
    const int NUM_PES = 4;

    std::vector<Cache*> caches;
    std::vector<ProcessingElement*> pes;

    // Archivos de programa: uno por PE
    std::vector<std::string> programFiles = {
        "programs/program_pe0.txt",
        "programs/program_pe0.txt",
        "programs/program_pe0.txt",
        "programs/program_pe0.txt"
    };

    // Crear memoria e interconnect
    MainMemory mem;
    Interconnect ic(&mem);

    /* // Prueba de tamaño de memoria
    std::cout << "=== Memory Size Test ===" << std::endl;
    std::cout << "Expected: 512 words (4096 bytes)" << std::endl;

    // Probar acceso a palabras individuales
    mem.write_word(0x0000, 0x123456789ABCDEF0);
    uint64_t val = mem.read_word(0x0000);
    std::cout << "Write/Read test: 0x" << std::hex << val << std::dec << std::endl;

    // Probar acceso desalineado (debería mostrar warning)
    mem.read_line(0x0008); // Desalineado - no múltiplo de 32 */

    // Inicializar datos de prueba para cada PE
    mem.initialize_dot_product_data(0x0000, 0x0040, 0x0080, 4); // PE0
    mem.initialize_dot_product_data(0x0100, 0x0140, 0x0180, 4); // PE1
    mem.initialize_dot_product_data(0x0200, 0x0240, 0x0280, 4); // PE2
    mem.initialize_dot_product_data(0x0300, 0x0340, 0x0380, 4); // PE3
    
    // Crear cachés y PEs
    for (int i = 0; i < NUM_PES; ++i) {
        auto* cache = new Cache(i, &ic, &mem);
        ic.attach(cache);
        caches.push_back(cache);
        pes.push_back(new ProcessingElement(i, cache));
    }

    // Cargar programas a cada PE
    Loader loader;
    loader.loadAndDistribute(programFiles, pes);

    // Inicializar registros de prueba para cada PE
    pes[0]->initializeRegisters(0x0000, 0x0040, 0x0080, 4);
    pes[1]->initializeRegisters(0x0100, 0x0140, 0x0180, 4);
    pes[2]->initializeRegisters(0x0200, 0x0240, 0x0280, 4);
    pes[3]->initializeRegisters(0x0300, 0x0340, 0x0380, 4);

    // Iniciar ejecución en paralelo
    for (auto& pe : pes) pe->start();
    for (auto& pe : pes) pe->join();

    // Recolectar estadísticas de caché
    for (int i = 0; i < NUM_PES; ++i) {
        ic.record_cache_stats(i, caches[i]->get_hits(), caches[i]->get_misses());
    }

    // Mostrar todas las estadísticas
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "SYSTEM STATISTICS SUMMARY" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    // Segmentación de memoria
    mem.dump_segments();

    // Estadísticas de memoria
    mem.dump_stats();

    // Estadísticas del bus y por PE
    ic.dump_stats();
    ic.dump_pe_stats();

    // Estado final de las cachés
    std::cout << "\n=== Final Cache States ===" << std::endl;
    for (auto cache : caches) {
        cache->dump_state();
    }

    // Resultados del producto punto
    std::cout << "\n=== DOT PRODUCT FINAL RESULTS ===" << std::endl;
    std::cout << "PE0 Partial Sum @ 0x0080: " << mem.read_word(0x0080) << std::endl;
    std::cout << "PE1 Partial Sum @ 0x0180: " << mem.read_word(0x0180) << std::endl; 
    std::cout << "PE2 Partial Sum @ 0x0280: " << mem.read_word(0x0280) << std::endl;
    std::cout << "PE3 Partial Sum @ 0x0380: " << mem.read_word(0x0380) << std::endl;

    // Calcular suma total (producto punto final)
    double final_sum = mem.read_word(0x0080) + mem.read_word(0x0180) + 
                    mem.read_word(0x0280) + mem.read_word(0x0380);
    std::cout << "FINAL DOT PRODUCT: " << final_sum << std::endl;

    // Validación (debería ser: (1×10 + 2×11 + 3×12 + 4×13) × 4 PEs = ...)
    std::cout << "EXPECTED: Each PE computes (1×10 + 2×11 + 3×12 + 4×13) = " 
            << (1*10 + 2*11 + 3*12 + 4*13) << std::endl;
    std::cout << "Total expected: " << (1*10 + 2*11 + 3*12 + 4*13) * 4 << std::endl;


    // Imprimir la salida de cada PE de forma ordenada
    for (auto& pe : pes) {
        std::cout << pe->getOutput();
    }

    // Liberar memoria
    for (auto c : caches) delete c;
    for (auto p : pes) delete p;

    std::cout << "All PEs finished execution.\n";

    /* // Prueba de estadísticas finales
    std::cout << "\n=== Final Statistics ===" << std::endl;
    mem.dump_stats();
    ic.dump_stats();

    // Mostrar resultados del producto punto
    std::cout << "\n=== Dot Product Results ===" << std::endl;
    mem.dump_memory_range(0x0080, 0x0080); // PE0 partial sum
    mem.dump_memory_range(0x0180, 0x0180); // PE1 partial sum  
    mem.dump_memory_range(0x0280, 0x0280); // PE2 partial sum
    mem.dump_memory_range(0x0380, 0x0380); // PE3 partial sum */

    return 0;
    
}
