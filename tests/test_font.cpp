// test_font <game.adf> <tests/data>: text drawn with the font strips must reproduce the game's own password panel text.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include "../src/assets.hpp"
#include "../src/font.hpp"
#include "../src/layout.hpp"
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
int main(int argc, char** argv) {
    CHECK(tg::glyphCell('A') == 0 && tg::glyphCell('Z') == 25 && tg::glyphCell('0') == 26 && tg::glyphCell('9') == 35 &&
          tg::glyphCell('@') == 36 && tg::glyphCell(' ') == -1 && tg::glyphCell('q') == 16);
    const char* adfPath = argc >= 2 && argv[1][0] ? argv[1] : std::getenv("TANGRAM_ADF");
    std::string dir = argc >= 3 ? argv[2] : "tests/data", err;
    if (!adfPath) { std::printf(failures ? "%d FAILURES\n" : "unit part passed (set TANGRAM_ADF for the rest)\n", failures); return failures ? 1 : 0; }
    tg::Adf adf;
    CHECK(adf.loadFile(adfPath, err));
    const tg::Bytes* dat = adf.file("TANGRAM.DAT");
    const tg::Bytes* mlu = adf.file("TANGRAM.MLU");
    if (!dat || !mlu) return 1;
    std::vector<tg::DatPicture> pics;
    tg::Palette pal;
    CHECK(tg::parseDat(*dat, pics, err) && tg::mluBoardPalette(*mlu, 0, pal, err));
    std::ifstream f(dir + "/password_screen.rgb", std::ios::binary);
    std::vector<char> shot((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    CHECK(shot.size() == 320 * 200 * 3);
    if (pics.size() < 7 || shot.size() != 320 * 200 * 3) return 1;
    tg::Image img;
    img.w = 320; img.h = 200;
    img.index.assign(320 * 200, 0);
    img.rgb.assign(320 * 200, tg::Rgb{0, 0, 0});
    const tg::DatPicture& ui = pics[tg::kFontUi];
    tg::drawText(img, ui, tg::kPwPassword.x, tg::kPwPassword.y, "PASSWORD", pal);
    tg::drawText(img, ui, tg::kPwFor.x, tg::kPwFor.y, "FOR", pal);
    tg::drawText(img, ui, tg::kPwLevel.x, tg::kPwLevel.y, "LEVEL", pal);
    tg::drawText(img, ui, tg::kPwLevelNo.x, tg::kPwLevelNo.y, "002", pal);
    tg::drawText(img, ui, tg::kPwCode.x, tg::kPwCode.y, "03797", pal);
    int drawn = 0, same = 0;
    for (int i = 0; i < 320 * 200; i++) {
        if (!img.index[i]) continue;
        drawn++;
        same += img.rgb[i].r == uint8_t(shot[i * 3]) && img.rgb[i].g == uint8_t(shot[i * 3 + 1]) && img.rgb[i].b == uint8_t(shot[i * 3 + 2]);
    }
    std::printf("glyph pixels drawn: %d, identical to the game's screenshot: %d\n", drawn, same);
    CHECK(drawn > 300);
    CHECK(same == drawn);
    std::printf(failures ? "%d FAILURES\n" : "all font tests passed\n", failures);
    return failures ? 1 : 0;
}
