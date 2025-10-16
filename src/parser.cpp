#include "parser.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <map>

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
    std::map<std::string, size_t> labels;

    if (!file.is_open()) {
        std::cerr << "[Parser] Error: Cannot open file " << filename << std::endl;
        return instructions;
    }

    // Primera pasada: identificar etiquetas y sus posiciones
    std::vector<std::string> allLines;
    while (std::getline(file, line)) {
        allLines.push_back(line);
    }
    file.close();

    // En la primera pasada (identificar etiquetas):
size_t instructionCount = 0;
for (size_t i = 0; i < allLines.size(); ++i) {
    line = allLines[i];
    
    // Elimina comentarios
    size_t comment_pos = line.find_first_of(";#");
    if (comment_pos != std::string::npos) 
        line = line.substr(0, comment_pos);
    
    // Elimina espacios al inicio y fin
    line = std::regex_replace(line, std::regex(R"(^\s+|\s+$)"), "");
    
    if (line.empty()) continue;

    // Verifica si es una etiqueta
    if (line.back() == ':') {
        std::string label = line.substr(0, line.size() - 1);
        labels[label] = instructionCount; // La siguiente instrucción tendrá este índice
        std::cout << "[Parser] Found label: " << label << " -> instruction " << instructionCount << std::endl;
    } else {
        instructionCount++;
    }
}

    // Segunda pasada: parsear instrucciones
    instructionCount = 0;
    for (size_t i = 0; i < allLines.size(); ++i) {
        line = allLines[i];
        
        // Elimina comentarios
        size_t comment_pos = line.find_first_of(";#");
        if (comment_pos != std::string::npos) 
            line = line.substr(0, comment_pos);
        
        // Elimina espacios al inicio y fin
        line = std::regex_replace(line, std::regex(R"(^\s+|\s+$)"), "");
        
        // Ignora líneas vacías y etiquetas (ya las procesamos)
        if (line.empty() || line.back() == ':') continue;

        // Usa el mismo regex que funcionaba antes
        std::regex instrRegex(R"(^\s*([A-Z]+)\s+([^,\s]+)(?:\s*,\s*(\[?\s*[^,\]\s]+\s*\]?))?(?:\s*,\s*(\[?\s*[^,\]\s]+\s*\]?))?\s*$)");
        std::smatch match;

        if (std::regex_match(line, match, instrRegex)) {
            Opcode op = toOpcode(match[1]);
            if (op == Opcode::INVALID) {
                std::cerr << "[Parser] Warning: Unknown opcode '" << match[1] 
                         << "' at line " << (i+1) << std::endl;
                instructionCount++;
                continue;
            }

            // Limpia operandos
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
            std::string label = "";

            // Para JNZ, guarda la etiqueta original
            if (op == Opcode::JNZ) {
                label = dest;
            }

            instructions.push_back({op, dest, src1, src2, label});
            
            std::cout << "[Parser] Parsed: " << match[1] << " dest='" << dest 
                      << "' src1='" << src1 << "' src2='" << src2 << "'" << std::endl;
        } else {
            std::cerr << "[Parser] Warning: Cannot parse line " << (i+1) 
                     << ": " << line << std::endl;
        }
        
        instructionCount++;
    }

    // Tercera pasada: resolver etiquetas en JNZ
    for (auto& instr : instructions) {
        if (instr.opcode == Opcode::JNZ && !instr.label.empty()) {
            auto it = labels.find(instr.label);
            if (it != labels.end()) {
                // Convierte la posición de la etiqueta a string para el destino
                instr.dest = std::to_string(it->second);
                std::cout << "[Parser] Resolved JNZ to label '" << instr.label 
                          << "' -> instruction " << instr.dest << std::endl;
            } else {
                std::cerr << "[Parser] Error: Unknown label '" << instr.label << "'" << std::endl;
            }
        }
    }
    
    std::cout << "[Parser] Loaded " << instructions.size() 
              << " instructions from " << filename << std::endl;
    
    return instructions;
}