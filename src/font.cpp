#include "font.hpp"

namespace tg {

int glyphCell(char c) {
    if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '0' && c <= '9') return 26 + (c - '0');
    if (c == '@') return 36;   // drawn as a solid black block (see drawText); the cell itself holds the copyright sign
    return -1;
}

int drawText(Image& dst, const DatPicture& strip, int x, int y, const std::string& text, const Palette& pal, bool opaque) {
    for (char c : text) {
        int cell = glyphCell(c);
        if (cell >= 0) {
            for (int gy = 0; gy < kGlyphH; gy++)
                for (int gx = 0; gx < kGlyphW; gx++) {
                    uint8_t v = c == '@' ? 0 : strip.index[size_t(gy) * strip.w + cell * kGlyphW + gx];   // '@' is a solid black block in the original
                    int px = x + gx, py = y + gy;
                    if ((!v && !opaque) || px < 0 || py < 0 || px >= dst.w || py >= dst.h) continue;
                    dst.index[size_t(py) * dst.w + px] = v;
                    dst.rgb[size_t(py) * dst.w + px] = pal.c[v & 31];
                }
        }
        x += kGlyphW;
    }
    return x;
}

}  // namespace tg
