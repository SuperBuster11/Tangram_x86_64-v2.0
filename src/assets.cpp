#include "assets.hpp"

namespace tg {

Palette mluTitlePalette(const Bytes& mlu) { return readPalette(mlu.data() + kMluTitlePalette); }

bool mluBoardPalette(const Bytes& mlu, int index, Palette& out, std::string& err) {
    if (index < 0 || index > 2) { err = "palette index out of range"; return false; }
    size_t at = kMluBoardPalettes + size_t(index) * 64;
    if (mlu.size() < at + 64) { err = "TANGRAM.MLU too short for the board palettes"; return false; }
    out = readPalette(mlu.data() + at);
    return true;
}

bool mluBoardPicture(const Bytes& mlu, int paletteIndex, Image& out, std::string& err) {
    Palette pal;
    if (!mluBoardPalette(mlu, paletteIndex, pal, err)) return false;
    if (mlu.size() < kMluBoardPicture + 40000) { err = "TANGRAM.MLU too short for the board picture"; return false; }
    Bytes pic(mlu.begin() + kMluBoardPicture, mlu.begin() + kMluBoardPicture + 40000);
    return decodeLo5(pic, pal, out, err);
}

}  // namespace tg
