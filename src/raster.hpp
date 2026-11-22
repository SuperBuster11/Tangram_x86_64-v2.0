// Polygon filling (even-odd, pixel centres) used to draw figures and to test coverage.
#pragma once
#include <cstdint>
#include <vector>

namespace tg {

struct Pt { int x, y; };

// Fills the polygon into a w*h mask (1 = inside). Rows are half-open (top edge in, bottom edge out) and each
// span includes its right-hand end pixel, which is what the game's area fill produces (checked against
// screenshots of the real game). Existing set pixels are kept (OR).
void fillPolygon(const std::vector<Pt>& poly, int w, int h, std::vector<uint8_t>& mask);

// fillPolygon plus the bottom row of every corner that points down: the half-open fill drops it, the game draws it
// (the tip of a downward corner is a whole pixel row). This is what is on the screen.
void fillPolygonDrawn(const std::vector<Pt>& poly, int w, int h, std::vector<uint8_t>& mask);

}  // namespace tg
