#include "loader.hpp"
#include <iostream>

void Loader::loadAndDistribute(const std::vector<std::string>& programFiles,
                               std::vector<ProcessingElement*>& pes) {
    Parser parser;
    for (size_t i = 0; i < programFiles.size(); ++i) {
        std::vector<Instruction> prog = parser.parseFile(programFiles[i]);
        pes[i]->loadProgram(prog);
        std::cout << "[Loader] Program loaded into PE" << i << " from " << programFiles[i] << "\n";
    }
}
