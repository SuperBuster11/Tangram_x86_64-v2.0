// test_dat <game.adf> <tests/data>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include "../src/dat.hpp"
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
int main(int argc, char** argv) {
    const char* adfPath = argc >= 2 && argv[1][0] ? argv[1] : std::getenv("TANGRAM_ADF");
    std::string dir = argc >= 3 ? argv[2] : "tests/data", err;
    if (!adfPath) { std::printf("SKIP: set TANGRAM_ADF\n"); return 0; }
    tg::Adf adf;
    CHECK(adf.loadFile(adfPath, err));
    const tg::Bytes* dat = adf.file("TANGRAM.DAT");
    CHECK(dat != nullptr);
    if (!dat) return 1;
    std::vector<tg::DatPicture> pics;
    CHECK(tg::parseDat(*dat, pics, err));
    CHECK(pics.size() == 85);
    if (pics.size() == 85) {
        CHECK(pics[0].w == 61 && pics[0].h == 12);       // button label (NOVICE)
        CHECK(pics[4].w == 109 && pics[4].h == 138);     // menu panel
        CHECK(pics[5].w == 296 && pics[5].h == 8);       // font strip: A-Z 0-9 (c)
        CHECK(pics[7].w == 206 && pics[7].h == 61);      // "GAME PAUSED" panel
        CHECK(pics[33].w == 16 && pics[33].h == 16);
        // the font strip must equal what the running game holds in memory (rows 0-7, x 8..303 of its glyph bitmap)
        std::ifstream f(dir + "/font_strip_game.bin", std::ios::binary);
        std::vector<char> game((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        CHECK(game.size() == 296 * 8);
        int diff = 0;
        for (size_t i = 0; i < game.size() && i < pics[5].index.size(); i++) diff += pics[5].index[i] != uint8_t(game[i]);
        std::printf("font strip pixels differing from the game's memory: %d\n", diff);
        CHECK(diff == 0);
    }
    // robustness: truncated and corrupted input must fail cleanly
    tg::Bytes cut(dat->begin(), dat->begin() + 1000);
    CHECK(!tg::parseDat(cut, pics, err));
    tg::Bytes junk = {'x', '\n'};
    CHECK(!tg::parseDat(junk, pics, err));
    std::printf(failures ? "%d FAILURES\n" : "all DAT tests passed\n", failures);
    return failures ? 1 : 0;
}
