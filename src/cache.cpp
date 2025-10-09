#include "cache.hpp"
#include <vector>      
#include <string>

// double Cache::read(size_t addr) {
//     std::lock_guard<std::mutex> lock(mtx_);
//     if (data_.find(addr) == data_.end()) {
//         std::cout << "[Cache] Miss at address " << addr << "\n";
//         data_[addr] = 0.0;
//     }
//     return data_[addr];
// }

//por el momento hice esto otro read para simular
double Cache::read(uint64_t address) {
    // Simulación temporal: devolvemos datos según rango de direcciones
    if (address >= 0x0000 && address < 0x0040) {
        // Segmento de A
        return 1.0 + (address / 8.0); // 1.0, 2.0, 3.0, ...
    }
    else if (address >= 0x0040 && address < 0x0080) {
        // Segmento de B
        return 5.0 + (address - 0x0040) / 8.0; // 5.0, 6.0, ...
    }
    else if (address == 0x0080) {
        // Partial sum inicial
        return 0.0;
    }
    return 0.0; // default
}


void Cache::write(size_t addr, double value) {
    std::lock_guard<std::mutex> lock(mtx_);
    data_[addr] = value;
    std::cout << "[Cache] Write " << value << " at address " << addr << "\n";
}

void Cache::loadProgram(const std::vector<std::string>& program) {
    std::lock_guard<std::mutex> lock(mtx_);
    size_t baseAddr = 0;
    for (const auto& line : program) {
        data_[baseAddr++] = 0.0; // simular instrucción cargada
    }
}
