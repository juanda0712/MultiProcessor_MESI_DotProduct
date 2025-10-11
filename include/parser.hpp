#pragma once
#include "instruction.hpp"
#include <vector>
#include <string>

class Parser {
public:
    std::vector<Instruction> parseFile(const std::string& filename);
};
