// Screen positions of the texts the original draws (top-left of the first glyph; read from the running game).
#pragma once

namespace tg {

struct TextPos { int x, y; };

// play screen, right-hand panel (font record 6)
constexpr TextPos kScoreRow = {234, 119};   // 7 digits, e.g. 0000065
constexpr TextPos kBonusRow = {234, 133};   // 7 digits, counts down while it is added to the score
constexpr TextPos kTimeRow = {263, 174};    // 3 digits, starts at 100
constexpr TextPos kLevelLabel = {236, 189}; // 3 digits, e.g. 001
constexpr TextPos kPlayerNo = {310, 189};   // the player number after the baked-in "PLAYER" (pixel-exact)
// password panel shown after a stage (font record 6)
constexpr TextPos kPwPassword = {26, 91}, kPwFor = {98, 91}, kPwLevel = {130, 91}, kPwLevelNo = {178, 91}, kPwCode = {94, 103};
// credits on the start screen (font record 5), as drawn by the game
constexpr TextPos kCreditDesign = {237, 88}, kCreditMutschall = {225, 98}, kCreditMusic = {241, 111}, kCreditHippel = {237, 121},
                  kCreditAmiga = {241, 134}, kCreditVersion = {233, 142}, kCreditBehrends = {229, 152}, kCreditMaack = {241, 160};

}  // namespace tg

// Pictures from TANGRAM.DAT whose screen position was confirmed pixel for pixel against the running game.
namespace tg {
constexpr int kDatNovice = 0, kDatExpert = 1, kDatOnePlayer = 2, kDatTwoPlayer = 3, kDatMenuPanel = 4, kDatPaused = 7,
              kDatCursor = 11, kDatTimeBar = 83;
constexpr TextPos kMenuPanel = {211, 62};       // record 4 over TITLEPIC.LO5 (title palette); best match of a position scan
constexpr TextPos kMenuPasswordLabel = {233, 151};   // 'PASSWORD' lettering on the menu scroll (ink only)
constexpr TextPos kMenuPasswordCode = {242, 162};    // typed code: pixel-identical to the original (checked with 12345)
constexpr int kPwDigitPitch = 8;
constexpr TextPos kMenuTwoPlayer = {231, 129};  // ink-only lettering of record 3 matched pixel for pixel
constexpr TextPos kMenuNovice = {231, 83};      // menu screen, title palette
constexpr TextPos kMenuOnePlayer = {231, 116};  // menu screen, title palette
constexpr TextPos kMenuExpert = {231, 96};      // menu screen (the 2PLAYER label, record 3, was not matched yet)
constexpr TextPos kPausePanel = {12, 67};       // record 7 "GAME PAUSED", play palette
constexpr int kDatYes = 8, kDatNo = 9;
constexpr TextPos kYesPos = {85, 105}, kNoPos = {117, 105};   // records 8 and 9 on the "PLAY WITH SCOREDISK" panel
constexpr int kDatMenuCursor = 10;              // 20x24 pointer on the menu screen (title palette), drawn at mouse + kCursorOffset
constexpr TextPos kCursorOffset = {-2, 0};      // the 20x24 pointer is drawn at mouse position + this offset (seen at mouse 90,130)
constexpr TextPos kTimeBarPos = {249, 162};     // 52x8 bar, partly filled (93% of its pixels matched the start screen)
}  // namespace tg
