// dis68k: MC68000 disassembler (Motorola syntax) for analysing the original game code.
//   dis68k <image> <start> <end> [base]   image = raw memory/hunk dump, addresses in hex; base = guest address of image byte 0 (default 0)
//   dis68k -x <image> <word-hex> ...       decode single words only for testing
// Output: address, raw words, instruction. Branch / PC-relative targets are printed as absolute addresses.
// Unknown opcodes are printed as dc.w. Line-A / line-F are shown as such.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

struct Dis {
    const std::vector<uint8_t>& img;
    uint32_t base;
    uint32_t pc = 0, start = 0;
    mutable bool ok = true;
    Dis(const std::vector<uint8_t>& i, uint32_t b) : img(i), base(b) {}

    uint8_t byteAt(uint32_t a) const { return a >= base && a - base < img.size() ? img[a - base] : (ok = false, uint8_t(0)); }
    uint16_t word(uint32_t a) const { return uint16_t(byteAt(a) << 8 | byteAt(a + 1)); }
    uint16_t next16() { uint16_t v = word(pc); pc += 2; return v; }
    uint32_t next32() { uint32_t hi = next16(); return hi << 16 | next16(); }

    static std::string hex(uint32_t v, bool dollar = true) { char b[16]; std::snprintf(b, sizeof b, dollar ? "$%x" : "%x", v); return b; }
    static std::string shex(int32_t v) { return v < 0 ? "-$" + hex(uint32_t(-int64_t(v)), false) : "$" + hex(uint32_t(v), false); }
    static std::string dn(int r) { return "d" + std::to_string(r); }
    static std::string an(int r) { return r == 7 ? "sp" : "a" + std::to_string(r); }

    // size: 0 byte, 1 word, 2 long
    std::string imm(int size) {
        if (size == 2) return "#" + hex(next32());
        uint16_t v = next16();
        return "#" + hex(size == 0 ? v & 0xFF : v);
    }

    std::string index(const char* baseName, uint32_t extPc, bool pcRel, uint32_t pcBase) {
        uint16_t e = word(extPc);
        std::string r = (e & 0x8000) ? an((e >> 12) & 7) : dn((e >> 12) & 7);
        r += (e & 0x800) ? ".l" : ".w";
        int8_t d = int8_t(e & 0xFF);
        if (pcRel) return shex(int32_t(pcBase) + d) + "(pc," + r + ")";
        return (d ? shex(d) : std::string("0")) + "(" + baseName + "," + r + ")";
    }

    std::string ea(int mode, int reg, int size) {
        switch (mode) {
        case 0: return dn(reg);
        case 1: return an(reg);
        case 2: return "(" + an(reg) + ")";
        case 3: return "(" + an(reg) + ")+";
        case 4: return "-(" + an(reg) + ")";
        case 5: { int16_t d = int16_t(next16()); return shex(d) + "(" + an(reg) + ")"; }
        case 6: { uint32_t p = pc; pc += 2; return index(an(reg).c_str(), p, false, 0); }
        default:
            switch (reg) {
            case 0: return hex(uint32_t(int32_t(int16_t(next16())))) + ".w";
            case 1: return hex(next32()) + ".l";
            case 2: { uint32_t p = pc; int16_t d = int16_t(next16()); return hex(p + d) + "(pc)"; }
            case 3: { uint32_t p = pc; pc += 2; return index("", p, true, p); }
            case 4: return imm(size);
            }
        }
        ok = false; return "?";
    }

    static const char* cc(int c) {
        static const char* n[16] = {"t", "f", "hi", "ls", "cc", "cs", "ne", "eq", "vc", "vs", "pl", "mi", "ge", "lt", "gt", "le"};
        return n[c];
    }
    static const char* sz(int s) { return s == 0 ? ".b" : s == 1 ? ".w" : ".l"; }

    std::string movemList(uint16_t mask, bool predec) {
        std::string out;
        for (int grp = 0; grp < 2; grp++) {
            int i = 0;
            while (i < 8) {
                auto bit = [&](int k) { int idx = grp * 8 + k; return predec ? ((mask >> (15 - idx)) & 1) : ((mask >> idx) & 1); };
                if (!bit(i)) { i++; continue; }
                int j = i; while (j + 1 < 8 && bit(j + 1)) j++;
                if (!out.empty()) out += "/";
                std::string p = grp ? "a" : "d";
                out += p + std::to_string(i); if (j > i) out += "-" + p + std::to_string(j);
                i = j + 1;
            }
        }
        return out.empty() ? "<none>" : out;
    }

