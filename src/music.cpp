#include "music.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace tg {
namespace {
constexpr uint32_t kBase = 0x20000, kStackTop = 0x1F0000, kExecBase = 0x1000, kCiaBase = 0x2000;
constexpr uint32_t kStopAddr = 0xFFFFF0, kIrqReturn = 0xFFFFE0;
constexpr double kPaulaClock = 3546895.0, kCiaClock = 709379.0;
int16_t clip16(double v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : int16_t(v); }
}  // namespace

Music::Music() {
    cpu_.io = [this](uint32_t a, int s, uint32_t v, bool w) { return ioHook(a, s, v, w); };
}

bool Music::load(const std::vector<uint8_t>& image, std::string& err) {
    if (image.size() < 64 || kBase + image.size() >= kStackTop - 0x10000) { err = "music image has a bad size"; return false; }
    if (loaded_) std::fill(cpu_.mem.begin(), cpu_.mem.end(), 0);   // a fresh load restarts the song from a clean memory
    std::memcpy(cpu_.mem.data() + kBase, image.data(), image.size());
    base_ = kBase;
    cpu_.wr(4, 4, kExecBase);
    cpu_.libs.clear();
    // exec.library (only what the players call): Forbid/Permit/..., OpenResource
    cpu_.libs.push_back({kExecBase - 0x400, kExecBase, [this](M68k& c, uint32_t target) {
        uint32_t lvo = kExecBase - target;
        if (lvo == 498) {   // OpenResource(a1 name)
            std::string name;
            for (uint32_t p = c.A[1]; c.mem[p & 0xFFFFFF] && name.size() < 32; p++) name += char(c.mem[p & 0xFFFFFF]);
            c.D[0] = name == "ciaa.resource" ? kCiaBase : 0;
        } else if (lvo == 120 || lvo == 126 || lvo == 132 || lvo == 138) {
            // Disable / Enable / Forbid / Permit: nothing to do
        } else {
            char b[64]; std::snprintf(b, sizeof b, "unsupported exec call LVO -%u", lvo); error_ = b;
            throw M68kHalt(b);
        }
    }});
    // ciaa.resource: AddICRVector -6, RemICRVector -12, AbleICR -18, SetICR -24
    cpu_.libs.push_back({kCiaBase - 64, kCiaBase, [this](M68k& c, uint32_t target) {
        uint32_t lvo = kCiaBase - target;
        if (lvo == 6) {
            if (c.D[0] == 0) { timer_.data = c.rd(c.A[1] + 14, 4); timer_.code = c.rd(c.A[1] + 18, 4); timer_.vec = true; }
            c.D[0] = 0;
        } else if (lvo == 12) {
            if (c.D[0] == 0) timer_.vec = false;
        } else if (lvo == 18 || lvo == 24) {
            c.D[0] = 0;
        } else {
            char b[64]; std::snprintf(b, sizeof b, "unsupported cia call LVO -%u", lvo); throw M68kHalt(b);
        }
    }});
    loaded_ = true; playing_ = false;
    for (auto& c : ch_) c = Chan();
    dmacon_ = 0x200; timer_ = Timer();   // the OS leaves master DMA enable (DMAEN) set
     nowUs_ = 0; lpL_ = lpR_ = 0;
    return true;
}

bool Music::call(uint32_t entry, std::string& err) {
    uint32_t sd[8], sa[8], spc = cpu_.PC;
    std::memcpy(sd, cpu_.D, sizeof sd); std::memcpy(sa, cpu_.A, sizeof sa);
    uint32_t ccr = cpu_.ccr();
    std::fill(std::begin(cpu_.D), std::end(cpu_.D), 0);
    std::fill(std::begin(cpu_.A), std::end(cpu_.A), 0);
    cpu_.A[7] = kStackTop; cpu_.A[6] = kExecBase;
    cpu_.push(kStopAddr);
    cpu_.PC = entry;
    runStart_ = cpu_.steps;
    bool ok = cpu_.run(kStopAddr, cpu_.steps + 5000000, err);
    std::memcpy(cpu_.D, sd, sizeof sd); std::memcpy(cpu_.A, sa, sizeof sa);
    cpu_.PC = spc; cpu_.setCcr(ccr);
    return ok;
}

bool Music::start(std::string& err) {
    if (!loaded_) { err = "no music loaded"; return false; }
    if (!call(base_ + 0, err)) return false;
    playing_ = true;
    return true;
}

void Music::stop() {
    if (!loaded_ || !playing_) return;
    std::string err;
    call(base_ + 4, err);
    playing_ = false;
}

double Music::periodUs(uint32_t latch) {
    if (latch == 0) latch = 65536;
    return double(uint64_t(double(latch + 1) * 1e6 / kCiaClock));
}

void Music::reload(Chan& c) {
    c.cur = c.lc;
    uint32_t n = c.ln ? c.ln : 65536;
    c.rem = n * 2;
}

