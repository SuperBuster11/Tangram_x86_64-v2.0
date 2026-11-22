// dump_dat <game.adf> <sheet.png> [scale]: contact sheet of all pictures in TANGRAM.DAT (board palette 0).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include "../src/assets.hpp"
#include "../src/dat.hpp"
int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: dump_dat game.adf sheet.png [scale]\n"); return 2; }
    tg::Adf adf; std::string err;
    if (!adf.loadFile(argv[1], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    const tg::Bytes* dat = adf.file("TANGRAM.DAT");
    const tg::Bytes* mlu = adf.file("TANGRAM.MLU");
    if (!dat || !mlu) { std::fprintf(stderr, "missing TANGRAM.DAT / TANGRAM.MLU\n"); return 1; }
    std::vector<tg::DatPicture> pics;
    tg::Palette pal;
    if (!tg::parseDat(*dat, pics, err) || !tg::mluBoardPalette(*mlu, 0, pal, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    const int W = 1000, margin = 4;
    tg::Image sheet; sheet.w = W; sheet.h = 0;
    std::vector<std::pair<int, int>> pos; int x = margin, y = margin, rowH = 0;
    for (auto& p : pics) {
        if (x + p.w + margin > W) { x = margin; y += rowH + margin; rowH = 0; }
        pos.push_back({x, y}); x += p.w + margin; rowH = std::max(rowH, p.h);
    }
    sheet.h = y + rowH + margin;
    sheet.index.assign(size_t(sheet.w) * sheet.h, 0);
    sheet.rgb.assign(size_t(sheet.w) * sheet.h, tg::Rgb{200, 200, 200});
    for (size_t i = 0; i < pics.size(); i++) {
        tg::Image im = pics[i].toImage(pal);
        for (int yy = 0; yy < im.h; yy++)
            for (int xx = 0; xx < im.w; xx++) sheet.rgb[size_t(pos[i].second + yy) * W + pos[i].first + xx] = im.rgb[size_t(yy) * im.w + xx];
    }
    std::printf("%zu pictures, sheet %dx%d\n", pics.size(), sheet.w, sheet.h);
    return tg::writePng(argv[2], sheet, argc > 3 ? std::atoi(argv[3]) : 1) ? 0 : 1;
}
