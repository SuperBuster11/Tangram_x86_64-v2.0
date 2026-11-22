// test_assets <game.adf> <tests/data>: the board picture decoded from TANGRAM.MLU must match a screenshot of the
// running game at the start of level 1, except where the game draws on top of it (figure, counters, time bar).
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include "../src/assets.hpp"
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
int main(int argc, char** argv) {
    const char* adfPath = argc >= 2 && argv[1][0] ? argv[1] : std::getenv("TANGRAM_ADF");
    std::string dir = argc >= 3 ? argv[2] : "tests/data", err;
    if (!adfPath) { std::printf("SKIP: set TANGRAM_ADF\n"); return 0; }
    tg::Adf adf;
    CHECK(adf.loadFile(adfPath, err));
    const tg::Bytes* mlu = adf.file("TANGRAM.MLU");
    CHECK(mlu != nullptr);
    if (!mlu) return 1;
    tg::Image img;
    CHECK(tg::mluBoardPicture(*mlu, 0, img, err));
    CHECK(img.w == 320 && img.h == 200);
    std::ifstream f(dir + "/board_start.rgb", std::ios::binary);
    std::vector<char> shot((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    CHECK(shot.size() == 320 * 200 * 3);
    // compare only where the game draws nothing on top: outside the figure + shadow box (level 1) and outside the
    // lower right panel (counters, time bar, player name)
    int same = 0, total = 0;
    for (int y = 0; y < 200; y++)
        for (int x = 0; x < 320; x++) {
            bool figureBox = x >= 22 && x <= 212 && y >= 60 && y <= 146;
            bool panel = x >= 228 && y >= 100;
            if (figureBox || panel) continue;
            int i = y * 320 + x;
            total++;
            same += img.rgb[i].r == uint8_t(shot[i * 3]) && img.rgb[i].g == uint8_t(shot[i * 3 + 1]) && img.rgb[i].b == uint8_t(shot[i * 3 + 2]);
        }
    double pct = 100.0 * same / total;
    std::printf("pixels equal to the game's screen outside the overlays: %.2f%% of %d\n", pct, total);
    CHECK(pct > 99.0);
    tg::Palette p1, p2;
    CHECK(tg::mluBoardPalette(*mlu, 1, p1, err) && tg::mluBoardPalette(*mlu, 2, p2, err));
    CHECK(!tg::mluBoardPalette(*mlu, 3, p1, err));
    std::printf(failures ? "%d FAILURES\n" : "all asset tests passed\n", failures);
    return failures ? 1 : 0;
}
