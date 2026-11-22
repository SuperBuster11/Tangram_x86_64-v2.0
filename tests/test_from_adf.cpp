// test_from_adf <game.adf> <tests/data>: levels and passwords read straight from the disk image must equal the
// tables exported earlier from the running original (levels.bin, passwords.txt).
#include <cstdio>
#include <cstdlib>
#include "../src/adf.hpp"
#include "../src/levels.hpp"
#include "../src/passwords.hpp"
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
int main(int argc, char** argv) {
    const char* adfPath = argc >= 2 && argv[1][0] ? argv[1] : std::getenv("TANGRAM_ADF");
    std::string dir = argc >= 3 ? argv[2] : "tests/data", err;
    if (!adfPath) { std::printf("SKIP: set TANGRAM_ADF\n"); return 0; }
    tg::Adf adf;
    CHECK(adf.loadFile(adfPath, err));
    const tg::Bytes* mlu = adf.file("TANGRAM.MLU");
    CHECK(mlu != nullptr);
    if (!mlu) return 1;
    std::vector<tg::Level> a, b;
    CHECK(tg::parseLevelsFromMlu(*mlu, a, err));
    CHECK(tg::loadLevelsFile(dir + "/levels.bin", b, err));
    CHECK(a.size() == 200 && b.size() == 200);
    int diff = 0;
    for (size_t i = 0; i < a.size() && i < b.size(); i++) diff += (a[i].time != b[i].time || a[i].coords != b[i].coords);
    std::printf("levels differing from the exported table: %d\n", diff);
    CHECK(diff == 0);
    tg::Passwords p, q;
    CHECK(p.loadFromMlu(*mlu, err));
    CHECK(q.load(dir + "/passwords.txt", err));
    CHECK(p.codes == q.codes);
    CHECK(p.codeFor(2) == "03797");
    std::printf(failures ? "%d FAILURES\n" : "all from-ADF tests passed\n", failures);
    return failures ? 1 : 0;
}
