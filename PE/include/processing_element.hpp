#pragma once
#include "instruction.hpp"
#include "cache.hpp"
#include <vector>
#include <unordered_map>
#include <thread>
#include <string>
#include <iostream>

class RegisterFile {
public:
    RegisterFile();
    double& get(const std::string& name);
    void dump();

private:
    std::unordered_map<std::string, double> regs_;
};

class ALU {
public:
    double execute(const std::string& op, double a, double b);
};

class ControlUnit {
public:
    ControlUnit(RegisterFile* rf, ALU* alu, Cache* cache);
    void executeInstruction(const Instruction& instr, size_t& pc);

private:
    RegisterFile* rf_;
    ALU* alu_;
    Cache* cache_;
};

class ProcessingElement {
public:
    ProcessingElement(int id, Cache* cache);
    void loadProgram(const std::vector<Instruction>& program);
    void start();
    void join();
    void initializeRegisters(double baseA, double baseB, double partialAddr, double count);

private:
    int id_;
    size_t pc_ = 0;
    std::thread thread_;
    std::vector<Instruction> program_;

    RegisterFile regFile_;
    ALU alu_;
    ControlUnit control_;
    Cache* cache_;

    void run();
};
