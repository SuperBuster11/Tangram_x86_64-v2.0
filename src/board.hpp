// Board model: placed pieces and the "is the figure covered" test.
#pragma once
#include <vector>

#include "levels.hpp"
#include "pieces.hpp"

namespace tg {

struct Placed { PieceType type; int rotation; int x, y; };

struct Coverage {
    int figurePixels = 0;   // pixels of the figure
    int uncovered = 0;      // figure pixels not covered by any piece
    int outside = 0;        // piece pixels lying outside the figure
    int overlap = 0;        // pixels covered by more than one piece
};

constexpr int kBoardW = 320, kBoardH = 200;

// Rasterises the figure of `level` and the placed pieces the way the game draws them.
Coverage measureCoverage(const Level& level, const std::vector<Placed>& pieces);

// What is left of the figure on the screen (colour 14): the figure as drawn minus the pieces as drawn, kBoardW x kBoardH.
std::vector<uint8_t> uncoveredMask(const Level& level, const std::vector<Placed>& pieces);

// The test of the original (machine code at 0x10d3c6, run every frame while all 7 pieces are down) on such a mask:
// the board is 224 x 200 pixels; rows 0..198 are examined. The stage is solved unless
//  - the 75th visible pixel (counted row by row, 32-pixel words left to right, each word from its right end) is the
//    leftmost visible pixel of its word (a flaw of the original: otherwise its counter wraps and the count never fails), or
//  - at some position P + left + right + above + below >= 4 (P = the pixel itself, the others its 4 neighbours, 1 = visible)
//    within a 32-pixel word that holds a visible pixel; i.e. thin slivers pass, anything 2+ pixels thick fails.
// The original also refuses while the left button is held; that is the caller's business. A quirk of the original is
// kept: it shifts the low word of the word to the upper left right by one bit when it takes the left neighbour, and that
// shifted word is what is later read as "above".
bool originalSolveTest(const std::vector<uint8_t>& visible, int w);

// The stage counts as solved (all 7 pieces placed): originalSolveTest(uncoveredMask(level, pieces)).
bool solved(const Level& level, const std::vector<Placed>& pieces);

}  // namespace tg
