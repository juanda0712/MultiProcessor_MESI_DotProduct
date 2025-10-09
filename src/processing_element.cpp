#include "processing_element.hpp"
#include <chrono>
#include <cmath>

/* ---------- Register File ---------- */
RegisterFile::RegisterFile() {
    for (int i = 0; i < 8; ++i)
        regs_["REG" + std::to_string(i)] = 0.0;
}

double& RegisterFile::get(const std::string& name) {
    return regs_[name];
}

void RegisterFile::dump() {
    std::cout << "Registers:\n";
    for (auto& [k, v] : regs_) std::cout << "  " << k << " = " << v << "\n";
}

/* ---------- ALU ---------- */
double ALU::execute(const std::string& op, double a, double b) {
    if (op == "FMUL") return a * b;
    if (op == "FADD") return a + b;
    return 0.0;
}

/* ---------- Control Unit ---------- */
ControlUnit::ControlUnit(RegisterFile* rf, ALU* alu, Cache* cache)
    : rf_(rf), alu_(alu), cache_(cache) {}

void ControlUnit::executeInstruction(const Instruction& instr, size_t& pc) {
    if (instr.opcode == Opcode::LOAD) {
        rf_->get(instr.dest) = cache_->read((size_t)rf_->get(instr.src1));
    }
    else if (instr.opcode == Opcode::STORE) {
        cache_->write((size_t)rf_->get(instr.src1), rf_->get(instr.dest));
    }
    else if (instr.opcode == Opcode::FMUL || instr.opcode == Opcode::FADD) {
        rf_->get(instr.dest) = alu_->execute(
            instr.opcode == Opcode::FMUL ? "FMUL" : "FADD",
            rf_->get(instr.src1), rf_->get(instr.src2));
    }
    else if (instr.opcode == Opcode::INC) {
        rf_->get(instr.dest)++;
    }
    else if (instr.opcode == Opcode::DEC) {
        rf_->get(instr.dest)--;
    }
    else if (instr.opcode == Opcode::JNZ) {
        if (rf_->get(instr.dest) != 0) pc -= 2; // ejemplo simple
    }
}

/* ---------- Processing Element ---------- */
ProcessingElement::ProcessingElement(int id, Cache* cache)
    : id_(id), cache_(cache), control_(&regFile_, &alu_, cache) {}

void ProcessingElement::loadProgram(const std::vector<Instruction>& program) {
    program_ = program;
}

void ProcessingElement::initializeRegisters(double baseA, double baseB, double partialAddr, double count) {
    regFile_.get("REG0") = baseA;
    regFile_.get("REG1") = baseB;
    regFile_.get("REG2") = partialAddr;
    regFile_.get("REG3") = count;
}

void ProcessingElement::start() {
    thread_ = std::thread(&ProcessingElement::run, this);
}

void ProcessingElement::join() {
    if (thread_.joinable()) thread_.join();
}

void ProcessingElement::run() {
    std::cout << "[PE" << id_ << "] Execution start.\n";
    while (pc_ < program_.size()) {
        control_.executeInstruction(program_[pc_], pc_);
        pc_++;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::cout << "[PE" << id_ << "] Execution end.\n";
    regFile_.dump();
}
