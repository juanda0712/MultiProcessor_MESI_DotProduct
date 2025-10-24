#include <sstream>
#include "processing_element.hpp"
#include <chrono>
#include <cmath>
#include "cache.hpp"

/* ---------- Register File ---------- */
RegisterFile::RegisterFile()
{
    for (int i = 0; i < 8; ++i)
        regs_["REG" + std::to_string(i)] = 0;
}

uint64_t& RegisterFile::get(const std::string &name)
{
    return regs_[name];
}

std::string RegisterFile::dump_str() const
{
    std::stringstream ss;
    ss << "Registers:\n";
    for (int i = 0; i < 8; ++i) {
        std::string regName = "REG" + std::to_string(i);
        auto it = regs_.find(regName);
        if (it != regs_.end()) {
            ss << "  " << regName << " = " << it->second << "\n";
        } else {
            ss << "  " << regName << " = [UNDEFINED]\n";
        }
    }
    return ss.str();
}

void RegisterFile::dump() const
{
    std::cout << dump_str();
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

    std::cout << "\n[ControlUnit] Ejecutando instrucción en PC=" << current_pc
              << " → Opcode=" << instr.opcodeName()  // Asumiendo que tienes un método que devuelve el nombre del opcode
              << " Dest=" << instr.dest
              << " Src1=" << instr.src1
              << " Src2=" << instr.src2
              << std::endl;

    if (instr.opcode == Opcode::LOAD)
    {
        uint64_t val;
        if (instr.src1.back() == 'M') {  // sufijo "_M" indica acceso a memoria
            std::string reg = instr.src1.substr(0, instr.src1.size() - 2);
            uint64_t addr = (uint64_t)rf_->get(reg);   // REG contiene dirección
            val = cache_->cpu_load(addr);

            std::cout << "[LOAD] Mem[" << reg << "] dirección " << addr
                    << ", valor cargado " << val
                    << " → " << instr.dest << std::endl;
        } else {
            // Cargar de registro directo (opcional)
            val = (uint64_t)rf_->get(instr.src1);
            std::cout << "[LOAD] Registro " << instr.src1 
                    << " valor cargado " << val 
                    << " → " << instr.dest << std::endl;
        }
        rf_->get(instr.dest) = static_cast<double>(val);
    }
    else if (instr.opcode == Opcode::STORE)
    {
        uint64_t addr;
        uint64_t data = static_cast<uint64_t>(rf_->get(instr.src1));

        if (instr.dest.back() == 'M') {  // sufijo "_M" indica destino memoria
            std::string reg = instr.dest.substr(0, instr.dest.size() - 2);
            addr = (uint64_t)rf_->get(reg);
            cache_->cpu_store(addr, data);

            // Reducir verbosidad para evitar spam de consola
            // std::cout << "[STORE] Guardando valor " << data
            //         << " en Mem[" << reg << "] dirección " << addr << std::endl;
            // std::cout.flush(); // Forzar escritura del buffer
        } else {
            std::cerr << "[STORE] Error: STORE requiere destino de memoria con corchetes" << std::endl;
        }
    }

    else if (instr.opcode == Opcode::FMUL || instr.opcode == Opcode::FADD)
    {
        double a = rf_->get(instr.src1);
        double b = rf_->get(instr.src2);
        double res = alu_->execute(instr.opcode == Opcode::FMUL ? "FMUL" : "FADD", a, b);

        rf_->get(instr.dest) = res;

        std::cout << "[" << (instr.opcode == Opcode::FMUL ? "FMUL" : "FADD") << "] "
                  << instr.src1 << "=" << a << ", " << instr.src2 << "=" << b
                  << " → " << instr.dest << "=" << res << std::endl;
    }
    else if (instr.opcode == Opcode::INC)
    {
        double old = rf_->get(instr.dest);
        rf_->get(instr.dest)++;
        std::cout << "[INC] " << instr.dest << ": " << old << " → " << rf_->get(instr.dest) << std::endl;
    }
    else if (instr.opcode == Opcode::DEC)
    {
        double old = rf_->get(instr.dest);
        rf_->get(instr.dest)--;
        std::cout << "[DEC] " << instr.dest << ": " << old << " → " << rf_->get(instr.dest) << std::endl;
    }
    else if (instr.opcode == Opcode::JNZ)
    {
        double condition = rf_->get(instr.condition_reg);
        if (condition != 0)
        {
            try
            {
                size_t target = std::stoul(instr.dest);
                std::cout << "[JNZ] Salto a " << target << " porque "
                          << instr.condition_reg << "=" << condition << std::endl;
                new_pc = target;
            }
            catch (const std::exception &e)
            {
                std::cerr << "[ControlUnit] Error parsing jump target: " << e.what() << std::endl;
            }
        }
        else
        {
            std::cout << "[JNZ] No salta porque " << instr.condition_reg << "=0" << std::endl;
        }
    }

    // Comentado: demasiada salida de debug puede causar que se cuelgue
    // std::cout << "[ControlUnit] Dump de registros:\n" << rf_->dump_str() << std::endl;
    // std::cout.flush(); // Forzar escritura del buffer

    return new_pc;
}


