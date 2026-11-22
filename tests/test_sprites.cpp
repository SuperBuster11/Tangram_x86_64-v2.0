// test_sprites <game.adf> <tests/data>: pictures of TANGRAM.DAT drawn at the positions listed in layout.hpp must be
// pixel-identical to screenshots of the running game (menu screen with the title palette, play screen with palette 0).
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include "../src/assets.hpp"
#include "../src/layout.hpp"
#include "../src/sprite.hpp"
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
static std::vector<char> slurp(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<char>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
// counts non-transparent pixels of `pic` placed at (x, y) that equal the screenshot
static void compare(const tg::DatPicture& pic, int x, int y, const tg::Palette& pal, const std::vector<char>& shot, int& total, int& same) {
    for (int gy = 0; gy < pic.h; gy++)
        for (int gx = 0; gx < pic.w; gx++) {
            uint8_t v = pic.index[size_t(gy) * pic.w + gx];
            if (!v) continue;
            size_t i = (size_t(y + gy) * 320 + (x + gx)) * 3;
            total++;
            same += tg::Rgb{uint8_t(shot[i]), uint8_t(shot[i + 1]), uint8_t(shot[i + 2])} == pal.c[v & 31];
        }
}
int main(int argc, char** argv) {
    const char* adfPath = argc >= 2 && argv[1][0] ? argv[1] : std::getenv("TANGRAM_ADF");
    std::string dir = argc >= 3 ? argv[2] : "tests/data", err;
    if (!adfPath) { std::printf("SKIP: set TANGRAM_ADF\n"); return 0; }
    tg::Adf adf;
    CHECK(adf.loadFile(adfPath, err));
    const tg::Bytes* dat = adf.file("TANGRAM.DAT");
    const tg::Bytes* mlu = adf.file("TANGRAM.MLU");
    if (!dat || !mlu) return 1;
    std::vector<tg::DatPicture> pics;
    tg::Palette board;
    CHECK(tg::parseDat(*dat, pics, err) && tg::mluBoardPalette(*mlu, 0, board, err));
    tg::Palette title = tg::mluTitlePalette(*mlu);
    auto menu = slurp(dir + "/screen_menu.rgb"), play = slurp(dir + "/screen_play.rgb");
    CHECK(menu.size() == 320 * 200 * 3 && play.size() == 320 * 200 * 3);
    if (pics.size() != 85 || menu.size() != 192000 || play.size() != 192000) return 1;
    int total = 0, same = 0;
    compare(pics[tg::kDatNovice], tg::kMenuNovice.x, tg::kMenuNovice.y, title, menu, total, same);
    compare(pics[tg::kDatOnePlayer], tg::kMenuOnePlayer.x, tg::kMenuOnePlayer.y, title, menu, total, same);
    int menuTotal = total, menuSame = same;
    compare(pics[tg::kDatCursor], 90 + tg::kCursorOffset.x, 130 + tg::kCursorOffset.y, board, play, total, same);
    std::printf("menu buttons: %d/%d pixels identical; with the pointer: %d/%d\n", menuSame, menuTotal, same, total);
    CHECK(menuSame == menuTotal && same == total && total > 400);
    {
        auto expert = slurp(dir + "/screen_menu_expert.rgb"), cursor = slurp(dir + "/screen_menu_cursor.rgb");
        auto pause = slurp(dir + "/screen_pause.rgb"), disk = slurp(dir + "/screen_scoredisk.rgb");
        CHECK(expert.size() == 192000 && cursor.size() == 192000 && pause.size() == 192000 && disk.size() == 192000);
        if (expert.size() == 192000 && cursor.size() == 192000 && pause.size() == 192000 && disk.size() == 192000) {
            int t2 = 0, s2 = 0;
            compare(pics[tg::kDatExpert], tg::kMenuExpert.x, tg::kMenuExpert.y, title, expert, t2, s2);
            compare(pics[tg::kDatMenuCursor], 245 + tg::kCursorOffset.x, 100 + tg::kCursorOffset.y, title, cursor, t2, s2);
            compare(pics[tg::kDatPaused], tg::kPausePanel.x, tg::kPausePanel.y, board, pause, t2, s2);
            compare(pics[8], tg::kYesPos.x, tg::kYesPos.y, board, disk, t2, s2);
            compare(pics[9], tg::kNoPos.x, tg::kNoPos.y, board, disk, t2, s2);
            std::printf("expert label, menu pointer, pause panel, YES/NO: %d/%d pixels identical\n", s2, t2);
            CHECK(s2 == t2 && t2 > 3000);
        }
    }
    // blitPicture uses the same indices and colours
    tg::Image img; img.w = 320; img.h = 200; img.index.assign(64000, 0); img.rgb.assign(64000, tg::Rgb{1, 2, 3});
    tg::blitPicture(img, pics[tg::kDatCursor], 10, 10, board);
    int opaque = 0;
    for (auto v : pics[tg::kDatCursor].index) opaque += v != 0;
    int changed = 0;
    for (auto& c : img.rgb) changed += !(c == tg::Rgb{1, 2, 3});
    CHECK(opaque > 50 && changed <= opaque && changed > opaque / 2);
    std::printf(failures ? "%d FAILURES\n" : "all sprite tests passed\n", failures);
    return failures ? 1 : 0;
}
