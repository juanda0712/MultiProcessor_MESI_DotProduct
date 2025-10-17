#include "memory.hpp"
#include <fstream>
#include <sstream>
#include <thread>
#include <algorithm>
#include <cstring>

MainMemory::MainMemory(size_t sizeBytes)
{
    size_t words = std::max<size_t>(1, sizeBytes / WORD_SIZE);
    mem_.assign(words, 0);
}

std::array<uint64_t, WORDS_PER_LINE> MainMemory::read_line(uint64_t base) {
    std::lock_guard<std::mutex> lk(mtx_);
    maybe_sleep_();
    std::array<uint64_t, WORDS_PER_LINE> out{};
    // base debe estar alineado a LINE_SIZE, pero por robustez no fallamos; redondeamos hacia abajo
    if (align_check_ && (base % LINE_SIZE) != 0) { ++misalign_count_; }
    uint64_t aligned = line_base(base);
    size_t w0 = static_cast<size_t>((aligned / WORD_SIZE));
    ensure_capacity_(w0, WORDS_PER_LINE);
    for (size_t i = 0; i < WORDS_PER_LINE; ++i) {
        size_t idx = w0 + i;
        out[i] = idx < mem_.size() ? mem_[idx] : 0;
    }
    stats_.line_reads++;
    stats_.bytes_read += LINE_SIZE;
    return out;
}

void MainMemory::write_line(uint64_t base, const std::array<uint64_t, WORDS_PER_LINE>& ln) {
    std::lock_guard<std::mutex> lk(mtx_);
    maybe_sleep_();
    if (align_check_ && (base % LINE_SIZE) != 0) { ++misalign_count_; }
    uint64_t aligned = line_base(base);
    size_t w0 = static_cast<size_t>((aligned / WORD_SIZE));
    ensure_capacity_(w0, WORDS_PER_LINE);
    for (size_t i = 0; i < WORDS_PER_LINE; ++i) {
        size_t idx = w0 + i;
        if (idx < mem_.size()) mem_[idx] = ln[i];
    }
    stats_.line_writes++;
    stats_.bytes_written += LINE_SIZE;
}

uint64_t MainMemory::read_word(uint64_t addr) {
    std::lock_guard<std::mutex> lk(mtx_);
    maybe_sleep_();
    if (align_check_ && (addr % WORD_SIZE) != 0) { ++misalign_count_; }
    size_t wi = index_from_addr_(addr);
    ensure_capacity_(wi, 1);
    uint64_t v = wi < mem_.size() ? mem_[wi] : 0ULL;
    stats_.word_reads++;
    stats_.bytes_read += sizeof(uint64_t);
    return v;
}

void MainMemory::write_word(uint64_t addr, uint64_t value) {
    std::lock_guard<std::mutex> lk(mtx_);
    maybe_sleep_();
    if (align_check_ && (addr % WORD_SIZE) != 0) { ++misalign_count_; }
    size_t wi = index_from_addr_(addr);
    ensure_capacity_(wi, 1);
    if (wi < mem_.size()) mem_[wi] = value;
    stats_.word_writes++;
    stats_.bytes_written += sizeof(uint64_t);
}

void MainMemory::clear() {
    std::lock_guard<std::mutex> lk(mtx_);
    std::fill(mem_.begin(), mem_.end(), 0ULL);
}

void MainMemory::fill(uint64_t value) {
    std::lock_guard<std::mutex> lk(mtx_);
    std::fill(mem_.begin(), mem_.end(), value);
}

void MainMemory::fill_linear(uint64_t mul, uint64_t add) {
    std::lock_guard<std::mutex> lk(mtx_);
    for (size_t i = 0; i < mem_.size(); ++i) mem_[i] = static_cast<uint64_t>(i) * mul + add;
}

static bool parse_u64(const std::string& tok, uint64_t& out) {
    std::stringstream ss;
    if (tok.size() > 2 && (tok.rfind("0x", 0) == 0 || tok.rfind("0X", 0) == 0)) {
        ss << std::hex << tok;
    } else {
        ss << tok;
    }
    ss >> out;
    return !ss.fail();
}

bool MainMemory::load_from_text(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::lock_guard<std::mutex> lk(mtx_);
    size_t cursor = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string a, b;
        if (!(iss >> a)) continue;
        if (a[0] == '#') continue;
        if (iss >> b) {
            // formato: index value
            uint64_t idx64 = 0, val = 0;
            if (!parse_u64(a, idx64)) continue;
            if (!parse_u64(b, val)) continue;
            size_t idx = static_cast<size_t>(idx64);
            if (idx >= mem_.size()) ensure_capacity_(idx, 1);
            if (idx < mem_.size()) mem_[idx] = val;
        } else {
            // formato: value (secuencial)
            uint64_t val = 0;
            if (!parse_u64(a, val)) continue;
            if (cursor >= mem_.size()) ensure_capacity_(cursor, 1);
            if (cursor < mem_.size()) mem_[cursor++] = val;
        }
    }
    return true;
}

size_t MainMemory::size_bytes() const { return mem_.size() * WORD_SIZE; }
size_t MainMemory::size_words() const { return mem_.size(); }

void MainMemory::set_auto_expand(bool enable) { auto_expand_ = enable; }
bool MainMemory::auto_expand() const { return auto_expand_; }

void MainMemory::set_latency_ns(uint64_t ns) { latency_ns_ = ns; }
void MainMemory::enable_latency(bool en) { latency_enabled_ = en; }

void MainMemory::reset_stats() {
    std::lock_guard<std::mutex> lk(mtx_);
    stats_ = Stats{};
}

size_t MainMemory::index_from_addr_(uint64_t addr) const {
    return static_cast<size_t>(addr / WORD_SIZE);
}

