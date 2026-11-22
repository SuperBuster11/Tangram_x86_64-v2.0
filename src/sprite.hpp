// Drawing of pictures from TANGRAM.DAT on an Image (colour 0 is transparent, as for the game's sprites and labels).
#pragma once
#include "dat.hpp"

namespace tg {

void blitPicture(Image& dst, const DatPicture& pic, int x, int y, const Palette& pal, bool transparentZero = true);

}  // namespace tg
