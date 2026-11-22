#include "scale_filter.hpp"

#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

namespace tg {
namespace {

inline uint32_t pack(const Rgb& c) { return uint32_t(c.r) << 16 | uint32_t(c.g) << 8 | c.b; }
inline int clampi(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

// Runs rows(y0, y1) for the rows 0..H-1 split over a few threads (the rows are independent).
template <class F> void parallelRows(int H, F rows) {
    const int T = int(std::max(1u, std::min(8u, std::thread::hardware_concurrency())));
    std::vector<std::thread> pool;
    for (int t = 1; t < T; t++) pool.emplace_back(rows, H * t / T, H * (t + 1) / T);
    rows(0, H / T);
    for (std::thread& th : pool) th.join();
}

// Centre of output pixel o (count output pixels over srcCount source pixels), in source pixels.
inline double centre(int o, int count, int srcCount) { return (o + 0.5) * srcCount / count; }

void nearest(const Image& s, uint32_t* d, int W, int H) {
    std::vector<int> sx; sx.resize(size_t(W));
    for (int x = 0; x < W; x++) sx[size_t(x)] = clampi(int(centre(x, W, s.w)), 0, s.w - 1);
    parallelRows(H, [&](int y0, int y1) {
        for (int y = y0; y < y1; y++) {
            const Rgb* r = &s.rgb[size_t(clampi(int(centre(y, H, s.h)), 0, s.h - 1)) * s.w];
            for (int x = 0; x < W; x++) d[size_t(y) * W + x] = pack(r[sx[size_t(x)]]);
        }
    });
}

// Bilinear interpolation between the centres of the source pixels (8-bit weights).
void smooth(const Image& s, uint32_t* d, int W, int H) {
    struct Tap { int i0, i1, w; };   // w = weight of i1, 0..256
    auto taps = [&](int count, int srcCount) {
        std::vector<Tap> t; t.resize(size_t(count));
        for (int o = 0; o < count; o++) {
            const double c = centre(o, count, srcCount) - 0.5; const int i0 = int(std::floor(c));
            t[size_t(o)] = {clampi(i0, 0, srcCount - 1), clampi(i0 + 1, 0, srcCount - 1), int(std::lround((c - i0) * 256))};
        }
        return t;
    };
    const std::vector<Tap> tx = taps(W, s.w), ty = taps(H, s.h);
    parallelRows(H, [&](int y0, int y1) {
        for (int y = y0; y < y1; y++) {
            const Tap& a = ty[size_t(y)];
            const Rgb* r0 = &s.rgb[size_t(a.i0) * s.w]; const Rgb* r1 = &s.rgb[size_t(a.i1) * s.w];
            for (int x = 0; x < W; x++) {
                const Tap& b = tx[size_t(x)];
                const int w00 = (256 - a.w) * (256 - b.w), w01 = (256 - a.w) * b.w, w10 = a.w * (256 - b.w), w11 = a.w * b.w;   // sum 65536
                const Rgb &c00 = r0[b.i0], &c01 = r0[b.i1], &c10 = r1[b.i0], &c11 = r1[b.i1];
                const unsigned r = unsigned((c00.r * w00 + c01.r * w01 + c10.r * w10 + c11.r * w11 + 32768) >> 16);
                const unsigned g = unsigned((c00.g * w00 + c01.g * w01 + c10.g * w10 + c11.g * w11 + 32768) >> 16);
                const unsigned bl = unsigned((c00.b * w00 + c01.b * w01 + c10.b * w10 + c11.b * w11 + 32768) >> 16);
                d[size_t(y) * W + x] = r << 16 | g << 8 | bl;
            }
        }
    });
}

// CRT look: the output rows in the last 1/n of every source row (n = output rows per source row) are darker, like the gaps between
// the scanlines of a monitor, and for n >= 4 the first 1/n a little; each window column mixes 1/4 of the neighbouring source pixel on
// its near side. Brightness is lifted a little to make up for the dark rows. Exactly one output row falls into each band at any size.
void crt(const Image& s, uint32_t* d, int W, int H) {
    const double n = double(H) / s.h;
    struct Col { int p, q; };
    std::vector<Col> cols; cols.resize(size_t(W));
    for (int x = 0; x < W; x++) {
        const double c = centre(x, W, s.w); const int sx = clampi(int(c), 0, s.w - 1);
        cols[size_t(x)] = {sx, clampi(c - sx < 0.5 ? sx - 1 : sx + 1, 0, s.w - 1)};
    }
    parallelRows(H, [&](int y0, int y1) {
        for (int y = y0; y < y1; y++) {
            const double v = centre(y, H, s.h); const int sy = clampi(int(v), 0, s.h - 1); const double f = v - sy;
            int gain = 256;
            if (n >= 2 && f >= 1 - 1 / n) gain = 128;
            else if (n >= 4 && f < 1 / n) gain = 210;
            gain = gain * 9 / 8;   // +12% to compensate the darker rows
            const Rgb* r = &s.rgb[size_t(sy) * s.w];
            for (int x = 0; x < W; x++) {
                const Rgb &P = r[cols[size_t(x)].p], &N = r[cols[size_t(x)].q];
                const int cr = (P.r * 3 + N.r) / 4, cg = (P.g * 3 + N.g) / 4, cb = (P.b * 3 + N.b) / 4;
                const unsigned R = unsigned(std::min(255, cr * gain / 256)), G = unsigned(std::min(255, cg * gain / 256)), B = unsigned(std::min(255, cb * gain / 256));
                d[size_t(y) * W + x] = R << 16 | G << 8 | B;
            }
        }
    });
}

// Commodore 1084S look, computed in linear light. Horizontally the picture is softened a little (Gaussian, like the limited bandwidth
// of the monitor); vertically each source row is a beam with a Gaussian profile that gets wider when it is brighter (dark rows are thin
// lines with gaps, bright rows almost touch); then an aperture grille (R, G, B stripes in output columns) and a small brightness boost.
void monitor1084(const Image& s, uint32_t* d, int W, int H) {
    static float lin[256]; static uint8_t enc[4096]; static bool tables = false;
    if (!tables) {
        for (int i = 0; i < 256; i++) lin[i] = float(std::pow(i / 255.0, 2.2));
        for (int i = 0; i < 4096; i++) enc[i] = uint8_t(std::lround(std::pow(i / 4095.0, 1 / 2.2) * 255));
        tables = true;
    }
    // horizontal: Gaussian taps (sigma 0.5 source pixels) over 4 neighbours
    struct Tap4 { int i[4]; float w[4]; };
    std::vector<Tap4> tx; tx.resize(size_t(W));
    for (int x = 0; x < W; x++) {
        const double c = centre(x, W, s.w) - 0.5; const int i0 = int(std::floor(c)); const double f = c - i0;
        double sum = 0, w[4];
        for (int k = 0; k < 4; k++) { const double dx = f - (k - 1); w[k] = std::exp(-dx * dx / (2 * 0.5 * 0.5)); sum += w[k]; }
        for (int k = 0; k < 4; k++) { tx[size_t(x)].i[k] = clampi(i0 - 1 + k, 0, s.w - 1); tx[size_t(x)].w[k] = float(w[k] / sum); }
    }
    std::vector<float> hor(size_t(W) * s.h * 3);
    parallelRows(s.h, [&](int y0, int y1) {
        for (int y = y0; y < y1; y++) {
            const Rgb* r = &s.rgb[size_t(y) * s.w]; float* o = &hor[size_t(y) * W * 3];
            for (int x = 0; x < W; x++) {
                const Tap4& T = tx[size_t(x)]; float R = 0, G = 0, B = 0;
                for (int k = 0; k < 4; k++) { const Rgb& c = r[T.i[k]]; R += lin[c.r] * T.w[k]; G += lin[c.g] * T.w[k]; B += lin[c.b] * T.w[k]; }
                o[3 * x] = R; o[3 * x + 1] = G; o[3 * x + 2] = B;
            }
        }
    });
    // vertical beam: per output row the own source row and the nearer neighbour row, with weight tables by brightness (64 levels)
    const int L = 64;
    struct Row { int a, b; float near[L], far[L]; };
    std::vector<Row> rows; rows.resize(size_t(H));
    for (int y = 0; y < H; y++) {
        const double c = centre(y, H, s.h); const int sy = clampi(int(c), 0, s.h - 1); const double off = c - sy - 0.5;   // -0.5..0.5 from the row centre
        Row& R = rows[size_t(y)];
        R.a = sy; R.b = clampi(off < 0 ? sy - 1 : sy + 1, 0, s.h - 1);
        for (int l = 0; l < L; l++) {
            const double sigma = 0.30 + 0.16 * (l / double(L - 1));
            const double norm = 1.0 / (sigma * std::sqrt(2 * 3.14159265358979));
            auto beam = [&](double dd) { return float(norm * std::exp(-dd * dd / (2 * sigma * sigma))); };
            R.near[l] = beam(std::fabs(off)); R.far[l] = beam(1.0 - std::fabs(off));
        }
    }
    const float maskOn = 1.0f, maskOff = 0.78f, gain = 3.0f / (maskOn + 2 * maskOff) * 1.05f;
    parallelRows(H, [&](int y0, int y1) {
        for (int y = y0; y < y1; y++) {
            const Row& R = rows[size_t(y)];
            const float* a = &hor[size_t(R.a) * W * 3]; const float* b = &hor[size_t(R.b) * W * 3];
            for (int x = 0; x < W; x++) {
                uint32_t out = 0;
                for (int c = 0; c < 3; c++) {
                    const float va = a[3 * x + c], vb = b[3 * x + c];
                    float v = va * R.near[clampi(int(va * (L - 1) + 0.5f), 0, L - 1)] + vb * R.far[clampi(int(vb * (L - 1) + 0.5f), 0, L - 1)];
                    v *= (x % 3 == c ? maskOn : maskOff) * gain;
                    out = out << 8 | enc[clampi(int(v * 4095 + 0.5f), 0, 4095)];
                }
                d[size_t(y) * W + x] = out;
            }
        }
    });
}

}  // namespace

void scaleFrame(const Image& src, uint32_t* dst, int outW, int outH, ScaleFilter mode) {
    if (outW < 1 || outH < 1) return;
    switch (mode) {
    case ScaleFilter::Smooth: smooth(src, dst, outW, outH); break;
    case ScaleFilter::Crt: crt(src, dst, outW, outH); break;
    case ScaleFilter::Monitor1084: monitor1084(src, dst, outW, outH); break;
    default: nearest(src, dst, outW, outH); break;
    }
}

}  // namespace tg
