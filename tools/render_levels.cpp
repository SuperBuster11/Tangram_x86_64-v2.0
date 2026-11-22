// render_levels <levels.bin> <out.png>: contact sheet with all figures (20 x 10).
#include <cstdio>
#include <string>

#include "../src/image.hpp"
#include "../src/levels.hpp"
#include "../src/raster.hpp"

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: render_levels levels.bin out.png\n"); return 2; }
    std::vector<tg::Level> lv;
    std::string err;
    if (!tg::loadLevelsFile(argv[1], lv, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    const int cw = 96, ch = 64, cols = 10;
    int rows = int((lv.size() + cols - 1) / cols);
    tg::Image img;
    img.w = cw * cols; img.h = ch * rows;
    img.rgb.assign(size_t(img.w) * img.h, tg::Rgb{40, 28, 20});
    for (size_t i = 0; i < lv.size(); i++) {
        std::vector<tg::Pt> poly;
        for (int k = 0; k + 1 < int(lv[i].coords.size()); k += 2)
            poly.push_back({cw / 2 + lv[i].coords[k] * 4 / 5 / 2 * 1, ch / 2 - lv[i].coords[k + 1] * 4 / 5 / 2 * 1});
        std::vector<uint8_t> m(size_t(cw) * ch, 0);
        tg::fillPolygon(poly, cw, ch, m);
        int ox = int(i % cols) * cw, oy = int(i / cols) * ch;
        for (int y = 0; y < ch; y++)
            for (int x = 0; x < cw; x++)
                if (m[size_t(y) * cw + x]) img.rgb[size_t(oy + y) * img.w + ox + x] = tg::Rgb{200, 60, 20};
    }
    return tg::writePng(argv[2], img, 1) ? 0 : 1;
}
