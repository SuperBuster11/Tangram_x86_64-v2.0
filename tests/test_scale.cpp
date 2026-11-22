#include <cstdio>
#include <vector>
#include "../src/scale_filter.hpp"
using namespace tg;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
static Image make(int w, int h) { Image i; i.w = w; i.h = h; i.index.assign(size_t(w) * h, 0); i.rgb.assign(size_t(w) * h, Rgb{0, 0, 0}); return i; }
int main() {
    for (int n = 1; n <= 5; n++) {
        Image a = make(4, 4);
        for (int i = 0; i < 16; i++) a.rgb[size_t(i)] = Rgb{uint8_t(i * 10), uint8_t(255 - i), uint8_t(i * 7)};
        std::vector<uint32_t> o(size_t(16 * n * n));
        scaleFrame(a, o.data(), n, ScaleFilter::Off);
        bool ok = true;
        for (int y = 0; y < 4 * n; y++) for (int x = 0; x < 4 * n; x++) { const Rgb& c = a.rgb[size_t(y / n) * 4 + x / n]; ok &= o[size_t(y) * 4 * n + x] == (uint32_t(c.r) << 16 | uint32_t(c.g) << 8 | c.b); }
        CHECK(ok);   // Off = plain nearest neighbour
        // flat colour stays flat in every mode
        Image f = make(5, 3); for (auto& c : f.rgb) c = Rgb{10, 200, 90};
        for (ScaleFilter m : {ScaleFilter::Smooth}) {
            std::vector<uint32_t> g(size_t(15 * n * n));
            scaleFrame(f, g.data(), n, m);
            bool flat = true; for (uint32_t v : g) flat &= v == (10u << 16 | 200u << 8 | 90u);
            CHECK(flat);
        }
    }
    {   // smooth: a black/white step gets intermediate grey between the pixel centres
        Image s = make(2, 1); s.rgb[1] = Rgb{255, 255, 255};
        std::vector<uint32_t> o(size_t(2 * 4) * 4);   // 8x4 output, the first row is checked
        scaleFrame(s, o.data(), 4, ScaleFilter::Smooth);
        CHECK((o[0] & 255) == 0 && (o[7] & 255) == 255);
        bool mono = true; for (int i = 1; i < 8; i++) mono &= (o[size_t(i)] & 255) >= (o[size_t(i - 1)] & 255);
        CHECK(mono && (o[3] & 255) > 0 && (o[4] & 255) < 255);
    }
    {   // CRT: every 4th output row of a flat grey area is darker (scanline gap), the others a little brighter
        Image g = make(4, 4);
        for (Rgb& c : g.rgb) c = Rgb{160, 160, 160};
        std::vector<uint32_t> o(size_t(16 * 16));
        scaleFrame(g, o.data(), 4, ScaleFilter::Crt);
        const unsigned row1 = o[size_t(1 * 16 + 5)] & 0xff, row3 = o[size_t(3 * 16 + 5)] & 0xff;
        CHECK(row3 < row1 && row3 < 160 && row1 >= 160);
    }
    {   // F8 cycle: 1084S -> CRT -> smoothed -> off -> 1084S
        ScaleFilter f = ScaleFilter::Monitor1084; const ScaleFilter want[] = {ScaleFilter::Crt, ScaleFilter::Smooth, ScaleFilter::Off, ScaleFilter::Monitor1084};
        for (ScaleFilter w : want) { f = nextFilter(f); CHECK(f == w); }
    }
    {   // any output size (fullscreen): 320x200 -> 1728x1080 (factor 5.4); every filter fills the whole buffer, CRT darkens exactly
        // one output row per source row
        Image g = make(320, 200); for (size_t i = 0; i < g.rgb.size(); i++) g.rgb[i] = Rgb{uint8_t(100 + i % 7), 150, 90};
        const int W = 1728, H = 1080;
        for (ScaleFilter m : {ScaleFilter::Off, ScaleFilter::Smooth, ScaleFilter::Crt, ScaleFilter::Monitor1084}) {
            std::vector<uint32_t> o(size_t(W) * H + 1, 0xDEADBEEFu);
            scaleFrame(g, o.data(), W, H, m);
            bool filled = true; for (size_t i = 0; i < size_t(W) * H; i++) filled &= o[i] != 0xDEADBEEFu && (o[i] >> 24) == 0;
            CHECK(filled && o[size_t(W) * H] == 0xDEADBEEFu);
            if (m == ScaleFilter::Crt) {
                int dark = 0; for (int y = 0; y < H; y++) dark += ((o[size_t(y) * W + 7] >> 8) & 255) < 120;
                CHECK(dark == 200);
            }
        }
    }
    {   // 1084S: a white area keeps roughly its brightness; across a source row the beam centre is brighter than its edge;
        // the aperture grille makes neighbouring output columns differ in colour
        Image g = make(6, 6); for (Rgb& c : g.rgb) c = Rgb{200, 200, 200};
        const int n = 4; std::vector<uint32_t> o(size_t(6 * n) * 6 * n);
        scaleFrame(g, o.data(), n, ScaleFilter::Monitor1084);
        const int W = 6 * n;
        long sum = 0; int cnt = 0;
        for (int y = 2 * n; y < 4 * n; y++) for (int x = 2 * n; x < 5 * n; x++) { const uint32_t v = o[size_t(y) * W + x]; sum += ((v >> 16) & 255) + ((v >> 8) & 255) + (v & 255); cnt += 3; }
        const double avg = double(sum) / cnt;
        std::printf("1084S: average %.1f for source 200\n", avg);
        CHECK(avg > 150 && avg < 235);
        auto lum = [&](int x, int y) { const uint32_t v = o[size_t(y) * W + x]; return ((v >> 16) & 255) + ((v >> 8) & 255) + (v & 255); };
        CHECK(lum(12, 3 * n + 1) + lum(13, 3 * n + 1) + lum(14, 3 * n + 1) > lum(12, 3 * n) + lum(13, 3 * n) + lum(14, 3 * n) - 1);
        CHECK(o[size_t(2 * n) * W + 12] != o[size_t(2 * n) * W + 13]);
    }
    std::printf(failures ? "%d FAILURES\n" : "all scale filter tests passed\n", failures);
    return failures ? 1 : 0;
}
