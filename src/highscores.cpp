#include "highscores.hpp"

#include <cstdio>
#include <cstdlib>

namespace tg {
namespace {

void putStr(Bytes& out, const std::string& s) {
    out.push_back(uint8_t(s.size() >> 8));
    out.push_back(uint8_t(s.size()));
    out.insert(out.end(), s.begin(), s.end());
}

std::string number(long v, int width) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%*ld", width, v);
    return buf;
}

}  // namespace

std::string padName(const std::string& name) {
    std::string s = name.substr(0, 8);
    s.resize(8, ' ');
    return s;
}

bool HighScores::parse(const Bytes& data, std::string& err) {
    std::vector<std::string> strs;
    size_t i = 0;
    while (i < data.size()) {
        if (i + 2 > data.size()) { err = "truncated length"; return false; }
        size_t n = (size_t(data[i]) << 8) | data[i + 1];
        i += 2;
        if (i + n > data.size()) { err = "truncated string"; return false; }
        strs.emplace_back(reinterpret_cast<const char*>(&data[i]), n);
        i += n;
    }
    if (strs.size() != size_t(kEntries * 5)) { err = "expected 60 strings"; return false; }
    auto num = [&](const std::string& s, long& v) {
        char* end = nullptr;
        v = std::strtol(s.c_str(), &end, 10);
        return end != s.c_str();
    };
    a.clear(); b.clear();
    size_t k = 0;
    for (int e = 0; e < kEntries; e++, k += 3) {
        ScoreEntry en; long lv = 0;
        en.name = strs[k];
        if (!num(strs[k + 1], en.score) || !num(strs[k + 2], lv)) { err = "bad number in table A"; return false; }
        en.level = int(lv);
        a.push_back(en);
    }
    for (int e = 0; e < kEntries; e++, k += 2) {
        ScoreEntry en;
        en.name = strs[k];
        if (!num(strs[k + 1], en.score)) { err = "bad number in table B"; return false; }
        b.push_back(en);
    }
    return true;
}

Bytes HighScores::serialize() const {
    Bytes out;
    for (const auto& e : a) { putStr(out, e.name); putStr(out, number(e.score, 7)); putStr(out, number(e.level, 3)); }
    for (const auto& e : b) { putStr(out, e.name); putStr(out, number(e.score, 7)); }
    return out;
}

int HighScores::rankOf(const std::vector<ScoreEntry>& t, long score) {
    for (size_t i = 0; i < t.size(); i++)
        if (score >= t[i].score) return int(i);
    return -1;
}

int HighScores::insert(std::vector<ScoreEntry>& t, const std::string& name, long score, int level) {
    int r = rankOf(t, score);
    if (r < 0) return -1;
    ScoreEntry e; e.name = name.substr(0, 8); e.score = score; e.level = level;
    t.insert(t.begin() + r, e);
    t.pop_back();
    return r;
}

}  // namespace tg
