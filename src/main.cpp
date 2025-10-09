#include "processing_element.hpp"
#include "cache.hpp"
#include "loader.hpp"
#include <iostream>

int main() {
    const int NUM_PES = 4;

    std::vector<Cache*> caches;
    std::vector<ProcessingElement*> pes;

    // Solo tenemos un archivo de programa, así que se usará el mismo para todos los PEs
    std::vector<std::string> programFiles = {"programs/program_pe0.txt"};

    // Crear cachés y PEs
    for (int i = 0; i < NUM_PES; ++i) {
        caches.push_back(new Cache());
        pes.push_back(new ProcessingElement(i, caches[i]));
    }

    // Cargar programa a cada PE
    Loader loader;
    loader.loadAndDistribute(programFiles, pes);

    // Inicializar registros de prueba (cada PE puede tener valores diferentes si se quiere)
    pes[0]->initializeRegisters(0x0000, 0x0040, 0x0080, 4); // PE0 procesa segmento A0,B0
    pes[1]->initializeRegisters(0x0100, 0x0140, 0x0180, 4); // PE1 procesa A1,B1
    pes[2]->initializeRegisters(0x0200, 0x0240, 0x0280, 4); // PE2 procesa A2,B2
    pes[3]->initializeRegisters(0x0300, 0x0340, 0x0380, 4); // PE3 procesa A3,B3

    // Iniciar ejecución en paralelo
    for (auto& pe : pes) pe->start();
    for (auto& pe : pes) pe->join();

    // Liberar memoria
    for (auto c : caches) delete c;
    for (auto p : pes) delete p;

    std::cout << "All PEs finished execution.\n";
    return 0;
}
