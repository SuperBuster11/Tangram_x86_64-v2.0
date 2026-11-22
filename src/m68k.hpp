// Minimal Motorola 68000 interpreter (user mode), translated from the Go port's interpreter. It only exists to run
// the original music players (music1.img / music2.img are 68000 programs that drive Paula and a CIA timer).
#pragma once
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace tg {

class M68k {
public:
    static constexpr uint32_t kMemSize = 0x1000000;   // 24-bit address space

    struct LibRange { uint32_t lo, hi; std::function<void(M68k&, uint32_t target)> fn; };
    using IoFn = std::function<uint32_t(uint32_t addr, int size, uint32_t val, bool write)>;

    uint32_t D[8] = {}, A[8] = {}, PC = 0;
    uint32_t N = 0, Z = 0, V = 0, C = 0, X = 0;
    std::vector<uint8_t> mem = std::vector<uint8_t>(kMemSize + 8, 0);
    std::vector<LibRange> libs;
    IoFn io;                  // custom chips / CIA (0xBF0000-0xBFFFFF, 0xDF0000-0xDFFFFF)
    uint64_t steps = 0;

    uint32_t rd(uint32_t addr, int size);
    void wr(uint32_t addr, int size, uint32_t v);
    void push(uint32_t v);
    uint32_t pop();
    uint32_t ccr() const { return C | V << 1 | Z << 2 | N << 3 | X << 4; }
    void setCcr(uint32_t v) { C = v & 1; V = (v >> 1) & 1; Z = (v >> 2) & 1; N = (v >> 3) & 1; X = (v >> 4) & 1; }
    // Runs until PC == stop. Returns false (and sets err) on an illegal instruction / step limit.
    bool run(uint32_t stop, uint64_t maxSteps, std::string& err);
    void step();

private:
    struct Op { char k; uint32_t v; };
    bool isIo(uint32_t addr) const { return io && addr >= 0xBF0000 && (addr < 0xC00000 || (addr >= 0xDF0000 && addr < 0xE00000)); }
    uint32_t f16() { uint32_t v = rd(PC, 2); PC += 2; return v; }
    uint32_t f32() { uint32_t v = rd(PC, 4); PC += 4; return v; }
    Op ea(unsigned mode, unsigned reg, int size);
    uint32_t index(uint32_t base);
    uint32_t read(const Op& op, int size);
    void write(const Op& op, int size, uint32_t v);
    void setnz(uint32_t r, int size);
    void logicFlags(uint32_t r, int size);
    uint32_t doAdd(uint32_t s, uint32_t d, int size, uint32_t x, bool zkeep);
    uint32_t doSub(uint32_t s, uint32_t d, int size, uint32_t x, bool zkeep, bool storeX);
    bool cond(unsigned cc) const;
    void call(uint32_t target, uint32_t ret);
    void g0(uint16_t op); void move(uint16_t op, int size); void g4(uint16_t op); void movem(uint16_t op, unsigned mode, unsigned reg);
    void g5(uint16_t op); void g6(uint16_t op); void g8(uint16_t op); void addsub(uint16_t op, bool isAdd);
    void gb(uint16_t op); void gc(uint16_t op); void ge(uint16_t op);
    void aluDir(uint16_t op, const std::function<uint32_t(uint32_t, uint32_t)>& fn, int size, unsigned mode, unsigned reg);
    uint32_t shift(unsigned t, bool left, uint32_t v, uint32_t cnt, int size);
    uint32_t reg16(int i) const { return i < 8 ? D[i] : A[i - 8]; }
};

struct M68kHalt : std::runtime_error { using std::runtime_error::runtime_error; };

}  // namespace tg
