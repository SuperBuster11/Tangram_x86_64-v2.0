// Pictures and palettes stored in TANGRAM.MLU (file layout found by following the game's own reads).
//   0      .. 64      title-screen palette (32 colour words)
//   64     .. 21662   figure table (see levels.hpp)
//   21662  .. 61662   board picture: 320x200, 5 bitplanes of 8000 bytes (dragon board + piece tray)
//   61662  .. 62662   passwords (see passwords.hpp)
//   62662  .. 62854   three 32-colour palettes for the board picture
#pragma once
#include <string>

#include "adf.hpp"
#include "image.hpp"

namespace tg {

constexpr size_t kMluTitlePalette = 0, kMluBoardPicture = 21662, kMluBoardPalettes = 62662;

Palette mluTitlePalette(const Bytes& mlu);
// index 0..2; palette 0 is the one the first levels are drawn with.
bool mluBoardPalette(const Bytes& mlu, int index, Palette& out, std::string& err);
bool mluBoardPicture(const Bytes& mlu, int paletteIndex, Image& out, std::string& err);

}  // namespace tg
