#include "dat.hpp"

namespace tg {
namespace {

bool readNumberLine(const Bytes& d, size_t& p, size_t& value) {
    size_t v = 0, digits = 0;
    while (p < d.size() && d[p] >= '0' && d[p] <= '9') { v = v * 10 + (d[p] - '0'); p++; digits++; if (digits > 9) return false; }
    if (!digits || p >= d.size() || d[p] != '\n') return false;
    p++;
    value = v;
    return true;
}

}  // namespace

Image DatPicture::toImage(const Palette& pal) const {
    Image img;
    img.w = w; img.h = h;
    img.index = index;
    img.rgb.resize(index.size());
    for (size_t i = 0; i < index.size(); i++) img.rgb[i] = pal.c[index[i] & 31];
    return img;
}

bool parseDat(const Bytes& d, std::vector<DatPicture>& out, std::string& err) {
    out.clear();
    size_t p = 0, count = 0;
    if (!readNumberLine(d, p, count)) { err = "missing picture count"; return false; }
    while (p < d.size()) {
        size_t len = 0;
        if (!readNumberLine(d, p, len)) { err = "bad length line at offset " + std::to_string(p); return false; }
        if (len < 6 || p + len > d.size()) { err = "bad record length at offset " + std::to_string(p); return false; }
        const uint8_t* r = &d[p];
        auto word = [&](int i) { return (int(r[i]) << 8) | r[i + 1]; };
        DatPicture pic;
        pic.depth = word(0); pic.w = word(2) + 1; pic.h = word(4) + 1;
        if (pic.depth < 1 || pic.depth > 5 || pic.w > 1024 || pic.h > 1024) { err = "bad picture header"; return false; }
        size_t rowBytes = size_t((pic.w + 15) / 16) * 2;
        if (len != 6 + size_t(pic.depth) * rowBytes * pic.h) { err = "record size does not match its header"; return false; }
        pic.index.assign(size_t(pic.w) * pic.h, 0);
        for (int pl = 0; pl < pic.depth; pl++)
            for (int y = 0; y < pic.h; y++)
                for (int x = 0; x < pic.w; x++) {
                    uint8_t byte = r[6 + (size_t(pl) * pic.h + y) * rowBytes + x / 8];
                    if (byte & (0x80 >> (x & 7))) pic.index[size_t(y) * pic.w + x] |= uint8_t(1 << pl);
                }
        out.push_back(std::move(pic));
        p += len;
    }
    if (out.size() != count + 1) { err = "picture count does not match"; return false; }
    return true;
}

}  // namespace tg
