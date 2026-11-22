// High-score file SCORE.DTA (516 bytes): a sequence of strings, each stored as u16 big-endian length + bytes.
// Table A: 12 x (name[8], score[7], level[3]); table B: 12 x (name[8], score[7]).
// A typed name is stored as typed (up to 8 characters); numbers are right aligned with blanks (as in the original file).
#pragma once
#include <string>
#include <vector>

#include "adf.hpp"

namespace tg {

struct ScoreEntry {
    std::string name;   // as typed (up to 8 characters, not padded; the default entries are "TANGRAM ")
    long score = 0;
    int level = 0;      // only used by table A
};

class HighScores {
public:
    static constexpr int kEntries = 12;
    std::vector<ScoreEntry> a, b;   // each kEntries long, best first

    bool parse(const Bytes& data, std::string& err);
    Bytes serialize() const;

    // Rank (0-based) a score would take in the table, or -1 when it does not make it. Ties go ABOVE equal entries (checked in the original).
    static int rankOf(const std::vector<ScoreEntry>& t, long score);
    // Inserts and drops the last entry; returns the rank or -1.
    static int insert(std::vector<ScoreEntry>& t, const std::string& name, long score, int level);
};

std::string padName(const std::string& name);   // upper-case not applied; padded/truncated to 8

}  // namespace tg