    std::string decode() {
        uint16_t op = next16();
        int hi = op >> 12, reg = (op >> 9) & 7, mode = (op >> 3) & 7, r = op & 7, szf = (op >> 6) & 3;
        auto mn = [](const char* m, int s) { return std::string(m) + sz(s); };
        switch (hi) {
        case 0: {
            if ((op & 0x138) == 0x108) {   // MOVEP
                int16_t d = int16_t(next16()); std::string m = shex(d) + "(" + an(r) + ")";
                bool toMem = op & 0x80; std::string s = (op & 0x40) ? ".l" : ".w";
                return "movep" + s + " " + (toMem ? dn(reg) + "," + m : m + "," + dn(reg));
            }
            if (op & 0x100) {
                static const char* b[4] = {"btst", "bchg", "bclr", "bset"};
                std::string d = dn(reg); return std::string(b[szf]) + " " + d + "," + ea(mode, r, 0);
            }
            static const char* names[8] = {"ori", "andi", "subi", "addi", nullptr, "eori", "cmpi", nullptr};
            int k = reg;
            if (k == 4) {
                static const char* b[4] = {"btst", "bchg", "bclr", "bset"};
                uint16_t n = next16(); return std::string(b[szf]) + " #" + std::to_string(n & 0xFF) + "," + ea(mode, r, 0);
            }
            if (!names[k] || szf == 3) break;
            if ((k == 0 || k == 1 || k == 5) && mode == 7 && r == 4) {
                uint16_t v = next16(); return std::string(names[k]) + " #" + hex(v & (szf == 0 ? 0xFF : 0xFFFF)) + "," + (szf == 0 ? "ccr" : "sr");
            }
            std::string i = imm(szf); return mn(names[k], szf) + " " + i + "," + ea(mode, r, szf);
        }
        case 1: case 2: case 3: {
            int s = hi == 1 ? 0 : hi == 3 ? 1 : 2, dm = (op >> 6) & 7;
            std::string src = ea(mode, r, s);
            if (dm == 1) return std::string("movea") + sz(s) + " " + src + "," + an(reg);
            return mn("move", s) + " " + src + "," + ea(dm, reg, s);
        }
        case 4: {
            if (op == 0x4E70) return "reset";
            if (op == 0x4E71) return "nop";
            if (op == 0x4E72) return "stop #" + hex(next16());
            if (op == 0x4E73) return "rte";
            if (op == 0x4E75) return "rts";
            if (op == 0x4E76) return "trapv";
            if (op == 0x4E77) return "rtr";
            if (op == 0x4AFC) return "illegal";
            if ((op & 0xFFF0) == 0x4E40) return "trap #" + std::to_string(op & 15);
            if ((op & 0xFFF8) == 0x4E50) { int16_t d = int16_t(next16()); return "link " + an(r) + ",#" + shex(d); }
            if ((op & 0xFFF8) == 0x4E58) return "unlk " + an(r);
            if ((op & 0xFFF8) == 0x4E60) return "move " + an(r) + ",usp";
            if ((op & 0xFFF8) == 0x4E68) return "move usp," + an(r);
            if ((op & 0xFFC0) == 0x4E80) return "jsr " + ea(mode, r, 2);
            if ((op & 0xFFC0) == 0x4EC0) return "jmp " + ea(mode, r, 2);
            if ((op & 0xFB80) == 0x4880 && mode != 0) {   // MOVEM
                bool toReg = op & 0x400; int s = (op & 0x40) ? 2 : 1; uint16_t mask = next16();
                std::string list = movemList(mask, !toReg && mode == 4), e = ea(mode, r, s);
                return mn("movem", s) + " " + (toReg ? e + "," + list : list + "," + e);
            }
            if ((op & 0xFFF8) == 0x4880) return "ext.w " + dn(r);
            if ((op & 0xFFF8) == 0x48C0) return "ext.l " + dn(r);
            if ((op & 0xFFF8) == 0x4840) return "swap " + dn(r);
            if ((op & 0xFFC0) == 0x4840) return "pea " + ea(mode, r, 2);
            if ((op & 0xFFC0) == 0x4800) return "nbcd " + ea(mode, r, 0);
            if ((op & 0xF1C0) == 0x41C0) return "lea " + ea(mode, r, 2) + "," + an(reg);
            if ((op & 0xF1C0) == 0x4180) return "chk.w " + ea(mode, r, 1) + "," + dn(reg);
            if ((op & 0xFFC0) == 0x40C0) return "move sr," + ea(mode, r, 1);
            if ((op & 0xFFC0) == 0x44C0) return "move " + ea(mode, r, 1) + ",ccr";
            if ((op & 0xFFC0) == 0x46C0) return "move " + ea(mode, r, 1) + ",sr";
            if ((op & 0xFFC0) == 0x4AC0) return "tas " + ea(mode, r, 0);
            static const char* n[4] = {"negx", "clr", "neg", "not"};
            int k = (op >> 9) & 7;
            if (szf != 3 && (k == 0 || k == 1 || k == 2 || k == 3)) return mn(n[k], szf) + " " + ea(mode, r, szf);
            if (szf != 3 && k == 5) return mn("tst", szf) + " " + ea(mode, r, szf);
            break;
        }
        case 5: {
            if (szf == 3) {
                if (mode == 1) { int16_t d = int16_t(next16()); uint32_t t = pc - 2 + d; return std::string("db") + cc((op >> 8) & 15) + " " + dn(r) + "," + hex(t); }
                return std::string("s") + cc((op >> 8) & 15) + " " + ea(mode, r, 0);
            }
            int q = reg ? reg : 8;
            return mn(op & 0x100 ? "subq" : "addq", szf) + " #" + std::to_string(q) + "," + ea(mode, r, szf);
        }
        case 6: {
            int c = (op >> 8) & 15; int8_t d8 = int8_t(op & 0xFF); uint32_t t0 = pc; uint32_t t;
            std::string s = ".s";
            if (d8 == 0) { t = t0 + int16_t(next16()); s = ".w"; } else t = t0 + d8;
            std::string m = c == 0 ? "bra" : c == 1 ? "bsr" : std::string("b") + cc(c);
            return m + s + " " + hex(t);
        }
        case 7:
            if (op & 0x100) break;
            return "moveq #" + shex(int8_t(op & 0xFF)) + "," + dn(reg);
        case 8: case 12: {
            bool isOr = hi == 8;
            if (szf == 3) return std::string(isOr ? ((op & 0x100) ? "divs.w " : "divu.w ") : ((op & 0x100) ? "muls.w " : "mulu.w ")) + ea(mode, r, 1) + "," + dn(reg);
            if ((op & 0x1F0) == 0x100) return std::string(isOr ? "sbcd " : "abcd ") + (op & 8 ? "-(" + an(r) + "),-(" + an(reg) + ")" : dn(r) + "," + dn(reg));
            if (!isOr && (op & 0x130) == 0x100) {
                int m = (op >> 3) & 0x1F;
                if (m == 0x08) return "exg " + dn(reg) + "," + dn(r);
                if (m == 0x09) return "exg " + an(reg) + "," + an(r);
                if (m == 0x11) return "exg " + dn(reg) + "," + an(r);
            }
            std::string m = isOr ? "or" : "and";
            if (op & 0x100) return mn(m.c_str(), szf) + " " + dn(reg) + "," + ea(mode, r, szf);
            return mn(m.c_str(), szf) + " " + ea(mode, r, szf) + "," + dn(reg);
        }
        case 9: case 13: {
            bool add = hi == 13; const char* m = add ? "add" : "sub";
            if (szf == 3) { int s = (op & 0x100) ? 2 : 1; return std::string(m) + "a" + sz(s) + " " + ea(mode, r, s) + "," + an(reg); }
            if ((op & 0x130) == 0x100) return std::string(m) + "x" + sz(szf) + " " + (op & 8 ? "-(" + an(r) + "),-(" + an(reg) + ")" : dn(r) + "," + dn(reg));
            if (op & 0x100) return mn(m, szf) + " " + dn(reg) + "," + ea(mode, r, szf);
            return mn(m, szf) + " " + ea(mode, r, szf) + "," + dn(reg);
        }
        case 11: {
            if (szf == 3) { int s = (op & 0x100) ? 2 : 1; return std::string("cmpa") + sz(s) + " " + ea(mode, r, s) + "," + an(reg); }
            if (op & 0x100) {
                if (mode == 1) return mn("cmpm", szf) + " (" + an(r) + ")+,(" + an(reg) + ")+";
                return mn("eor", szf) + " " + dn(reg) + "," + ea(mode, r, szf);
            }
            return mn("cmp", szf) + " " + ea(mode, r, szf) + "," + dn(reg);
        }
        case 14: {
            static const char* n[4] = {"as", "ls", "rox", "ro"};
            const char* d = (op & 0x100) ? "l" : "r";
            if (szf == 3) return std::string(n[(op >> 9) & 3]) + d + ".w " + ea(mode, r, 1);
            int k = (op >> 3) & 3; std::string cnt = (op & 0x20) ? dn(reg) : "#" + std::to_string(reg ? reg : 8);
            return std::string(n[k]) + d + sz(szf) + " " + cnt + "," + dn(r);
        }
        case 10: return "line-a #" + hex(op & 0xFFF);
        case 15: return "line-f #" + hex(op & 0xFFF);
        }
        ok = false;
        return "dc.w " + hex(op);
    }

