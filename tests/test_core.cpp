// Self-checking tests for the core library. Usage: test_core <disk.adf>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

#include "../src/adf.hpp"
#include "../src/image.hpp"

static int failures = 0;

// Temporary directory that exists on Windows and Linux.
static std::string tempDir() {
    for (const char* v : {"TEMP", "TMP", "TMPDIR"})
        if (const char* p = std::getenv(v)) if (*p) return p;
    return "/tmp";
}
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
            failures++;                                                      \
        }                                                                    \
    } while (0)

int main(int argc, char** argv) {
    const char* adfPath = argc >= 2 ? argv[1] : std::getenv("TANGRAM_ADF");
    if (!adfPath || !*adfPath) {
        std::printf("usage: test_core disk.adf   (or set TANGRAM_ADF)\n");
        return 2;
    }
    std::string err;
    tg::Adf adf;
    CHECK(adf.loadFile(adfPath, err));
    CHECK(adf.label() == "Tangram");

    // expected files and sizes (from the original disk)
    struct { const char* n; size_t sz; } want[] = {
        {"tancrunch.prg", 34344}, {"tangram.dat", 43498}, {"tangram.mlu", 62854},
        {"thalion.lo5", 40064},   {"titlepic.lo5", 40000}, {"highscorepic.lo5", 40000},
        {"music1.img", 31800},    {"music2.img", 30266},   {"score.dta", 516},
        {"s/startup-sequence", 108}, {"C/Run", 2568},      // case-insensitive path
    };
    for (auto& w : want) {
        const tg::Bytes* f = adf.file(w.n);
        CHECK(f != nullptr);
        if (f) CHECK(f->size() == w.sz);
    }
    CHECK(adf.file("nonexistent") == nullptr);
    CHECK(adf.names().size() == 19);

    // known contents
    const tg::Bytes* dat = adf.file("TANGRAM.DAT");
    if (dat) CHECK(std::string(dat->begin(), dat->begin() + 7) == "84\n486\n");
    const tg::Bytes* ss = adf.file("s/startup-sequence");
    if (ss) CHECK(ss->size() > 0);

    // palette decoding: 0x0F80 -> (255,136,0)
    uint8_t w[64] = {0x0F, 0x80};
    tg::Palette p = tg::readPalette(w);
    CHECK(p.c[0].r == 255 && p.c[0].g == 136 && p.c[0].b == 0);
    CHECK(p.c[1].r == 0 && p.c[1].g == 0 && p.c[1].b == 0);

    // picture decoding
    const tg::Bytes* mlu = adf.file("tangram.mlu");
    const tg::Bytes* title = adf.file("titlepic.lo5");
    tg::Image img;
    if (mlu && title) {
        CHECK(tg::decodeLo5(*title, tg::readPalette(mlu->data()), img, err));
        CHECK(img.w == 320 && img.h == 200 && img.index.size() == 64000);
        for (uint8_t v : img.index) CHECK(v < 32);
        // sand colour dominates the upper right corner region
        CHECK(img.rgb[5 * 320 + 310].r > 150);
        CHECK(tg::writePng(tempDir() + "/test_core_title.png", img, 1));
        CHECK(tg::writePng(tempDir() + "/test_core_title_x2.png", img, 2));
    }
    tg::Bytes shortData(100);
    CHECK(!tg::decodeLo5(shortData, tg::Palette{}, img, err));

    // error handling for bad images
    tg::Adf bad;
    CHECK(!bad.load(tg::Bytes(1000), err));
    tg::Bytes notDos(901120);
    CHECK(!bad.load(notDos, err));
    tg::Bytes noRoot(901120);
    noRoot[0] = 'D'; noRoot[1] = 'O'; noRoot[2] = 'S';
    CHECK(!bad.load(noRoot, err));
    CHECK(!bad.loadFile("/nonexistent/file.adf", err));

    // damaged image: a hash chain that points back to itself must terminate
    {
        std::ifstream f(adfPath, std::ios::binary);
        tg::Bytes d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        size_t root = d.size() / 512 / 2 * 512;
        for (int i = 0; i < 72; i++) {
            uint32_t c = uint32_t(d[root + 24 + 4 * i]) << 24 | d[root + 25 + 4 * i] << 16 | d[root + 26 + 4 * i] << 8 | d[root + 27 + 4 * i];
            if (c) {  // make the entry's "next in chain" point to itself
                size_t e = size_t(c) * 512 + 496;
                d[e] = uint8_t(c >> 24); d[e + 1] = uint8_t(c >> 16); d[e + 2] = uint8_t(c >> 8); d[e + 3] = uint8_t(c);
            }
        }
        tg::Adf loop;
        CHECK(loop.load(d, err));  // returns (does not hang)
        CHECK(loop.names().size() > 0);
    }

    std::printf(failures ? "%d FAILURES\n" : "all tests passed\n", failures);
    return failures ? 1 : 0;
}
