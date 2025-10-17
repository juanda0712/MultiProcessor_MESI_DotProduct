// Testbench autocontenido de memoria y coherencia MESI
#include <iostream>
#include <iomanip>
#include <vector>
#include <cstdint>
#include "cache.hpp"
#include "interconnect.hpp"
#include "memory.hpp"

struct TestCtx {
    int passed{0};
    int failed{0};
};

static void assert_eq_u64(TestCtx& t, const char* name, uint64_t a, uint64_t b) {
    if (a == b) {
        std::cout << "[PASS] " << name << ": " << std::hex << std::showbase << a << std::dec << "\n";
        t.passed++;
    } else {
        std::cout << "[FAIL] " << name << ": got " << std::hex << std::showbase << a
                  << ", expected " << b << std::dec << "\n";
        t.failed++;
    }
}

static void run_memory_api_tests(MainMemory& mem, TestCtx& T) {
    std::cout << "\n== Mem API tests ==\n";
    mem.clear();
    uint64_t a = 0x1000;
    mem.write_word(a, 0x1122334455667788ull);
    assert_eq_u64(T, "read_word@0x1000", mem.read_word(a), 0x1122334455667788ull);

    // bytes cruzando límites de palabra
    uint8_t bufw[6] = {0xDE,0xAD,0xBE,0xEF,0xCA,0xFE};
    mem.write_bytes(a + WORD_SIZE - 3, bufw, sizeof(bufw));
    uint8_t bufr[6]{};
    mem.read_bytes(a + WORD_SIZE - 3, bufr, sizeof(bufr));
    bool ok=true; for (int i=0;i<6;++i) ok &= (bufw[i]==bufr[i]);
    std::cout << (ok?"[PASS]":"[FAIL]") << " read/write_bytes crossing word boundary\n";
    if (ok) T.passed++; else T.failed++;

    // endianness para 16/32
    mem.set_endianness(MainMemory::Endianness::Big);
    mem.write_u16(a+0, 0x1234);
    mem.write_u32(a+2, 0xA1B2C3D4u);
    assert_eq_u64(T, "read_u16 big endian", mem.read_u16(a+0), 0x1234);
    assert_eq_u64(T, "read_u32 big endian", mem.read_u32(a+2), 0xA1B2C3D4u);
    mem.set_endianness(MainMemory::Endianness::Little);
}

static void run_mesi_scenario(MainMemory& mem, TestCtx& T) {
    std::cout << "\n== MESI Coherency scenario ==\n";
    Interconnect ic(&mem);
    Cache c0(0, &ic, &mem);
    Cache c1(1, &ic, &mem);
    ic.attach(&c0);
    ic.attach(&c1);

    uint64_t base = line_base(0x2000);
    // Inicializa línea base con valores conocidos
    auto line = mem.read_line(base);
    for (size_t i=0;i<WORDS_PER_LINE;++i) line[i] = 0xAAAABBBBCCCC0000ull + i;
    mem.write_line(base, line);

    // Paso 1: c0 lee -> E
    auto v0 = c0.cpu_load(base);
    std::cout << "[Step1] c0 load base=0x" << std::hex << base << std::dec << ", value=" << std::hex << v0 << std::dec << "\n";
    c0.dump_state(); c1.dump_state();

    // Paso 2: c1 lee -> ambos S
    auto v1 = c1.cpu_load(base);
    std::cout << "[Step2] c1 load base=0x" << std::hex << base << std::dec << ", value=" << std::hex << v1 << std::dec << "\n";
    c0.dump_state(); c1.dump_state();

    // Paso 3: c0 escribe -> Upgrade, c1 invalida, c0 M
    uint64_t newv = 0x5555666677778888ull;
    c0.cpu_store(base, newv);
    std::cout << "[Step3] c0 store newv=" << std::hex << newv << std::dec << "\n";
    c0.dump_state(); c1.dump_state();

    // Paso 4: c1 lee -> c0 M hace write-back y degrada a S; c1 S
    auto v1b = c1.cpu_load(base);
    std::cout << "[Step4] c1 reload value=" << std::hex << v1b << std::dec << "\n";
    c0.dump_state(); c1.dump_state();
    assert_eq_u64(T, "value propagated to memory/other caches", v1b, newv);

    // Paso 5: forzar eviction de la línea M/S en c0 mediante thrash del mismo set
    // Usamos stride de 16 sets * LINE_SIZE para mapear al mismo set (con la config actual)
    const uint64_t stride = 16ull * LINE_SIZE; // depende de la cache actual (NUM_SETS=16)
    for (int k=1; k<=3; ++k) {
        uint64_t other = base + stride * k;
        (void)c0.cpu_load(other); // traer nuevas líneas y forzar reemplazo
    }
    // Verificar que memoria persiste el valor escrito
    auto line_after = mem.read_line(base);
    assert_eq_u64(T, "writeback persists after eviction", line_after[0], newv);
}

int main() {
    TestCtx T{};
    MainMemory mem; // 1 MiB por defecto
    mem.set_auto_expand(true);
    mem.enable_latency(false);

    std::cout << "[Main] Params: LINE_SIZE=" << LINE_SIZE
              << ", WORD_SIZE=" << WORD_SIZE
              << ", WORDS_PER_LINE=" << WORDS_PER_LINE
              << ", MEM(bytes)=" << mem.size_bytes() << "\n";

    run_memory_api_tests(mem, T);
    run_mesi_scenario(mem, T);

    auto st = mem.stats();
    std::cout << "\n== Mem Stats ==\n"
              << "line_reads=" << st.line_reads
              << ", line_writes=" << st.line_writes
              << ", word_reads=" << st.word_reads
              << ", word_writes=" << st.word_writes
              << ", bytes_read=" << st.bytes_read
              << ", bytes_written=" << st.bytes_written << "\n";

    std::cout << "\n== Summary ==\n";
    std::cout << "Passed: " << T.passed << ", Failed: " << T.failed << "\n";
    return (T.failed==0)?0:1;
}