    void line(uint32_t addr, std::string& out) {
        start = pc = addr; ok = true;
        std::string s = decode();
        char b[64]; std::snprintf(b, sizeof b, "%06x: ", addr); out = b;
        std::string raw;
        for (uint32_t a = start; a < pc && a < start + 10; a += 2) { std::snprintf(b, sizeof b, "%04x ", word(a)); raw += b; }
        raw.resize(25, ' ');
        out += raw + s;
        if (!ok && s.rfind("dc.w", 0) != 0) out += "   ; ??";
    }
};

}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) { std::fprintf(stderr, "usage: dis68k <image> <start-hex> <end-hex> [base-hex]\n"); return 2; }
    std::ifstream f(argv[1], std::ios::binary);
    if (!f) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    std::vector<uint8_t> img((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    uint32_t start = uint32_t(std::strtoul(argv[2], nullptr, 16)), end = uint32_t(std::strtoul(argv[3], nullptr, 16));
    uint32_t base = argc > 4 ? uint32_t(std::strtoul(argv[4], nullptr, 16)) : 0;
    Dis d(img, base);
    for (uint32_t a = start; a < end;) {
        std::string l; d.line(a, l); std::puts(l.c_str());
        a = d.pc > a ? d.pc : a + 2;
    }
    return 0;
}