uint32_t Music::ioHook(uint32_t addr, int size, uint32_t val, bool write) {
    if (!write) {   // beam position derived from the virtual clock (PAL: 312 lines of 64 us per 20 ms frame); everything else reads 0
        uint64_t us = uint64_t(virtualUs()) % 20000;
        uint32_t v = uint32_t(us / 64), h = uint32_t((us % 64) * 227 / 64 / 2), w = 0;
        bool ok = true;
        uint32_t reg = addr & ~1u;
        if (reg == 0xDFF004) w = 0x8000 | ((v >> 8) & 1);
        else if (reg == 0xDFF006) w = ((v & 0xFF) << 8) | (h & 0xFF);
        else ok = false;
        if (addr == 0xBFE001) return 0xFF;
        if (!ok) return 0;
        if (size == 1) return (addr & 1) == 0 ? w >> 8 : (w & 0xFF);
        if (size == 2) return w;
        return w << 16;
    }
    if (size == 4) { ioHook(addr, 2, val >> 16, true); ioHook(addr + 2, 2, val & 0xFFFF, true); return 0; }
    if (logOn_ && ((addr >= 0xDFF0A0 && addr < 0xDFF0E0) || addr == 0xDFF096 || addr == 0xBFE401 || addr == 0xBFE501 || addr == 0xBFEE01)) {
        char b[48]; std::snprintf(b, sizeof b, "%06x %x\n", addr, val); log_ += b;
    }
    if (addr >= 0xDFF000) paulaWrite(addr, size, val); else ciaWrite(addr, val);
    return 0;
}

std::string Music::takeLog() { std::string s; s.swap(log_); return s; }

void Music::paulaWrite(uint32_t addr, int, uint32_t val) {
    if (addr == 0xDFF096) {   // DMACON
        uint32_t old = dmacon_;
        if (val & 0x8000) dmacon_ |= val & 0x7FFF; else dmacon_ &= ~(val & 0x7FFF);
        for (int i = 0; i < 4; i++) {
            bool was = (old & 0x200) && (old & (1u << i));
            bool now = (dmacon_ & 0x200) && (dmacon_ & (1u << i));
            if (now && !was) { ch_[i].on = true; reload(ch_[i]); ch_[i].phase = 1; }
            else if (!now && was) ch_[i].on = false;
        }
    } else if (addr >= 0xDFF0A0 && addr < 0xDFF0E0) {
        Chan& c = ch_[(addr - 0xDFF0A0) >> 4];
        switch ((addr - 0xDFF0A0) & 0xF) {
        case 0: c.lc = (c.lc & 0xFFFF) | ((val << 16) & 0x1F0000); break;
        case 2: c.lc = (c.lc & 0x1F0000) | (val & 0xFFFE); break;
        case 4: c.ln = val & 0xFFFF; break;
        case 6: c.per = val & 0xFFFF; break;
        case 8: c.vol = val & 0x7F; break;
        }
    }
}

void Music::ciaWrite(uint32_t addr, uint32_t val) {
    Timer& t = timer_;
    switch (addr) {
    case 0xBFE401: t.latch = (t.latch & 0xFF00) | (val & 0xFF); break;
    case 0xBFE501:
        t.latch = (t.latch & 0xFF) | ((val & 0xFF) << 8);
        if (t.running) t.next = virtualUs() + periodUs(t.latch);
        break;
    case 0xBFEE01:   // CRA
        if ((val & 1) && !t.running) { t.running = true; t.next = virtualUs() + periodUs(t.latch); }
        else if (!(val & 1)) t.running = false;
        if (val & 0x10) t.next = virtualUs() + periodUs(t.latch);
        break;
    }
}

void Music::runInterrupt() {
    uint32_t sd[8], sa[8], spc = cpu_.PC;
    std::memcpy(sd, cpu_.D, sizeof sd); std::memcpy(sa, cpu_.A, sizeof sa);
    uint32_t ccr = cpu_.ccr();
    cpu_.A[7] -= 64;
    std::fill(std::begin(cpu_.D), std::end(cpu_.D), 0);
    cpu_.A[0] = 0xDFF000; cpu_.A[1] = timer_.data; cpu_.A[5] = timer_.code; cpu_.A[6] = kExecBase;
    cpu_.push(kIrqReturn);
    cpu_.PC = timer_.code;
    runStart_ = cpu_.steps;
    std::string err;
    if (!cpu_.run(kIrqReturn, cpu_.steps + 2000000, err)) { error_ = err; timer_.vec = false; }
    std::memcpy(cpu_.D, sd, sizeof sd); std::memcpy(cpu_.A, sa, sizeof sa);
    cpu_.PC = spc; cpu_.setCcr(ccr);
}

void Music::render(int16_t* out, int frames) {
    const double stepUs = 1e6 / kRate;
    for (int k = 0; k < frames; k++) {
        if (playing_ && timer_.running && timer_.vec) {
            for (int n = 0; n < 4 && timer_.next <= nowUs_; n++) {
                timer_.next += periodUs(timer_.latch);
                runInterrupt();
            }
            if (timer_.next + 100000 < nowUs_) timer_.next = nowUs_;
        }
        double l = 0, r = 0;
        for (int i = 0; i < 4; i++) {
            Chan& c = ch_[i];
            if (!c.on) continue;
            uint32_t per = c.per < 124 ? 124 : c.per;
            c.phase += kPaulaClock / double(per) / kRate;
            while (c.phase >= 1) {
                c.phase--;
                if (c.rem == 0) reload(c);
                c.s = int8_t(cpu_.mem[c.cur & (M68k::kMemSize - 1)]);
                c.cur++; c.rem--;
            }
            uint32_t v = c.vol > 64 ? 64 : c.vol;
            double x = double(c.s) * double(v) * 1.5 * gain_;
            if (i == 0 || i == 3) l += x; else r += x;
        }
        lpL_ += (l - lpL_) * 0.5;
        lpR_ += (r - lpR_) * 0.5;
        out[2 * k] = clip16(lpL_); out[2 * k + 1] = clip16(lpR_);
        nowUs_ += stepUs;
    }
}

}  // namespace tg
