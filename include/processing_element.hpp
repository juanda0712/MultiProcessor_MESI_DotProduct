#pragma once
#include "instruction.hpp"
#include "cache.hpp"
#include <vector>
#include <unordered_map>
#include <thread>
#include <string>
#include <iostream>

/* ---------- Register File ---------- */
class RegisterFile {
public:
    RegisterFile();
    uint64_t& get(const std::string& name);
    void dump() const;
    std::string dump_str() const;
private:
    std::unordered_map<std::string, uint64_t> regs_;
};


/* ---------- ALU ---------- */
class ALU {
public:
    double execute(const std::string& op, double a, double b);
};

/* ---------- Control Unit ---------- */
class ControlUnit {
private:
    RegisterFile* rf_;
    ALU* alu_;
    Cache* cache_;

public:
    ControlUnit(RegisterFile* rf, ALU* alu, Cache* cache);
    size_t executeInstruction(const Instruction& instr, size_t current_pc);
};

/* ---------- Processing Element ---------- */
class ProcessingElement {
public:
    ProcessingElement(int id, Cache* cache);
    void loadProgram(const std::vector<Instruction>& program);
    void start();
    void join();
    void initializeRegisters(double baseA, double baseB, double partialAddr, double count);
    std::string getOutput() const;
    void dump_registers() const;

private:
    void run();

    int id_;
    size_t pc_ = 0;
    std::thread thread_;
    std::vector<Instruction> program_;

    RegisterFile regFile_;
    ALU alu_;
    ControlUnit control_;
    Cache* cache_;

    std::string output_;
};
