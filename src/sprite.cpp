#include "sprite.hpp"

namespace tg {

void blitPicture(Image& dst, const DatPicture& pic, int x, int y, const Palette& pal, bool transparentZero) {
    for (int gy = 0; gy < pic.h; gy++)
        for (int gx = 0; gx < pic.w; gx++) {
            uint8_t v = pic.index[size_t(gy) * pic.w + gx];
            int px = x + gx, py = y + gy;
            if ((transparentZero && !v) || px < 0 || py < 0 || px >= dst.w || py >= dst.h) continue;
            dst.index[size_t(py) * dst.w + px] = v;
            dst.rgb[size_t(py) * dst.w + px] = pal.c[v & 31];
        }
}

}  // namespace tg
