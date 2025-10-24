#include "parser.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <map>

static Opcode toOpcode(const std::string &op)
{
    if (op == "LOAD")
        return Opcode::LOAD;
    if (op == "STORE")
        return Opcode::STORE;
    if (op == "FMUL")
        return Opcode::FMUL;
    if (op == "FADD")
        return Opcode::FADD;
    if (op == "INC")
        return Opcode::INC;
    if (op == "DEC")
        return Opcode::DEC;
    if (op == "JNZ")
        return Opcode::JNZ;
    return Opcode::INVALID;
}

std::vector<Instruction> Parser::parseFile(const std::string &filename)
{
    std::ifstream file(filename);
    std::vector<Instruction> instructions;
    std::string line;
    std::map<std::string, size_t> labels;

    if (!file.is_open())
    {
        std::cerr << "[Parser] Error: Cannot open file " << filename << std::endl;
        return instructions;
    }

    // Primera pasada: identificar etiquetas
    std::vector<std::string> allLines;
    while (std::getline(file, line))
    {
        allLines.push_back(line);
    }
    file.close();

    // Segunda pasada: construir instrucciones y mapear etiquetas
    for (size_t i = 0; i < allLines.size(); ++i)
    {
        line = allLines[i];

        // Elimina comentarios
        size_t comment_pos = line.find_first_of(";#");
        if (comment_pos != std::string::npos)
            line = line.substr(0, comment_pos);

        // Elimina espacios al inicio y fin
        line = std::regex_replace(line, std::regex(R"(^\s+|\s+$)"), "");

        if (line.empty())
            continue;

        // Verifica si es una etiqueta
        if (line.back() == ':')
        {
            std::string label = line.substr(0, line.size() - 1);
            // La etiqueta apunta a la siguiente instrucción que se va a agregar
            labels[label] = instructions.size();
            std::cout << "[Parser] Found label: " << label << " -> instruction " << instructions.size() << std::endl;
            continue;
        }

        // Parsear instrucción normal
        std::regex instrRegex(R"(^\s*([A-Z]+)\s+([^,\s]+)(?:\s*,\s*(\[?\s*[^,\]\s]+\s*\]?))?(?:\s*,\s*(\[?\s*[^,\]\s]+\s*\]?))?\s*$)");
        std::smatch match;

        if (std::regex_match(line, match, instrRegex))
        {
            Opcode op = toOpcode(match[1]);
            if (op == Opcode::INVALID)
            {
                std::cerr << "[Parser] Warning: Unknown opcode '" << match[1]
                          << "' at line " << (i + 1) << std::endl;
                continue;
            }

            auto clean_operand = [](const std::string &s) -> std::string
            {
                if (s.empty())
                    return "";

                std::string result = s;
                result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());

                // Detectar corchetes para acceso a memoria
                bool is_mem = false;
                if (result.size() >= 2 && result.front() == '[' && result.back() == ']') {
                    result = result.substr(1, result.size() - 2); // quitar corchetes
                    is_mem = true;
                }

                // Guardar la info de si es memoria usando un sufijo por ejemplo "_M"
                if (is_mem)
                    result += "_M";

                return result;
            };


            std::string dest = clean_operand(match[2]);
            std::string src1 = (match.size() > 3 && match[3].matched) ? clean_operand(match[3]) : "";
            std::string src2 = (match.size() > 4 && match[4].matched) ? clean_operand(match[4]) : "";
            std::string label = "";

            if (op == Opcode::JNZ)
            {
                label = dest;
                // Para JNZ, 'dest' es la etiqueta, necesitamos determinar qué registro usar
                // Por convención, JNZ usa REG3 como contador
                instructions.push_back({op, "", "", "", label, "REG3"});
            }
            else
            {
                instructions.push_back({op, dest, src1, src2, label, ""});
            }


            std::cout << "[Parser] Parsed instruction " << (instructions.size() - 1) << ": "
                      << match[1] << " dest='" << dest << "' src1='" << src1 << "' src2='" << src2 << "'" << std::endl;
        }
        else
        {
            std::cerr << "[Parser] Warning: Cannot parse line " << (i + 1)
                      << ": " << line << std::endl;
        }
    }

    // Tercera pasada: resolver etiquetas en JNZ
    for (size_t i = 0; i < instructions.size(); ++i)
    {
        auto &instr = instructions[i];
        if (instr.opcode == Opcode::JNZ && !instr.label.empty())
        {
            auto it = labels.find(instr.label);
            if (it != labels.end())
            {
                instr.dest = std::to_string(it->second);
                std::cout << "[Parser] Resolved JNZ at instruction " << i
                          << " to label '" << instr.label << "' -> instruction " << instr.dest << std::endl;
                std::cout << "[Parser] JNZ instruction at index " << i << ": dest='" << instr.dest
                          << "', label='" << instr.label << "', resolved to '" << std::to_string(it->second) << "'" << std::endl;
            }
            else
            {
                std::cerr << "[Parser] Error: Unknown label '" << instr.label << "' at instruction " << i << std::endl;
            }
        }
    }

    std::cout << "[Parser] Loaded " << instructions.size()
              << " instructions from " << filename << std::endl;

    return instructions;
}