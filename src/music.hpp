// The original music: music1.img / music2.img are 68000 programs (Jochen Hippel's player + song + samples).
// They are run on the interpreter in m68k.cpp; their writes to Paula and to the CIA-A timer are caught and turned into audio.
// Entry +0 starts the song (installs a 50 Hz CIA interrupt), entry +4 stops it.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "m68k.hpp"

namespace tg {

class Music {
public:
    static constexpr int kRate = 44100;
    Music();
    bool load(const std::vector<uint8_t>& image, std::string& err);   // returns false on a bad image
    bool start(std::string& err);
    void stop();
    bool playing() const { return playing_; }
    const std::string& error() const { return error_; }
    void setGain(double g) { gain_ = g; }
    // Renders `frames` stereo frames (interleaved L,R int16), advancing the player's time.
    void render(int16_t* out, int frames);
    // Writes to Paula / the CIA timer seen so far ("addr val" in hex, one per line): for comparing with the reference port.
    std::string takeLog();
    void enableLog(bool on) { logOn_ = on; }

private:
    struct Chan { uint32_t lc = 0, ln = 0, per = 0, vol = 0; bool on = false; uint32_t cur = 0, rem = 0; double phase = 0; int8_t s = 0; };
    uint32_t ioHook(uint32_t addr, int size, uint32_t val, bool write);
    void paulaWrite(uint32_t addr, int size, uint32_t val);
    void reload(Chan& c);
    void ciaWrite(uint32_t addr, uint32_t val);
    void runInterrupt();
    bool call(uint32_t entry, std::string& err);
    static double periodUs(uint32_t latch);

    M68k cpu_;
    uint32_t base_ = 0;
    bool loaded_ = false, playing_ = false;
    Chan ch_[4];
    uint32_t dmacon_ = 0;
    struct Timer { uint32_t latch = 0; bool running = false, vec = false; double next = 0; uint32_t data = 0, code = 0; } timer_;
    double virtualUs() const { return nowUs_ + double(cpu_.steps - runStart_) * 1.7; }   // 1.7 us per instruction, like the reference port
    uint64_t runStart_ = 0;
    double nowUs_ = 0, lpL_ = 0, lpR_ = 0, gain_ = 1.0;
    bool logOn_ = false;
    std::string log_, error_;
};

}  // namespace tg
