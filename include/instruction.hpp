#pragma once
#include <string>
#include <vector>
#include <optional>

enum class Opcode {
    LOAD, STORE, FMUL, FADD, INC, DEC, JNZ, LABEL, INVALID
};

struct Instruction {
    Opcode opcode;
    std::string dest;
    std::string src1;
    std::string src2;
    std::string label;  // Para almacenar etiquetas en saltos
};
