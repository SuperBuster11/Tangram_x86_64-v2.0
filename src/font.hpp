// Bitmap font: two 296x8 strips in TANGRAM.DAT (records 5 and 6), 8x7 glyph cells in this order:
// A-Z, 0-9, (c). Space has no glyph (the cursor just advances 8 pixels). Index 0 is transparent.
#pragma once
#include <string>

#include "dat.hpp"

namespace tg {

constexpr int kFontLight = 5;   // record with the dark-blue glyphs used for the credits/title texts
constexpr int kFontUi = 6;      // record used for the counters, time and the password panel
constexpr int kGlyphW = 8, kGlyphH = 7;

// Cell number of a character in the strip, or -1 when it has no glyph (space and everything unknown).
int glyphCell(char c);

// Draws `text` with its top-left corner at (x, y), 8 pixels per character, on an Image that carries both the
// palette indices and the colours. Pixels with index 0 stay as they are. Returns the x after the last character.
// opaque = true blits whole cells (index 0 is black ink; used for the UI font, record 6); false skips index 0.
int drawText(Image& dst, const DatPicture& strip, int x, int y, const std::string& text, const Palette& pal, bool opaque = false);

}  // namespace tg
