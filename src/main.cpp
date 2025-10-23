#include "processing_element.hpp"
#include "loader.hpp"
#include "task_distributor.hpp"
#include <iostream>
#include "cache.hpp"
#include "interconnect.hpp"

int main(int argc, char** argv) {
    const int NUM_PES = 4;
    // Default program path is relative to repo root when launched via GUI
    std::string programFile = (argc > 1) ? argv[1] : "programs/program_pe0.txt";

    std::vector<Cache*> caches;
    std::vector<ProcessingElement*> pes;

    MainMemory mem;
    Interconnect ic(&mem);

    // Inicializa datos (por defecto A=1..16, B=10..25) en direcciones consistentes con distribución
    mem.initialize_dot_product_data(0x0000, 0x0080, 0x0200, 16);

    // Crear caches y PEs
    for (int i = 0; i < NUM_PES; ++i) {
        auto* cache = new Cache(i, &ic, &mem);
        ic.attach(cache);
        caches.push_back(cache);
        pes.push_back(new ProcessingElement(i, cache));
    }

    // Cargar programa
    Loader loader;
    loader.loadAndDistribute({programFile}, pes);

    // Distribuir trabajo: elegir partialBase fuera de A/B
    TaskDistributor distributor;
    uint64_t baseA = 0x0000;
    uint64_t baseB = 0x0080; // Debe coincidir con initialize_dot_product_data
    uint64_t partialBase = 0x0200; // <-- importante: debe estar fuera de A/B
    distributor.distributeWork(pes, baseA, baseB, partialBase, 16);

    // debug dumps
    mem.dump_memory_range(baseA, baseA + 16 * WORD_SIZE - 1);
    mem.dump_memory_range(baseB, baseB + 16 * WORD_SIZE - 1);
    mem.dump_memory_range(partialBase, partialBase + NUM_PES * WORD_SIZE - 1);

    for (auto &pe : pes) pe->dump_registers();

    // Ejecutar
    for (auto& pe : pes) pe->start();
    for (auto& pe : pes) pe->join();

    // recolectar stats...
    for (int i = 0; i < NUM_PES; ++i) {
        ic.record_cache_stats(i, caches[i]->get_hits(), caches[i]->get_misses());
    }

    mem.dump_segments();
    mem.dump_stats();
    ic.dump_stats();
    ic.dump_pe_stats();

    // flush caches so memory reflects the latest writes
    for (auto c : caches) {
        c->flush();
    }

    // leer parciales
    double final_sum = 0;
    for (int i = 0; i < NUM_PES; ++i) {
        uint64_t addr = partialBase + i * WORD_SIZE;
        uint64_t val = mem.read_word(addr);
        final_sum += val;
        std::cout << "PE" << i << " Partial Sum @ 0x" << std::hex << addr 
                  << ": " << std::dec << val << std::endl;
    }
    std::cout << "FINAL DOT PRODUCT: " << final_sum << std::endl;

    for (auto c : caches) delete c;
    for (auto p : pes) delete p;
    return 0;
}
