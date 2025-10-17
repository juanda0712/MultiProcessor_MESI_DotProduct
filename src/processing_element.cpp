#include <sstream>
#include "processing_element.hpp"
#include <chrono>
#include <cmath>

#include "cache.hpp"

// Definición correcta del método de RegisterFile
std::string RegisterFile::dump_str() const
{
    std::stringstream ss;
    ss << "Registers:\n";
    for (int i = 0; i < 8; ++i)
    {
        std::string regName = "REG" + std::to_string(i);
        ss << "  " << regName << " = " << regs_.at(regName) << "\n";
    }
    return ss.str();
}

/* ---------- Register File ---------- */
RegisterFile::RegisterFile()
{
    for (int i = 0; i < 8; ++i)
        regs_["REG" + std::to_string(i)] = 0.0;
}

double &RegisterFile::get(const std::string &name)
{
    return regs_[name];
}

void RegisterFile::dump()
{
    std::cout << dump_str();
}

// Definición correcta del método de ProcessingElement
std::string ProcessingElement::getOutput() const
{
    return output_;
}

/* ---------- ALU ---------- */
double ALU::execute(const std::string &op, double a, double b)
{
    if (op == "FMUL")
        return a * b;
    if (op == "FADD")
        return a + b;
    return 0.0;
}

/* ---------- Control Unit ---------- */
ControlUnit::ControlUnit(RegisterFile *rf, ALU *alu, Cache *cache)
    : rf_(rf), alu_(alu), cache_(cache) {}

size_t ControlUnit::executeInstruction(const Instruction &instr, size_t current_pc)
{
    size_t new_pc = current_pc + 1;

    if (instr.opcode == Opcode::LOAD)
    {
        uint64_t addr = (uint64_t)rf_->get(instr.src1);
        uint64_t val = cache_->cpu_load(addr);
        rf_->get(instr.dest) = static_cast<double>(val);
    }
    else if (instr.opcode == Opcode::STORE)
    {
        uint64_t addr = (uint64_t)rf_->get(instr.src1);
        uint64_t data = static_cast<uint64_t>(rf_->get(instr.dest));
        cache_->cpu_store(addr, data);
    }
    else if (instr.opcode == Opcode::FMUL || instr.opcode == Opcode::FADD)
    {
        rf_->get(instr.dest) = alu_->execute(
            instr.opcode == Opcode::FMUL ? "FMUL" : "FADD",
            rf_->get(instr.src1), rf_->get(instr.src2));
    }
    else if (instr.opcode == Opcode::INC)
    {
        rf_->get(instr.dest)++;
    }
    else if (instr.opcode == Opcode::DEC)
    {
        rf_->get(instr.dest)--;
    }
    else if (instr.opcode == Opcode::JNZ)
    {
        // Usar el registro de condición específico (REG3)
        double condition = rf_->get(instr.condition_reg);

        if (condition != 0)
        {
            try
            {
                size_t target = std::stoul(instr.dest);
                new_pc = target;
            }
            catch (const std::exception &e)
            {
                std::cerr << "[ControlUnit] Error parsing jump target: " << e.what() << std::endl;
            }
        }
    }

    return new_pc;
}

/* ---------- Processing Element ---------- */
ProcessingElement::ProcessingElement(int id, Cache *cache)
    : id_(id), cache_(cache), control_(&regFile_, &alu_, cache) {}

void ProcessingElement::loadProgram(const std::vector<Instruction> &program)
{
    program_ = program;
}

void ProcessingElement::initializeRegisters(double baseA, double baseB, double partialAddr, double count)
{
    regFile_.get("REG0") = baseA;
    regFile_.get("REG1") = baseB;
    regFile_.get("REG2") = partialAddr;
    regFile_.get("REG3") = count;
}

void ProcessingElement::start()
{
    thread_ = std::thread(&ProcessingElement::run, this);
}

void ProcessingElement::join()
{
    if (thread_.joinable())
        thread_.join();
}

void ProcessingElement::run()
{
    std::stringstream ss;
    ss << "[PE" << id_ << "] Execution start.\n";

    while (pc_ < program_.size())
    {
        ss << "[PE" << id_ << "] PC=" << pc_ << " Executing: ";

        const Instruction &instr = program_[pc_];

        // Mostrar la instrucción actual
        switch (instr.opcode)
        {
        case Opcode::LOAD:
            ss << "LOAD " << instr.dest << ", [" << instr.src1 << "]";
            break;
        case Opcode::STORE:
            ss << "STORE " << instr.dest << ", [" << instr.src1 << "]";
            break;
        case Opcode::FMUL:
            ss << "FMUL " << instr.dest << ", " << instr.src1 << ", " << instr.src2;
            break;
        case Opcode::FADD:
            ss << "FADD " << instr.dest << ", " << instr.src1 << ", " << instr.src2;
            break;
        case Opcode::INC:
            ss << "INC " << instr.dest;
            break;
        case Opcode::DEC:
            ss << "DEC " << instr.dest;
            break;
        case Opcode::JNZ:
            ss << "JNZ " << instr.dest << " (REG3=" << regFile_.get("REG3") << ")";
            break;
        default:
            ss << "UNKNOWN";
            break;
        }
        ss << "\n";

        size_t old_pc = pc_;

        // Ejecutar instrucción y obtener nuevo PC
        pc_ = control_.executeInstruction(instr, old_pc);

        // Mostrar si hubo salto
        if (instr.opcode == Opcode::JNZ)
        {
            if (old_pc != pc_)
            {
                ss << "[PE" << id_ << "] Jump taken from " << old_pc << " to PC=" << pc_ << "\n";
            }
            else
            {
                ss << "[PE" << id_ << "] Jump not taken, continuing to PC=" << pc_ << "\n";
            }
        }

        // Mostrar registros relevantes periódicamente
        if (instr.opcode == Opcode::FADD || instr.opcode == Opcode::JNZ)
        {
            ss << "[PE" << id_ << "] REG3=" << regFile_.get("REG3")
               << ", REG4=" << regFile_.get("REG4") << "\n";
        }

        // Pequeña pausa para evitar salida masiva
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ss << "[PE" << id_ << "] Execution end.\n";
    ss << regFile_.dump_str();
    output_ = ss.str();
}