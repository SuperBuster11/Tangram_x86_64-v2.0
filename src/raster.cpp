#include "raster.hpp"

#include <algorithm>

namespace tg {

void fillPolygon(const std::vector<Pt>& p, int w, int h, std::vector<uint8_t>& mask) {
    if (p.size() < 3) return;
    if (mask.size() != size_t(w) * h) mask.assign(size_t(w) * h, 0);
    const size_t n = p.size();
    std::vector<int> xs;
    for (int y = 0; y < h; y++) {
        xs.clear();
        for (size_t i = 0; i < n; i++) {
            Pt a = p[i], b = p[(i + 1) % n];
            if (a.y == b.y) continue;
            if (a.y > b.y) std::swap(a, b);
            // half-open rule on scanline y (sampled at the pixel's top edge)
            if (y < a.y || y >= b.y) continue;
            // x where the edge crosses the scanline, rounded to the nearest pixel
            long num = long(b.x - a.x) * (y - a.y);
            int den = b.y - a.y;
            int x = a.x + int(num >= 0 ? (2 * num + den) / (2 * den) : -((-2 * num + den) / (2 * den)));
            xs.push_back(x);
        }
        std::sort(xs.begin(), xs.end());
        for (size_t i = 0; i + 1 < xs.size(); i += 2)
            for (int x = std::max(xs[i], 0); x <= std::min(xs[i + 1], w - 1); x++) mask[size_t(y) * w + x] = 1;
    }
}

void fillPolygonDrawn(const std::vector<Pt>& poly, int w, int h, std::vector<uint8_t>& mask) {
    fillPolygon(poly, w, h, mask);
    const size_t n = poly.size();
    for (size_t i = 0; i < n; i++) {
        const Pt a = poly[i], prev = poly[(i + n - 1) % n];
        if (prev.y == a.y && prev.x != a.x) continue;   // not the start of a run of equal-y vertices
        size_t j = i; int x0 = a.x, x1 = a.x;
        while (poly[(j + 1) % n].y == a.y && (j + 1) % n != i) { j = (j + 1) % n; x0 = std::min(x0, poly[j].x); x1 = std::max(x1, poly[j].x); }
        if (prev.y < a.y && poly[(j + 1) % n].y < a.y && a.y >= 0 && a.y < h)
            for (int x = std::max(x0, 0); x <= std::min(x1, w - 1); x++) mask[size_t(a.y) * w + x] = 1;
    }
}

}  // namespace tg
