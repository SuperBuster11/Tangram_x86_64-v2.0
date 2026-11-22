#include "passwords.hpp"

#include <fstream>

namespace tg {

bool Passwords::load(const std::string& path, std::string& err) {
    std::ifstream f(path);
    if (!f) { err = "cannot open " + path; return false; }
    codes.clear();
    std::string line;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty()) continue;
        if (line.size() != 5 || line.find_first_not_of("0123456789") != std::string::npos) {
            err = "bad password line: " + line;
            return false;
        }
        codes.push_back(line);
    }
    if (codes.size() != 200) { err = "expected 200 passwords"; return false; }
    return true;
}

bool Passwords::loadFromMlu(const std::vector<unsigned char>& mlu, std::string& err) {
    constexpr size_t kOffset = 61662, kCount = 200;
    if (mlu.size() < kOffset + kCount * 5) { err = "TANGRAM.MLU too short for the password table"; return false; }
    codes.clear();
    for (size_t i = 0; i < kCount; i++) {
        std::string s;
        for (size_t k = 0; k < 5; k++) {
            unsigned char v = mlu[kOffset + 5 * i + k];
            if (v > 9) { err = "bad digit in password table"; return false; }
            s += char('0' + v);
        }
        codes.push_back(s);
    }
    return true;
}

// Codes from the player's own list that differ from the game's table in one digit (typos in the list, most likely);
// accepted as well so that a code copied from that list always works.
static const struct { int level; const char* code; } kAlternatives[] = {
    {6, "22585"},
    {8, "50037"},
    {15, "85720"},
    {24, "85239"},
    {25, "72851"},
    {31, "51522"},
    {32, "58875"},
    {35, "36388"},
    {43, "50897"},
    {52, "38599"},
    {58, "91998"},
    {141, "95862"},
    {149, "44059"},
};

int Passwords::levelFor(std::string code) const {
    // A code typed without its leading zeros (the published lists give 3797, 7274, 8439 ...) means the same number.
    if (!code.empty() && code.size() < 5) code.insert(0, 5 - code.size(), '0');
    for (size_t i = 0; i < codes.size(); i++)
        if (codes[i] == code) return int(i) + 1;
    for (const auto& a : kAlternatives)
        if (code == a.code && a.level <= int(codes.size())) return a.level;
    return 0;
}

std::string Passwords::codeFor(int level) const {
    if (level < 1 || level > int(codes.size())) return std::string();
    return codes[level - 1];
}

}  // namespace tg
