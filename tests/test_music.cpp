// test_music <game.adf> <tests/data>: the 68000 music player must write the same Paula / CIA register sequence as the
// reference port (golden file from the Go engine's trace of the real game), and must run both songs without errors.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include "../src/adf.hpp"
#include "../src/music.hpp"
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
// "addr val" lines (hex) -> canonical tokens: LCH/LCL pairs become "lcN pointer-minus-image-base"
static std::vector<std::string> canon(const std::string& log, unsigned base) {
    std::vector<std::string> out; std::istringstream in(log); std::string a, v; unsigned pend[4] = {};
    while (in >> a >> v) {
        unsigned addr = unsigned(std::stoul(a, nullptr, 16)), val = unsigned(std::stoul(v, nullptr, 16));
        char buf[40];
        if (addr >= 0xDFF0A0 && addr < 0xDFF0E0 && (((addr - 0xDFF0A0) & 0xF) == 0 || ((addr - 0xDFF0A0) & 0xF) == 2)) {
            unsigned ch = (addr - 0xDFF0A0) >> 4;
            if (((addr - 0xDFF0A0) & 0xF) == 0) { pend[ch] = val; continue; }
            unsigned p = pend[ch] << 16 | val;
            std::snprintf(buf, sizeof buf, "lc%u %x", ch, p ? p - base : 0u);
        } else std::snprintf(buf, sizeof buf, "%06x %x", addr, val);
        out.push_back(buf);
    }
    return out;
}
int main(int argc, char** argv) {
    const char* adfPath = argc >= 2 && argv[1][0] ? argv[1] : std::getenv("TANGRAM_ADF");
    std::string dir = argc >= 3 ? argv[2] : "tests/data", err;
    if (!adfPath) { std::printf("SKIP: set TANGRAM_ADF\n"); return 0; }
    tg::Adf adf; CHECK(adf.loadFile(adfPath, err));
    for (int song = 1; song <= 2; song++) {
        const tg::Bytes* img = adf.file(song == 1 ? "MUSIC1.IMG" : "MUSIC2.IMG");
        CHECK(img != nullptr);
        if (!img) return 1;
        tg::Music m; m.enableLog(true);
        CHECK(m.load(*img, err) && m.start(err));
        std::vector<int16_t> pcm(size_t(tg::Music::kRate) * 2 * 6);
        m.render(pcm.data(), tg::Music::kRate * 6);
        CHECK(m.error().empty());
        long energy = 0; for (int16_t s : pcm) energy += std::abs(int(s));
        std::printf("song %d: 6 s rendered, mean |sample| %ld, error '%s'\n", song, energy / long(pcm.size()), m.error().c_str());
        CHECK(energy / long(pcm.size()) > 50);
        auto got = canon(m.takeLog(), 0x20000);
        if (song == 1) {
            std::ifstream f(dir + "/music1_writes_go.txt"); std::vector<std::string> want; std::string line;
            while (std::getline(f, line)) if (!line.empty()) want.push_back(line);
            size_t n = std::min(want.size(), got.size());
            std::printf("register writes: %zu golden, %zu produced, comparing %zu\n", want.size(), got.size(), n);
            CHECK(n >= 3000);
            size_t bad = 0; for (size_t i = 0; i < n; i++) bad += want[i] != got[i];
            CHECK(bad == 0);
        }
        // the same song twice gives the same samples (deterministic)
        tg::Music m2; CHECK(m2.load(*img, err) && m2.start(err));
        std::vector<int16_t> pcm2(pcm.size()); m2.render(pcm2.data(), tg::Music::kRate * 6);
        CHECK(pcm == pcm2);
    }
    std::printf(failures ? "FAILED\n" : "OK\n");
    return failures ? 1 : 0;
}
