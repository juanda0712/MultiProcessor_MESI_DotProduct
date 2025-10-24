#pragma once
#include <string>
#include <vector>
#include <optional>

enum class Opcode {
    LOAD, STORE, FMUL, FADD, INC, DEC, JNZ, LABEL, INVALID
};

struct Instruction {
    Opcode opcode;
    std::string dest;      // Para la mayoría de instrucciones
    std::string src1;
    std::string src2;
    std::string label;     // Para JNZ
    std::string condition_reg; // NUEVO: registro para condición en JNZ
    std::string opcodeName() const;
};
