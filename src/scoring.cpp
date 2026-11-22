#include "scoring.hpp"

namespace tg {

int startBonus(int level) { return 25 * level; }
int bonusPerStep(int level) { return level / 2 + 1; }
// Measured per stage on the original (write watch on E, all 200 stages; stage 200 assumed like 199). Almost always constant in
// blocks of 20 stages, with exceptions (stage 61 and 63 are 68, stage 62 is 64).
static const int kPeriod[200] = {
    76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76,
    72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
    68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68,
    68, 64, 68, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
    58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
    54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 24, 24,
};
int tickFrames(int level) { return kPeriod[level < 1 ? 0 : level > 200 ? 199 : level - 1]; }
int elapsedTicks(int frames, int level) { return frames / tickFrames(level); }

int stageGain(int level, int ticks) {
    int e = ticks, bonus = startBonus(level);
    while (e < kTimeLimit) {
        e += (e == 98) ? 1 : 2;
        bonus += bonusPerStep(level);
    }
    return bonus;
}

}  // namespace tg
