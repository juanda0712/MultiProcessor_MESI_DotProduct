#include "instruction.hpp"

std::string opcodeToString(Opcode opcode) {
    switch (opcode) {
        case Opcode::LOAD:  return "LOAD";
        case Opcode::STORE: return "STORE";
        case Opcode::FMUL:  return "FMUL";
        case Opcode::FADD:  return "FADD";
        case Opcode::INC:   return "INC";
        case Opcode::DEC:   return "DEC";
        case Opcode::JNZ:   return "JNZ";
        case Opcode::LABEL: return "LABEL";
        default:            return "INVALID";
    }
}

std::string Instruction::opcodeName() const {
    return opcodeToString(opcode);
}
