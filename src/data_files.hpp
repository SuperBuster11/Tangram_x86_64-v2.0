// The game data as separate files in the "data" folder next to the program (v2.0; until v1 they were compiled into the exe).
#pragma once
#include <string>

#include "adf.hpp"

namespace tg {

// The eight files the game reads from the original disk.
extern const char* const kDataFiles[8];   // TANGRAM.DAT, TANGRAM.MLU, TITLEPIC.LO5, THALION.LO5, MUSIC1.IMG, MUSIC2.IMG, HIGHSCOREPIC.LO5, SCORE.DTA

// Reads them from `dir` (ending with a slash) into `adf`; false and the missing names in `err` when any cannot be read.
bool loadDataFolder(Adf& adf, const std::string& dir, std::string& err);

// The data folder: "data/" next to the program if TANGRAM.DAT is there, otherwise "data/" in the current folder.
std::string findDataFolder(const std::string& exeDir);

}  // namespace tg
