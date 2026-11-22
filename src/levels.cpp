#include "levels.hpp"

#include <fstream>
#include <iterator>

namespace tg {

bool parseLevels(const Bytes& d, std::vector<Level>& out, std::string& err) {
    out.clear();
    auto u16 = [&](size_t o) { return uint16_t(d[o] << 8 | d[o + 1]); };
    if (d.size() < 6 || d[0] != 'T' || d[1] != 'G' || d[2] != 'L' || d[3] != 'V') {
        err = "not a level table";
        return false;
    }
    size_t count = u16(4), o = 6;
    for (size_t i = 0; i < count; i++) {
        if (o + 4 > d.size()) { err = "truncated level table"; return false; }
        Level l;
        l.time = u16(o);
        size_t n = u16(o + 2);
        o += 4;
        if (n > 64 || o + 2 * n > d.size()) { err = "bad level record"; return false; }
        for (size_t k = 0; k < n; k++) l.coords.push_back(int16_t(u16(o + 2 * k)));
        o += 2 * n;
        out.push_back(std::move(l));
    }
    return true;
}

bool loadLevelsFile(const std::string& path, std::vector<Level>& out, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { err = "cannot open " + path; return false; }
    Bytes d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return parseLevels(d, out, err);
}

bool parseLevelsFromMlu(const Bytes& mlu, std::vector<Level>& out, std::string& err) {
    constexpr size_t kOffset = 64, kRows = 54, kCols = 200;
    if (mlu.size() < kOffset + kRows * kCols * 2 - 2) { err = "TANGRAM.MLU too short"; return false; }
    auto word = [&](size_t row, size_t col) -> int {
        size_t at = kOffset + 2 * (row * kCols + col);
        if (at + 2 > mlu.size()) return 0;  // the game's read is 2 bytes short of the array; the last word is 0
        return int(int16_t((mlu[at] << 8) | mlu[at + 1]));
    };
    out.clear();
    for (size_t c = 0; c < kCols; c++) {
        Level lv;
        lv.time = word(0, c);
        int n = word(1, c);
        if (n < 0 || n > int(kRows) - 2) { err = "bad vertex count in level " + std::to_string(c + 1); return false; }
        for (int i = 0; i < n; i++) lv.coords.push_back(int16_t(word(2 + i, c)));
        out.push_back(lv);
    }
    return true;
}

}  // namespace tg
