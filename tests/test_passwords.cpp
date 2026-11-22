#include <cstdio>
#include <set>
#include "../src/passwords.hpp"
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "tests/data", err;
    tg::Passwords p;
    CHECK(p.load(dir + "/passwords.txt", err));
    CHECK(p.codes.size() == 200);
    std::set<std::string> uniq(p.codes.begin(), p.codes.end());
    CHECK(uniq.size() == 200);
    // shown by the real game after solving levels 1, 2, 3 (as the passwords of levels 2, 3, 4)
    CHECK(p.codeFor(2) == "03797" && p.codeFor(3) == "58829" && p.codeFor(4) == "73159");
    CHECK(p.levelFor("73159") == 4 && p.levelFor("56885") == 1 && p.levelFor("00000") == 0);
    CHECK(p.codeFor(0).empty() && p.codeFor(201).empty());
    std::printf(failures ? "%d FAILURES\n" : "all password tests passed\n", failures);
    return failures ? 1 : 0;
}
