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

    // Imprimir la salida de cada PE de forma ordenada
    for (auto& pe : pes) {
        std::cout << pe->getOutput();
    }

    // Liberar memoria
    for (auto c : caches) delete c;
    for (auto p : pes) delete p;

    std::cout << "All PEs finished execution.\n";
    return 0;
}