void MainMemory::maybe_sleep_() const {
    if (latency_enabled_ && latency_ns_ > 0) {
        std::this_thread::sleep_for(std::chrono::nanoseconds(latency_ns_));
    }
}

void MainMemory::ensure_capacity_(size_t wordIndex, size_t wordsNeeded) {
    if (!auto_expand_) {
        // sin expansión, no exceder; el caller verifica bounds y usa defaults
        return;
    }
    size_t need = wordIndex + wordsNeeded;
    if (need > mem_.size()) {
        mem_.resize(need, 0ULL);
    }
}

static inline uint16_t bswap16(uint16_t x) { return static_cast<uint16_t>((x>>8) | (x<<8)); }
static inline uint32_t bswap32(uint32_t x) {
    return ((x & 0x000000FFu) << 24) |
           ((x & 0x0000FF00u) << 8)  |
           ((x & 0x00FF0000u) >> 8)  |
           ((x & 0xFF000000u) >> 24);
}
static inline uint64_t bswap64(uint64_t x) {
    return ((x & 0x00000000000000FFull) << 56) |
           ((x & 0x000000000000FF00ull) << 40) |
           ((x & 0x0000000000FF0000ull) << 24) |
           ((x & 0x00000000FF000000ull) << 8)  |
           ((x & 0x000000FF00000000ull) >> 8)  |
           ((x & 0x0000FF0000000000ull) >> 24) |
           ((x & 0x00FF000000000000ull) >> 40) |
           ((x & 0xFF00000000000000ull) >> 56);
}

bool MainMemory::read_bytes(uint64_t addr, void* dst, size_t len) {
    if (align_check_ && (addr % 1) != 0) { ++misalign_count_; }
    std::lock_guard<std::mutex> lk(mtx_);
    maybe_sleep_();
    uint8_t* out = static_cast<uint8_t*>(dst);
    for (size_t i = 0; i < len; ++i) {
        size_t wi = index_from_addr_(addr + i);
        size_t byte_off = static_cast<size_t>((addr + i) % WORD_SIZE);
        ensure_capacity_(wi, 1);
        uint64_t word = wi < mem_.size() ? mem_[wi] : 0ULL;
        uint8_t b = static_cast<uint8_t>((word >> (byte_off * 8)) & 0xFF);
        out[i] = b;
    }
    stats_.bytes_read += len;
    return true;
}

bool MainMemory::write_bytes(uint64_t addr, const void* src, size_t len) {
    if (align_check_ && (addr % 1) != 0) { ++misalign_count_; }
    std::lock_guard<std::mutex> lk(mtx_);
    maybe_sleep_();
    const uint8_t* in = static_cast<const uint8_t*>(src);
    for (size_t i = 0; i < len; ++i) {
        size_t wi = index_from_addr_(addr + i);
        size_t byte_off = static_cast<size_t>((addr + i) % WORD_SIZE);
        ensure_capacity_(wi, 1);
        if (wi < mem_.size()) {
            uint64_t word = mem_[wi];
            uint64_t mask = ~(0xFFULL << (byte_off * 8));
            word = (word & mask) | (static_cast<uint64_t>(in[i]) << (byte_off * 8));
            mem_[wi] = word;
        }
    }
    stats_.bytes_written += len;
    return true;
}

uint8_t  MainMemory::read_u8(uint64_t addr)  { uint8_t v=0; read_bytes(addr, &v, sizeof(v)); return v; }
uint16_t MainMemory::read_u16(uint64_t addr) {
    uint16_t v=0; read_bytes(addr, &v, sizeof(v));
    if (endianness_ == Endianness::Big) v = bswap16(v);
    return v;
}
uint32_t MainMemory::read_u32(uint64_t addr) {
    uint32_t v=0; read_bytes(addr, &v, sizeof(v));
    if (endianness_ == Endianness::Big) v = bswap32(v);
    return v;
}
void MainMemory::write_u8(uint64_t addr, uint8_t v)  { write_bytes(addr, &v, sizeof(v)); }
void MainMemory::write_u16(uint64_t addr, uint16_t v){ if (endianness_==Endianness::Big) v=bswap16(v); write_bytes(addr, &v, sizeof(v)); }
void MainMemory::write_u32(uint64_t addr, uint32_t v){ if (endianness_==Endianness::Big) v=bswap32(v); write_bytes(addr, &v, sizeof(v)); }

bool MainMemory::load_from_binary(const std::string& path, uint64_t baseAddr) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    std::vector<char> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return write_bytes(baseAddr, buf.data(), buf.size());
}

bool MainMemory::dump_to_binary(const std::string& path, uint64_t baseAddr, size_t lenBytes) const {
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    std::vector<uint8_t> buf(lenBytes);
    const_cast<MainMemory*>(this)->read_bytes(baseAddr, buf.data(), buf.size());
    f.write(reinterpret_cast<const char*>(buf.data()), buf.size());
    return true;
}

bool MainMemory::dump_to_text(const std::string& path, uint64_t baseAddr, size_t lenBytes, size_t bytesPerLine) const {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    std::vector<uint8_t> buf(lenBytes);
    const_cast<MainMemory*>(this)->read_bytes(baseAddr, buf.data(), buf.size());
    for (size_t i = 0; i < buf.size(); i += bytesPerLine) {
        f << std::hex;
        f << "0x" << (baseAddr + i) << ": ";
        for (size_t j = 0; j < bytesPerLine && i + j < buf.size(); ++j) {
            uint8_t b = buf[i + j];
            f << (b < 16 ? "0" : "") << static_cast<int>(b) << ' ';
        }
        f << '\n';
    }
    return true;
}
