#include "m68k.hpp"

#include <cstdio>

namespace tg {
namespace {
const uint32_t kSzMask[5] = {0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF};
const unsigned kSzBits[5] = {0, 8, 16, 0, 32};
const int kOpSize[8] = {1, 2, 4, 0, 1, 2, 4, 0};

uint32_t sx(uint32_t v, int size) {
    if (size == 1) return uint32_t(int32_t(int8_t(v)));
    if (size == 2) return uint32_t(int32_t(int16_t(v)));
    return v;
}
uint32_t b2u(bool b) { return b ? 1 : 0; }
[[noreturn]] void halt(const char* what, unsigned a = 0, unsigned b = 0) {
    char buf[96]; std::snprintf(buf, sizeof buf, "%s %x %x", what, a, b);
    throw M68kHalt(buf);
}
}  // namespace

uint32_t M68k::rd(uint32_t addr, int size) {
    addr &= 0xFFFFFF;
    if (addr >= 0xBF0000 && isIo(addr)) return io(addr, size, 0, false);
    const uint8_t* m = mem.data();
    if (size == 1) return m[addr];
    if (size == 2) return uint32_t(m[addr]) << 8 | m[addr + 1];
    return uint32_t(m[addr]) << 24 | uint32_t(m[addr + 1]) << 16 | uint32_t(m[addr + 2]) << 8 | m[addr + 3];
}

void M68k::wr(uint32_t addr, int size, uint32_t v) {
    addr &= 0xFFFFFF;
    if (addr >= 0xBF0000 && isIo(addr)) { io(addr, size, v, true); return; }
    uint8_t* m = mem.data();
    if (size == 1) { m[addr] = uint8_t(v); }
    else if (size == 2) { m[addr] = uint8_t(v >> 8); m[addr + 1] = uint8_t(v); }
    else { m[addr] = uint8_t(v >> 24); m[addr + 1] = uint8_t(v >> 16); m[addr + 2] = uint8_t(v >> 8); m[addr + 3] = uint8_t(v); }
}

void M68k::push(uint32_t v) { A[7] -= 4; wr(A[7], 4, v); }
uint32_t M68k::pop() { uint32_t v = rd(A[7], 4); A[7] += 4; return v; }

M68k::Op M68k::ea(unsigned mode, unsigned reg, int size) {
    switch (mode) {
    case 0: return {'d', reg};
    case 1: return {'a', reg};
    case 2: return {'m', A[reg]};
    case 3: { uint32_t ad = A[reg]; uint32_t inc = size; if (size == 1 && reg == 7) inc = 2; A[reg] += inc; return {'m', ad}; }
    case 4: { uint32_t dec = size; if (size == 1 && reg == 7) dec = 2; A[reg] -= dec; return {'m', A[reg]}; }
    case 5: { uint32_t d = sx(f16(), 2); return {'m', A[reg] + d}; }
    case 6: return {'m', index(A[reg])};
    }
    switch (reg) {
    case 0: return {'m', sx(f16(), 2)};
    case 1: return {'m', f32()};
    case 2: { uint32_t base = PC; return {'m', base + sx(f16(), 2)}; }
    case 3: return {'m', index(PC)};
    case 4:
        if (size == 4) return {'i', f32()};
        return {'i', f16() & kSzMask[size]};
    }
    halt("bad ea", mode, reg);
}

uint32_t M68k::index(uint32_t base) {
    uint32_t ext = f16();
    unsigned r = (ext >> 12) & 7;
    uint32_t v = (ext & 0x8000) ? A[r] : D[r];
    if (!(ext & 0x800)) v = sx(v, 2);
    return base + sx(ext & 0xFF, 1) + v;
}

uint32_t M68k::read(const Op& op, int size) {
    switch (op.k) {
    case 'd': return D[op.v] & kSzMask[size];
    case 'a': return A[op.v] & kSzMask[size];
    case 'm': return rd(op.v, size);
    }
    return op.v & kSzMask[size];
}

void M68k::write(const Op& op, int size, uint32_t v) {
    switch (op.k) {
    case 'd':
        if (size == 4) D[op.v] = v;
        else { uint32_t m = kSzMask[size]; D[op.v] = (D[op.v] & ~m) | (v & m); }
        break;
    case 'a': A[op.v] = size == 2 ? sx(v, 2) : v; break;
    case 'm': wr(op.v, size, v); break;
    default: halt("write to immediate");
    }
}

void M68k::setnz(uint32_t r, int size) {
    r &= kSzMask[size];
    N = (r >> (kSzBits[size] - 1)) & 1;
    Z = b2u(r == 0);
}
void M68k::logicFlags(uint32_t r, int size) { setnz(r, size); V = 0; C = 0; }

uint32_t M68k::doAdd(uint32_t s, uint32_t d, int size, uint32_t x, bool zkeep) {
    uint32_t m = kSzMask[size];
    uint64_t full = uint64_t(s & m) + uint64_t(d & m) + uint64_t(x);
    uint32_t r = uint32_t(full) & m;
    unsigned sb = kSzBits[size] - 1;
    N = (r >> sb) & 1;
    if (!zkeep || r != 0) Z = b2u(r == 0);
    V = ((((s ^ r) & (d ^ r)) & m) >> sb) & 1;
    C = b2u(full > uint64_t(m));
    X = C;
    return r;
}

uint32_t M68k::doSub(uint32_t s, uint32_t d, int size, uint32_t x, bool zkeep, bool storeX) {
    uint32_t m = kSzMask[size];
    s &= m; d &= m;
    uint32_t r = (d - s - x) & m;
    unsigned sb = kSzBits[size] - 1;
    N = (r >> sb) & 1;
    if (!zkeep || r != 0) Z = b2u(r == 0);
    V = ((((d ^ s) & (d ^ r)) & m) >> sb) & 1;
    C = b2u(uint64_t(s) + uint64_t(x) > uint64_t(d));
    if (storeX) X = C;
    return r;
}

bool M68k::cond(unsigned cc) const {
    bool n = N != 0, z = Z != 0, v = V != 0, c = C != 0;
    switch (cc) {
    case 0: return true;
    case 1: return false;
    case 2: return !(c || z);
    case 3: return c || z;
    case 4: return !c;
    case 5: return c;
    case 6: return !z;
    case 7: return z;
    case 8: return !v;
    case 9: return v;
    case 10: return !n;
    case 11: return n;
    case 12: return n == v;
    case 13: return n != v;
    case 14: return !z && n == v;
    }
    return z || n != v;
}

void M68k::call(uint32_t target, uint32_t ret) {
    for (auto& l : libs) {
        if (target >= l.lo && target < l.hi) { PC = ret; l.fn(*this, target); return; }
    }
    push(ret);
    PC = target;
}

bool M68k::run(uint32_t stop, uint64_t maxSteps, std::string& err) {
    try {
        while (PC != stop) {
            step();
            if (maxSteps && steps >= maxSteps) {
                char b[64]; std::snprintf(b, sizeof b, "step limit (pc=%06x)", PC); err = b; return false;
            }
        }
    } catch (const M68kHalt& h) {
        char b[160]; std::snprintf(b, sizeof b, "%s (pc=%06x steps=%llu)", h.what(), PC, (unsigned long long)steps); err = b; return false;
    }
    return true;
}

void M68k::step() {
    uint32_t pc0 = PC;
    uint16_t o = uint16_t(f16());
    steps++;
    switch (o >> 12) {
    case 0: g0(o); break;
    case 1: move(o, 1); break;
    case 2: move(o, 4); break;
    case 3: move(o, 2); break;
    case 4: g4(o); break;
    case 5: g5(o); break;
    case 6: g6(o); break;
    case 7: { uint32_t v = sx(o & 0xFF, 1); D[(o >> 9) & 7] = v; logicFlags(v, 4); break; }
    case 8: g8(o); break;
    case 9: addsub(o, false); break;
    case 0xB: gb(o); break;
    case 0xC: gc(o); break;
    case 0xD: addsub(o, true); break;
    case 0xE: ge(o); break;
    default: halt("illegal opcode", o, pc0);
    }
}

void M68k::g0(uint16_t op) {
    unsigned mode = (op >> 3) & 7, reg = op & 7;
    if ((op & 0x100 && mode != 1) || (op & 0xF00) == 0x800) {
        uint32_t bit = (op & 0x100) ? D[(op >> 9) & 7] : f16();
        unsigned t = (op >> 6) & 3;
        Op e = ea(mode, reg, 1);
        uint32_t v; int size = 1;
        if (e.k == 'd') { bit &= 31; v = D[e.v]; size = 4; }
        else { bit &= 7; v = read(e, 1); }
        Z = b2u(((v >> bit) & 1) == 0);
        switch (t) { case 1: v ^= 1u << bit; break; case 2: v &= ~(1u << bit); break; case 3: v |= 1u << bit; break; }
        if (t != 0) write(e, size, v);
        return;
    }
    unsigned t = (op >> 9) & 7; int size;
    switch ((op >> 6) & 3) { case 0: size = 1; break; case 1: size = 2; break; case 2: size = 4; break; default: halt("movep/unknown", op); }
    if (op == 0x003C || op == 0x023C || op == 0x0A3C) {
        uint32_t imm = f16() & 0xFF, cc = ccr();
        if (op == 0x003C) cc |= imm; else if (op == 0x023C) cc &= imm; else cc ^= imm;
        setCcr(cc);
        return;
    }
    uint32_t imm = size == 4 ? f32() : (f16() & kSzMask[size]);
    Op e = ea(mode, reg, size);
    uint32_t v = read(e, size), r;
    switch (t) {
    case 0: r = v | imm; logicFlags(r, size); break;
    case 1: r = v & imm; logicFlags(r, size); break;
    case 2: r = doSub(imm, v, size, 0, false, true); break;
    case 3: r = doAdd(imm, v, size, 0, false); break;
    case 5: r = v ^ imm; logicFlags(r, size); break;
    case 6: doSub(imm, v, size, 0, false, false); return;
    default: halt("imm op", op);
    }
    write(e, size, r);
}

void M68k::move(uint16_t op, int size) {
    Op src = ea((op >> 3) & 7, op & 7, size);
    uint32_t v = read(src, size);
    unsigned dm = (op >> 6) & 7, dr = (op >> 9) & 7;
    if (dm == 1) { A[dr] = sx(v, size); return; }
    Op dst = ea(dm, dr, size);
    write(dst, size, v);
    logicFlags(v, size);
}

void M68k::g4(uint16_t op) {
    unsigned mode = (op >> 3) & 7, reg = op & 7;
    if (op == 0x4E71) return;
    if (op == 0x4E75) { PC = pop(); return; }
    if (op == 0x4E77) { setCcr(pop() & 0xFF); PC = pop(); return; }
    if ((op & 0xFFF8) == 0x4E50) { push(A[reg]); A[reg] = A[7]; A[7] += sx(f16(), 2); return; }
    if ((op & 0xFFF8) == 0x4E58) { A[7] = A[reg]; A[reg] = pop(); return; }
    if ((op & 0xFFC0) == 0x4E80) { Op e = ea(mode, reg, 4); call(e.v, PC); return; }
    if ((op & 0xFFC0) == 0x4EC0) { PC = ea(mode, reg, 4).v; return; }
    if ((op & 0xFFF8) == 0x4840) { uint32_t v = D[reg]; v = v << 16 | v >> 16; D[reg] = v; logicFlags(v, 4); return; }
    if ((op & 0xFFC0) == 0x4840) { push(ea(mode, reg, 4).v); return; }
    if ((op & 0xFFB8) == 0x4880) {
        if (op & 0x40) { uint32_t v = sx(D[reg], 2); D[reg] = v; logicFlags(v, 4); }
        else { uint32_t v = sx(D[reg], 1) & 0xFFFF; D[reg] = (D[reg] & 0xFFFF0000) | v; logicFlags(v, 2); }
        return;
    }
    if ((op & 0xFB80) == 0x4880) { movem(op, mode, reg); return; }
    if ((op & 0xF1C0) == 0x41C0) { A[(op >> 9) & 7] = ea(mode, reg, 4).v; return; }
    if ((op & 0xFFC0) == 0x40C0) { write(ea(mode, reg, 2), 2, ccr()); return; }
    if ((op & 0xFFC0) == 0x44C0) { setCcr(read(ea(mode, reg, 2), 2) & 0xFF); return; }
    if ((op & 0xFFC0) == 0x46C0) { setCcr(read(ea(mode, reg, 2), 2) & 0xFF); return; }
    unsigned t = op & 0xFF00;
    if (t == 0x4200 || t == 0x4400 || t == 0x4600 || t == 0x4000 || t == 0x4A00) {
        int size;
        switch ((op >> 6) & 3) {
        case 0: size = 1; break; case 1: size = 2; break; case 2: size = 4; break;
        default:
            if (t == 0x4A00) { Op e = ea(mode, reg, 1); uint32_t v = read(e, 1); logicFlags(v, 1); write(e, 1, v | 0x80); return; }
            halt("op", op);
        }
        Op e = ea(mode, reg, size);
        switch (t) {
        case 0x4A00: logicFlags(read(e, size), size); break;
        case 0x4200: write(e, size, 0); N = 0; Z = 1; V = 0; C = 0; break;
        case 0x4600: { uint32_t r = ~read(e, size) & kSzMask[size]; write(e, size, r); logicFlags(r, size); break; }
        case 0x4400: write(e, size, doSub(read(e, size), 0, size, 0, false, true)); break;
        default: write(e, size, doSub(read(e, size), 0, size, X, true, true));
        }
        return;
    }
    halt("unknown opcode", op, PC - 2);
}

void M68k::movem(uint16_t op, unsigned mode, unsigned reg) {
    int size = (op & 0x40) ? 4 : 2;
    uint32_t mask = f16();
    bool toRegs = (op & 0x400) != 0;
    uint32_t step = size;
    if (!toRegs) {
        if (mode == 4) {
            uint32_t ad = A[reg];
            for (int i = 0; i < 16; i++) if (mask & (1u << i)) { ad -= step; wr(ad, size, reg16(15 - i)); }
            A[reg] = ad;
        } else {
            uint32_t ad = ea(mode, reg, size).v;
            for (int i = 0; i < 16; i++) if (mask & (1u << i)) { wr(ad, size, reg16(i)); ad += step; }
        }
        return;
    }
    uint32_t ad = mode == 3 ? A[reg] : ea(mode, reg, size).v;
    for (int i = 0; i < 16; i++) if (mask & (1u << i)) {
        uint32_t v = rd(ad, size);
        if (size == 2) v = sx(v, 2);
        if (i < 8) D[i] = v; else A[i - 8] = v;
        ad += step;
    }
    if (mode == 3) A[reg] = ad;
}

void M68k::g5(uint16_t op) {
    unsigned mode = (op >> 3) & 7, reg = op & 7, s = (op >> 6) & 3;
    if (s == 3) {
        unsigned cc = (op >> 8) & 0xF;
        if (mode == 1) {
            uint32_t base = PC, disp = sx(f16(), 2);
            if (!cond(cc)) {
                uint32_t n = (D[reg] - 1) & 0xFFFF;
                D[reg] = (D[reg] & 0xFFFF0000) | n;
                if (n != 0xFFFF) PC = base + disp;
            }
            return;
        }
        Op e = ea(mode, reg, 1);
        write(e, 1, cond(cc) ? 0xFF : 0);
        return;
    }
    static const int sizes[3] = {1, 2, 4};
    int size = sizes[s];
    uint32_t q = (op >> 9) & 7; if (q == 0) q = 8;
    Op e = ea(mode, reg, size);
    if (e.k == 'a') { if (op & 0x100) A[e.v] -= q; else A[e.v] += q; return; }
    uint32_t v = read(e, size);
    uint32_t r = (op & 0x100) ? doSub(q, v, size, 0, false, true) : doAdd(q, v, size, 0, false);
    write(e, size, r);
}

void M68k::g6(uint16_t op) {
    unsigned cc = (op >> 8) & 0xF;
    uint32_t base = PC, d = op & 0xFF, disp;
    if (d == 0) disp = sx(f16(), 2);
    else if (d == 0xFF) disp = f32();
    else disp = sx(d, 1);
    if (cc == 1) call(base + disp, PC);
    else if (cond(cc)) PC = base + disp;
}

void M68k::aluDir(uint16_t op, const std::function<uint32_t(uint32_t, uint32_t)>& fn, int size, unsigned mode, unsigned reg) {
    unsigned dn = (op >> 9) & 7, om = (op >> 6) & 7;
    Op e = ea(mode, reg, size);
    uint32_t r = fn(read(e, size), D[dn] & kSzMask[size]);
    logicFlags(r, size);
    if (om < 4) write({'d', dn}, size, r); else write(e, size, r);
}

void M68k::g8(uint16_t op) {
    unsigned mode = (op >> 3) & 7, reg = op & 7, om = (op >> 6) & 7, dn = (op >> 9) & 7;
    if (om == 3 || om == 7) {
        uint32_t src = read(ea(mode, reg, 2), 2);
        if (src == 0) halt("division by zero", PC);
        uint32_t q, r;
        if (om == 3) {
            uint32_t n = D[dn]; q = n / src; r = n % src;
            if (q > 0xFFFF) { V = 1; return; }
        } else {
            int32_t n = int32_t(D[dn]), dv = int32_t(int16_t(src));
            int64_t qq = int64_t(n) / dv; int64_t rr = int64_t(n) - qq * dv;
            if (qq < -32768 || qq > 32767) { V = 1; return; }
            q = uint32_t(qq); r = uint32_t(rr);
        }
        D[dn] = (r & 0xFFFF) << 16 | (q & 0xFFFF);
        setnz(q, 2); V = 0; C = 0;
        return;
    }
    if ((op & 0x1F0) == 0x100) halt("SBCD", PC);
    aluDir(op, [](uint32_t a, uint32_t b) { return a | b; }, kOpSize[om], mode, reg);
}

void M68k::addsub(uint16_t op, bool isAdd) {
    unsigned mode = (op >> 3) & 7, reg = op & 7, om = (op >> 6) & 7, dn = (op >> 9) & 7;
    if (om == 3 || om == 7) {
        int size = om == 7 ? 4 : 2;
        uint32_t v = read(ea(mode, reg, size), size);
        if (size == 2) v = sx(v, 2);
        if (isAdd) A[dn] += v; else A[dn] -= v;
        return;
    }
    int size = kOpSize[om];
    if (om >= 4 && (mode == 0 || mode == 1)) {
        Op so, dop;
        if (mode == 0) { so = {'d', reg}; dop = {'d', dn}; }
        else { A[reg] -= size; A[dn] -= size; so = {'m', A[reg]}; dop = {'m', A[dn]}; }
        uint32_t s = read(so, size), d = read(dop, size);
        uint32_t r = isAdd ? doAdd(s, d, size, X, true) : doSub(s, d, size, X, true, true);
        write(dop, size, r);
        return;
    }
    Op e = ea(mode, reg, size);
    uint32_t s, d;
    if (om < 4) { s = read(e, size); d = D[dn] & kSzMask[size]; }
    else { s = D[dn] & kSzMask[size]; d = read(e, size); }
    uint32_t r = isAdd ? doAdd(s, d, size, 0, false) : doSub(s, d, size, 0, false, true);
    if (om < 4) write({'d', dn}, size, r); else write(e, size, r);
}

void M68k::gb(uint16_t op) {
    unsigned mode = (op >> 3) & 7, reg = op & 7, om = (op >> 6) & 7, dn = (op >> 9) & 7;
    if (om == 3 || om == 7) {
        int size = om == 7 ? 4 : 2;
        uint32_t v = read(ea(mode, reg, size), size);
        if (size == 2) v = sx(v, 2);
        doSub(v, A[dn], 4, 0, false, false);
        return;
    }
    int size = kOpSize[om];
    if (om < 4) {
        Op e = ea(mode, reg, size);
        doSub(read(e, size), D[dn] & kSzMask[size], size, 0, false, false);
    } else if (mode == 1) {
        Op s{'m', A[reg]}; A[reg] += size;
        Op d{'m', A[dn]}; A[dn] += size;
        uint32_t sv = read(s, size), dv = read(d, size);
        doSub(sv, dv, size, 0, false, false);
    } else {
        aluDir(op, [](uint32_t a, uint32_t b) { return a ^ b; }, size, mode, reg);
    }
}

void M68k::gc(uint16_t op) {
    unsigned mode = (op >> 3) & 7, reg = op & 7, om = (op >> 6) & 7, dn = (op >> 9) & 7;
    if (om == 3) { uint32_t v = read(ea(mode, reg, 2), 2) * (D[dn] & 0xFFFF); D[dn] = v; logicFlags(v, 4); return; }
    if (om == 7) {
        int32_t v = int32_t(int16_t(read(ea(mode, reg, 2), 2))) * int32_t(int16_t(D[dn]));
        D[dn] = uint32_t(v); logicFlags(uint32_t(v), 4); return;
    }
    if ((op & 0x1F8) == 0x140) { std::swap(D[dn], D[reg]); return; }
    if ((op & 0x1F8) == 0x148) { std::swap(A[dn], A[reg]); return; }
    if ((op & 0x1F8) == 0x188) { std::swap(D[dn], A[reg]); return; }
    if ((op & 0x1F0) == 0x100) halt("ABCD", PC);
    aluDir(op, [](uint32_t a, uint32_t b) { return a & b; }, kOpSize[om], mode, reg);
}

void M68k::ge(uint16_t op) {
    bool left = (op & 0x100) != 0;
    unsigned sz = (op >> 6) & 3;
    if (sz == 3) {
        unsigned t = (op >> 9) & 3;
        Op e = ea((op >> 3) & 7, op & 7, 2);
        write(e, 2, shift(t, left, read(e, 2), 1, 2));
        return;
    }
    static const int sizes[3] = {1, 2, 4};
    int size = sizes[sz];
    unsigned t = (op >> 3) & 3;
    uint32_t cnt;
    if (op & 0x20) cnt = D[(op >> 9) & 7] & 63;
    else { cnt = (op >> 9) & 7; if (cnt == 0) cnt = 8; }
    unsigned reg = op & 7;
    uint32_t r = shift(t, left, D[reg] & kSzMask[size], cnt, size);
    write({'d', reg}, size, r);
}

uint32_t M68k::shift(unsigned t, bool left, uint32_t v, uint32_t cnt, int size) {
    unsigned bits = kSzBits[size];
    uint32_t m = kSzMask[size];
    v &= m;
    uint32_t ov = 0, cc = 0;
    if (cnt == 0) {
        C = t == 2 ? X : 0;
        V = 0;
        setnz(v, size);
        return v;
    }
    for (uint32_t i = 0; i < cnt; i++) {
        switch (t) {
        case 0: case 1:
            if (left) {
                cc = (v >> (bits - 1)) & 1;
                uint32_t nv = (v << 1) & m;
                if (t == 0 && (((nv ^ v) >> (bits - 1)) & 1)) ov = 1;
                v = nv;
            } else {
                cc = v & 1;
                if (t == 0) v = (v >> 1) | (v & (1u << (bits - 1)));
                else v >>= 1;
            }
            X = cc;
            break;
        case 2:
            if (left) { cc = (v >> (bits - 1)) & 1; v = ((v << 1) | X) & m; }
            else { cc = v & 1; v = (v >> 1) | (X << (bits - 1)); }
            X = cc;
            break;
        default:
            if (left) { cc = (v >> (bits - 1)) & 1; v = ((v << 1) | cc) & m; }
            else { cc = v & 1; v = (v >> 1) | (cc << (bits - 1)); }
        }
    }
    C = cc; V = ov;
    setnz(v, size);
    return v;
}

}  // namespace tg
