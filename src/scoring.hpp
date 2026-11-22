// Score at the end of a stage, reconstructed from the game's end-of-stage loop (machine code at 0x103120..0x1032a0
// of the running program, observed with write watches on its variables).
#pragma once

namespace tg {

constexpr int kTimeLimit = 101;   // the end-of-stage loop runs until E >= 101

int startBonus(int level);        // value of the bonus counter when a stage begins: 25 * level (levels 1..4 observed)
int bonusPerStep(int level);      // added per loop step: x/2 + 1, with x = level (levels 1..3 observed)
// Frames (50 Hz) between two steps of the elapsed-time counter E; it depends on the stage (table measured on the original):
// 76 for stages 1-20, 72 for 21-40, 68, 64, 61, 58, 54, 50 (141-160), 38 (161-180), 31 (181-198), 24 for 199 and 200.
// The same period paces the bonus count-down after the time is up.
int tickFrames(int level);
int elapsedTicks(int frames, int level);

// Points gained for solving a stage when the elapsed-time counter E has the value `ticks` at the moment of completion.
// The loop: while (E < 101) { E += (E == 98) ? 1 : 2; bonus += bonusPerStep }.
int stageGain(int level, int ticks);

}  // namespace tg
