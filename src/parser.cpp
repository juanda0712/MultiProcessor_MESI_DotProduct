#include "parser.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>

static Opcode toOpcode(const std::string& op) {
    if (op == "LOAD") return Opcode::LOAD;
    if (op == "STORE") return Opcode::STORE;
    if (op == "FMUL") return Opcode::FMUL;
    if (op == "FADD") return Opcode::FADD;
    if (op == "INC")  return Opcode::INC;
    if (op == "DEC")  return Opcode::DEC;
    if (op == "JNZ")  return Opcode::JNZ;
    return Opcode::INVALID;
}

std::vector<Instruction> Parser::parseFile(const std::string& filename) {
    std::ifstream file(filename);
    std::vector<Instruction> instructions;
    std::string line;

    std::regex labelRegex(R"(^\s*([A-Za-z_][A-Za-z0-9_]*):\s*$)");
    std::regex instrRegex(R"(^\s*([A-Z]+)\s+([^,\s]+)(?:,\s*([^,\s]+))?(?:,\s*([^,\s]+))?)");

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        std::smatch match;

        if (std::regex_match(line, match, labelRegex)) {
            instructions.push_back({Opcode::LABEL, "", "", "", match[1]});
            continue;
        }

        if (std::regex_match(line, match, instrRegex)) {
            Opcode op = toOpcode(match[1]);
            std::string d  = match.size() > 2 ? std::string(match[2]) : "";
            std::string s1 = match.size() > 3 ? std::string(match[3]) : "";
            std::string s2 = match.size() > 4 ? std::string(match[4]) : "";
            instructions.push_back({op, d, s1, s2, ""});
        }
    }
    return instructions;
}
