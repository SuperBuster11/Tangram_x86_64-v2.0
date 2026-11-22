// Checks against values observed in the real game (see tests/data/game_observations.txt and NOTATKI.md, etap 6).
#include <cstdio>
#include "../src/scoring.hpp"
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
int main() {
    // level 1: counter E = 22 at completion -> 40 loop steps (39 with +2, one +1 at E == 98) -> 25 + 40 = 65 (game: 65)
    CHECK(tg::stageGain(1, 22) == 65);
    // level 1: E = 35 -> 33 steps -> 58 (game, similar run: 59; E was not captured exactly in that run)
    CHECK(tg::stageGain(1, 35) == 58);
    CHECK(tg::stageGain(1, 34) == 59);
    // level 2: base 50, +2 per step, 40 steps -> 130 (game: 130)
    CHECK(tg::stageGain(2, 22) == 130);
    // level 3: base 75, +2 per step, 39 steps -> 153 (game: 153)
    CHECK(tg::stageGain(3, 24) == 153);
    CHECK(tg::stageGain(1, 101) == 25 && tg::stageGain(1, 120) == 25);
    CHECK(tg::elapsedTicks(150, 1) == 1);
    CHECK(tg::tickFrames(1) == 76 && tg::tickFrames(20) == 76 && tg::tickFrames(21) == 72 && tg::tickFrames(100) == 61 && tg::tickFrames(62) == 64 && tg::tickFrames(63) == 68);
    CHECK(tg::tickFrames(160) == 50 && tg::tickFrames(161) == 38 && tg::tickFrames(198) == 31 && tg::tickFrames(199) == 24 && tg::tickFrames(200) == 24);
    std::printf(failures ? "%d FAILURES\n" : "all scoring tests passed\n", failures);
    return failures ? 1 : 0;
}
