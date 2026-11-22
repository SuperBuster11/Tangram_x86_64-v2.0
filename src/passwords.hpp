// Level passwords: the game keeps a table of 200 five-digit codes (level N shown after solving level N-1).
#pragma once
#include <string>
#include <vector>

namespace tg {

struct Passwords {
    std::vector<std::string> codes;                 // codes[i] is the password of level i+1
    bool load(const std::string& path, std::string& err);   // text file, one code per line
    // From TANGRAM.MLU: 1000 raw digit bytes (values 0-9) at offset 61662, five per level.
    bool loadFromMlu(const std::vector<unsigned char>& mlu, std::string& err);
    // 1-based level for a typed code, or 0 when the code is not in the table.
    int levelFor(std::string code) const;
    // Code that unlocks `level` (1-based); empty when out of range.
    std::string codeFor(int level) const;
};

}  // namespace tg
