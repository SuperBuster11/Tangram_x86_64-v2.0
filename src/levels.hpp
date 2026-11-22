// Figure ("level") table of the game: 200 outlines the player has to cover with the 7 pieces.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "adf.hpp"

namespace tg {

struct Level {
    int time = 0;                // basic time allowance (30..100)
    std::vector<int16_t> coords; // x0,y0,x1,y1,... relative to the board centre; last pair repeats/ends the outline
    int points() const { return int(coords.size() / 2); }
};

// File written by the exporter (see NOTATKI.md): "TGLV", u16 count, then per level u16 time, u16 n, n * i16 (big endian).
bool parseLevels(const Bytes& data, std::vector<Level>& out, std::string& err);
// Straight from the game's own TANGRAM.MLU (no exporter needed): the figure table is a raw 54 x 200 word array
// (row 0 = time, row 1 = vertex count, rows 2.. = coordinates; column = level) stored at file offset 64.
bool parseLevelsFromMlu(const Bytes& mlu, std::vector<Level>& out, std::string& err);
bool loadLevelsFile(const std::string& path, std::vector<Level>& out, std::string& err);

// Screen position of an outline vertex. The game draws the figure at (kCx + x, kCy - y) in red
// and the same outline shifted by (+5,+5) in black as its shadow.
constexpr int kCx = 114, kCy = 100, kShadow = 5;

}  // namespace tg