/* ---------- Processing Element ---------- */
ProcessingElement::ProcessingElement(int id, Cache *cache)
    : id_(id), regFile_(), alu_(), control_(&regFile_, &alu_, cache), cache_(cache) {}

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
    ss << "[PE" << id_ << "] Starting execution with " << program_.size() << " instructions\n";
    ss << "[PE" << id_ << "] Initial registers:\n" << regFile_.dump_str() << "\n";

    size_t instruction_count = 0;
    const size_t MAX_INSTRUCTIONS = 10000; // Prevenir bucles infinitos

    while (pc_ < program_.size() && instruction_count < MAX_INSTRUCTIONS)
    {
        const Instruction &instr = program_[pc_];

        // Solo mostrar algunas instrucciones para evitar spam
        if (instruction_count < 10 || instruction_count % 100 == 0) {
            ss << "[PE" << id_ << "] PC=" << pc_ << " #" << instruction_count << " Executing: ";
            switch (instr.opcode)
            {
            case Opcode::LOAD:  ss << "LOAD " << instr.dest << ", [" << instr.src1 << "]"; break;
            case Opcode::STORE: ss << "STORE " << instr.dest << ", [" << instr.src1 << "]"; break;
            case Opcode::FMUL:  ss << "FMUL " << instr.dest << ", " << instr.src1 << ", " << instr.src2; break;
            case Opcode::FADD:  ss << "FADD " << instr.dest << ", " << instr.src1 << ", " << instr.src2; break;
            case Opcode::INC:   ss << "INC " << instr.dest; break;
            case Opcode::DEC:   ss << "DEC " << instr.dest; break;
            case Opcode::JNZ:   ss << "JNZ " << instr.dest << " (REG3=" << regFile_.get("REG3") << ")"; break;
            default:            ss << "UNKNOWN"; break;
            }
            ss << "\n";
        }

        size_t old_pc = pc_;
        pc_ = control_.executeInstruction(instr, old_pc);
        instruction_count++;

        if (instr.opcode == Opcode::JNZ && (instruction_count < 10 || instruction_count % 100 == 0))
        {
            if (old_pc != pc_)
                ss << "[PE" << id_ << "] Jump taken from " << old_pc << " to PC=" << pc_ << "\n";
            else
                ss << "[PE" << id_ << "] Jump not taken, continuing to PC=" << pc_ << "\n";
        }

        // Solo mostrar registros ocasionalmente
        if (instruction_count < 5 || instruction_count % 500 == 0) {
            ss << "[PE" << id_ << "] Registers after instruction #" << instruction_count << ":\n" << regFile_.dump_str() << "\n";
        }

        // Quitar el sleep que ralentiza mucho
        //std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (instruction_count >= MAX_INSTRUCTIONS) {
        ss << "[PE" << id_ << "] WARNING: Execution stopped after " << MAX_INSTRUCTIONS << " instructions (possible infinite loop)\n";
    }

    ss << "[PE" << id_ << "] Execution end after " << instruction_count << " instructions.\n";
    ss << "[PE" << id_ << "] Final registers:\n" << regFile_.dump_str() << "\n";
    output_ = ss.str();
}

std::string ProcessingElement::getOutput() const
{
    return output_;
}

void ProcessingElement::dump_registers() const
{
    std::cout << "[PE" << id_ << "] Register dump:\n";
    std::cout << regFile_.dump_str() << std::endl;
}
