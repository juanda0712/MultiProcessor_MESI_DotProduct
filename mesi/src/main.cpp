// Inicializa la memoria principal y el interconnect, crea 3 caches (C0–C2), 
// las adjunta al bus y define tres direcciones dentro de dos líneas: A=0x0000, 
// C=A+8 (misma línea que A) y B=0x0020 (otra línea). Luego ejecuta una secuencia de 
// 9 operaciones que fuerzan las transiciones típicas de MESI y finalmente imprime el 
// estado de cada caché.
#include "fsm.cpp"
#include "core.cpp"
#include "memory.cpp"

#include <array>
#include <optional>
#include <vector>
#include <cstdint>
#include <array>
#include <iostream>
#include <memory>

// include implementations
#include "cache.cpp"

static constexpr size_t NUM_PES = 3;

int main(){
    MainMemory mem;
    Interconnect ic(&mem);
    std::vector<Cache*> cs;
    std::vector<std::unique_ptr<Cache>> holder;
    for (int i=0;i<(int)NUM_PES;i++){
        holder.emplace_back(new Cache(i, &ic, &mem));
        ic.attach(holder.back().get());
        cs.push_back(holder.back().get());
    }

    uint64_t A = 0x0000; uint64_t B = 0x0020; uint64_t C = 0x0008;

    std::cout << "=== PRUEBAS MESI (C++) ===\n";
    auto r0 = cs[0]->cpu_load(A); std::cout << "[T1] C0 LOAD A -> " << r0 << "\n";
    auto r1 = cs[1]->cpu_load(A); std::cout << "[T2] C1 LOAD A -> " << r1 << "\n";
    cs[1]->cpu_store(A, 0xAAAA'AAAA'AAAA'AAAAULL); std::cout << "[T3] C1 STORE A = AAAA...\n";
    auto r2 = cs[2]->cpu_load(A); std::cout << "[T4] C2 LOAD A -> " << std::hex << r2 << std::dec << " (esperado AAAA...)\n";
    cs[0]->cpu_store(C, 777); std::cout << "[T5] C0 STORE C(=A+8) = 777\n";
    auto r3 = cs[2]->cpu_load(C); std::cout << "[T6] C2 LOAD C -> " << r3 << " (esperado 777)\n";
    auto r4 = cs[0]->cpu_load(B); std::cout << "[T7] C0 LOAD B -> " << r4 << "\n";
    cs[2]->cpu_store(B, 42); std::cout << "[T8] C2 STORE B = 42\n";
    auto r5 = cs[1]->cpu_load(B); std::cout << "[T9] C1 LOAD B -> " << r5 << " (esperado 42)\n";

    for (auto* c : cs) c->dump_state();
    std::cout << "=== FIN ===\n";
    return 0;
}
