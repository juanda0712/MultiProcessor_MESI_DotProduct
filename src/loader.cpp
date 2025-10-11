#include "loader.hpp"
#include <iostream>

void Loader::loadAndDistribute(const std::vector<std::string>& programFiles,
                               std::vector<ProcessingElement*>& pes) {
    Parser parser;
    size_t numFiles = programFiles.size();
    size_t numPEs = pes.size();

    for (size_t i = 0; i < numPEs; ++i) {
        // Si hay menos archivos que PEs, se reutiliza el último archivo
        std::string file = (i < numFiles) ? programFiles[i] : programFiles.back();

        std::vector<Instruction> prog = parser.parseFile(file);
        pes[i]->loadProgram(prog);

        std::cout << "[Loader] Program loaded into PE" << i
                  << " from " << file << "\n";
    }
}
