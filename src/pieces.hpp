// The seven pieces ("klocki") of the game and their rotation.
#pragma once
#include <vector>

#include "raster.hpp"

namespace tg {

enum PieceType { kLargeTriangle = 0, kSquare = 1, kMediumTriangle = 2, kSmallTriangle = 3, kParallelogram = 4, kPieceTypes = 5 };

// How many of each type the player has (2 large, 1 square, 1 medium, 2 small, 1 parallelogram = 7).
int pieceCount(PieceType t);

constexpr int kRotations = 16;  // 22.5 degrees each (manual: "in Schritten zu je 22.5 Grad")

// Outline of a piece rotated r steps counter-clockwise on screen around the cursor hot spot, as offsets from
// the cursor position. The values are rounded exactly like the game does (verified on all 16 rotations).
std::vector<Pt> pieceOutline(PieceType t, int rotation);

// Same, placed with the cursor at (cx, cy).
std::vector<Pt> placedOutline(PieceType t, int rotation, int cx, int cy);

}  // namespace tg
