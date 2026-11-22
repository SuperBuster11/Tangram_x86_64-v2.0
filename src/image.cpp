#include "image.hpp"

#include <algorithm>
#include <cstdio>

namespace tg {

Rgb amigaColor(uint16_t w) {
    return Rgb{uint8_t(((w >> 8) & 15) * 17), uint8_t(((w >> 4) & 15) * 17), uint8_t((w & 15) * 17)};
}

Palette readPalette(const uint8_t* p) {
    Palette pal;
    for (int i = 0; i < 32; i++) pal.c[i] = amigaColor(uint16_t(p[i * 2] << 8 | p[i * 2 + 1]));
    return pal;
}

bool decodeLo5(const Bytes& d, const Palette& pal, Image& out, std::string& err) {
    constexpr int W = 320, H = 200, planes = 5, planeBytes = W / 8 * H;
    if (d.size() < size_t(planes) * planeBytes) {
        err = "picture too short";
        return false;
    }
    out.w = W;
    out.h = H;
    out.index.assign(W * H, 0);
    out.rgb.assign(W * H, Rgb{});
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int v = 0;
            for (int p = 0; p < planes; p++) {
                uint8_t byte = d[size_t(p) * planeBytes + size_t(y) * (W / 8) + x / 8];
                v |= ((byte >> (7 - x % 8)) & 1) << p;
            }
            out.index[y * W + x] = uint8_t(v);
            out.rgb[y * W + x] = pal.c[v];
        }
    }
    return true;
}

namespace {

uint32_t crcTable[256];
bool crcInit = false;

uint32_t crc32(const uint8_t* p, size_t n, uint32_t c = 0xFFFFFFFFu) {
    if (!crcInit) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t v = i;
            for (int k = 0; k < 8; k++) v = (v & 1) ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            crcTable[i] = v;
        }
        crcInit = true;
    }
    for (size_t i = 0; i < n; i++) c = crcTable[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c;
}

void put32(std::vector<uint8_t>& v, uint32_t x) {
    for (int s = 24; s >= 0; s -= 8) v.push_back(uint8_t(x >> s));
}

void chunk(FILE* f, const char* type, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> b;
    put32(b, uint32_t(data.size()));
    b.insert(b.end(), type, type + 4);
    b.insert(b.end(), data.begin(), data.end());
    uint32_t crc = ~crc32(b.data() + 4, b.size() - 4);
    put32(b, crc);
    std::fwrite(b.data(), 1, b.size(), f);
}

}  // namespace

bool writePng(const std::string& path, const Image& img, int scale) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const int W = img.w * scale, H = img.h * scale;
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::fwrite(sig, 1, 8, f);
    std::vector<uint8_t> ihdr;
    put32(ihdr, W);
    put32(ihdr, H);
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
    chunk(f, "IHDR", ihdr);

    std::vector<uint8_t> raw;
    raw.reserve(size_t(H) * (W * 3 + 1));
    for (int y = 0; y < H; y++) {
        raw.push_back(0);
        for (int x = 0; x < W; x++) {
            const Rgb& c = img.rgb[(y / scale) * img.w + x / scale];
            raw.push_back(c.r);
            raw.push_back(c.g);
            raw.push_back(c.b);
        }
    }
    // zlib stream with stored blocks
    std::vector<uint8_t> z{0x78, 0x01};
    size_t pos = 0;
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
    }
    while (pos < raw.size() || raw.empty()) {
        size_t n = std::min<size_t>(65535, raw.size() - pos);
        bool last = pos + n >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(uint8_t(n & 0xFF));
        z.push_back(uint8_t(n >> 8));
        z.push_back(uint8_t(~n & 0xFF));
        z.push_back(uint8_t((~n >> 8) & 0xFF));
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
        if (last) break;
    }
    put32(z, (b << 16) | a);
    chunk(f, "IDAT", z);
    chunk(f, "IEND", {});
    std::fclose(f);
    return true;
}

}  // namespace tg
