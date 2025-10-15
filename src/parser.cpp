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

    if (!file.is_open()) {
        std::cerr << "[Parser] Error: Cannot open file " << filename << std::endl;
        return instructions;
    }

    // Regex mejorado que maneja diferentes patrones de instrucciones
    // Patrón 1: INSTR REG, [REG]
    // Patrón 2: INSTR REG, REG, REG  
    // Patrón 3: INSTR REG
    // Patrón 4: INSTR LABEL
    std::regex instrRegex(R"(^\s*([A-Z]+)\s+([^,\s]+)(?:\s*,\s*(\[?\s*[^,\]\s]+\s*\]?))?(?:\s*,\s*(\[?\s*[^,\]\s]+\s*\]?))?\s*$)");

    int lineNum = 0;
    while (std::getline(file, line)) {
        lineNum++;
        
        // Elimina comentarios
        size_t comment_pos = line.find_first_of(";#");
        if (comment_pos != std::string::npos) 
            line = line.substr(0, comment_pos);
        
        // Elimina espacios al inicio y fin
        line = std::regex_replace(line, std::regex(R"(^\s+|\s+$)"), "");
        
        // Ignora líneas vacías y etiquetas
        if (line.empty()) 
            continue;
            
        if (line.back() == ':') {
            std::cout << "[Parser] Skipping label: " << line << std::endl;
            continue;
        }

        std::smatch match;
        
        if (std::regex_match(line, match, instrRegex)) {
            Opcode op = toOpcode(match[1]);
            if (op == Opcode::INVALID) {
                std::cerr << "[Parser] Warning: Unknown opcode '" << match[1] 
                         << "' at line " << lineNum << std::endl;
                continue;
            }

            // Función para limpiar operandos
            auto clean_operand = [](const std::string& s) -> std::string {
                if (s.empty()) return "";
                std::string result = s;
                // Eliminar espacios
                result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
                // Eliminar corchetes si están presentes
                if (result.size() >= 2 && result.front() == '[' && result.back() == ']') {
                    result = result.substr(1, result.size() - 2);
                }
                return result;
            };

            std::string dest = clean_operand(match[2]);
            std::string src1 = (match.size() > 3 && match[3].matched) ? clean_operand(match[3]) : "";
            std::string src2 = (match.size() > 4 && match[4].matched) ? clean_operand(match[4]) : "";

            instructions.push_back({op, dest, src1, src2, ""});
            
            std::cout << "[Parser] Parsed: " << match[1] << " dest='" << dest 
                      << "' src1='" << src1 << "' src2='" << src2 << "'" << std::endl;
                      
        } else {
            std::cerr << "[Parser] Warning: Cannot parse line " << lineNum 
                     << ": " << line << std::endl;
        }
    }
    
    std::cout << "[Parser] Loaded " << instructions.size() 
              << " instructions from " << filename << std::endl;
    
    return instructions;
}