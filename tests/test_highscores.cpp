#include <cstdio>
#include <fstream>
#include <iterator>
#include "../src/highscores.hpp"
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "tests/data", err;
    std::ifstream f(dir + "/score.dta", std::ios::binary);
    tg::Bytes raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    CHECK(raw.size() == 516);
    tg::HighScores h;
    CHECK(h.parse(raw, err));
    CHECK(h.a.size() == 12 && h.b.size() == 12);
    CHECK(h.a[0].score == 1200000 && h.a[0].level == 150 && h.a[11].score == 200 && h.a[11].level == 2);
    CHECK(h.b[2].score == 450000 && h.a[0].name == "TANGRAM ");
    CHECK(h.serialize() == raw);   // byte-exact round trip of the original file
    CHECK(tg::HighScores::rankOf(h.a, 1) == -1);          // below the last entry
    CHECK(tg::HighScores::rankOf(h.a, 199) == -1);
    CHECK(tg::HighScores::rankOf(h.a, 200) == 11);        // ties go above equal entries (checked in the original)
    CHECK(tg::HighScores::rankOf(h.a, 201) == 11);
    CHECK(tg::HighScores::rankOf(h.a, 2000000) == 0);
    CHECK(tg::HighScores::insert(h.a, "ANNA", 7000, 8) == 10);
    CHECK(h.a.size() == 12 && h.a[10].name == "ANNA" && h.a[10].score == 7000 && h.a[11].score == 5000);
    tg::HighScores h2;
    CHECK(h2.parse(h.serialize(), err));
    CHECK(h2.a[10].score == 7000 && h2.a[10].level == 8);
    tg::Bytes bad(raw.begin(), raw.begin() + 100);
    CHECK(!h2.parse(bad, err));
    std::printf(failures ? "%d FAILURES\n" : "all highscore tests passed\n", failures);
    return failures ? 1 : 0;
}
