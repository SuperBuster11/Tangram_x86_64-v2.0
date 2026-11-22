// Platform-independent game: menu, play screen, password screen. 320x200 frame, 50 Hz steps.
// Input arrives as Events (mouse in game pixels); the window layers (X11 / Win32) only translate and scale.
#pragma once
#include <array>
#include <random>
#include <string>
#include <vector>

#include "adf.hpp"
#include "board.hpp"
#include "dat.hpp"
#include "image.hpp"
#include "scale_filter.hpp"
#include "levels.hpp"
#include "scoring.hpp"
#include "music.hpp"
#include "highscores.hpp"
#include "passwords.hpp"

namespace tg {

struct Event {
    enum Type { Move, Down, Up, Key } type = Move;
    int x = 0, y = 0;     // mouse position (Move/Down/Up)
    int button = 0;       // 0 left, 1 right
    int key = 0;          // Key: ASCII ('0'..'9', 'A'..'Z'), or kKeyEnter / kKeyEsc
};
constexpr int kKeyEnter = 13, kKeyEsc = 27, kKeyBackspace = 8;
constexpr int kKeyF1 = 0x101;   // F1..F10 = kKeyF1 .. kKeyF1 + 9

class Game {
public:
    enum class Mode { Intro, Menu, Play, Password, Bonus, AskDisk, HighScores, NameEntry, AskNew, AskFormat, AskReplay };
    bool init(const Adf& adf, std::string& err, bool intro = true);
    void event(const Event& e);
    void step();                          // one 50 Hz frame
    const Image& frame() const { return frame_; }
    // The frame enlarged n times for the window (F8 filter).
    void scaleFrame(uint32_t* dst, int n, ScaleFilter mode) const { tg::scaleFrame(frame_, dst, n, mode); }
    void scaleFrame(uint32_t* dst, int w, int h, ScaleFilter mode) const { tg::scaleFrame(frame_, dst, w, h, mode); }   // any size (fullscreen)
    // Renders `frames` stereo frames (interleaved int16, 44100 Hz) of the current music: song 1 (continuously). Call from the audio thread/loop of the window layer; the game itself never needs it.
    void audio(int16_t* out, int frames);
    // Replaces song 1 or 2 by a recording (RIFF WAV, 16-bit PCM, mono or stereo, any rate; converted to 44100 Hz stereo). The recording
    // is played from its start whenever the emulated song would start, and repeats from the start at its end; volume as the
    // emulated song (25%). Without a recording the original music player is emulated. Returns false (and leaves the song) on a bad file.
    bool loadMusicWav(int song, const std::vector<uint8_t>& wav, std::string& err);
    // time, bonus counter and score as shown on the panel in the last rendered frame (they differ from the real ones during the count-down)
    std::array<int, 3> shownNumbers() const { return {dTime_, dBonus_, dScore_}; }
    bool musicFromWav(int song) const { return song >= 1 && song <= 2 && !wav_[song - 1].empty(); }
    int song() const { return song_; }
    bool musicMuted() const { return !musicOn_; }       // M (during a stage): song stopped; M again: started from the beginning
    int musicSong() const { return songSel_; }          // song chosen with F2 (1 at start-up; the original never changes it by itself)

    // High scores (the original keeps them on a "scoredisk"; here in a small file managed by the window layer):
    const HighScores& highScores() const { return scores_; }
    void loadScores(const Bytes& data);                 // replaces the table (ignored when the data is not a valid table)
    Bytes saveScores() const { return scores_.serialize(); }
    bool takeScoresDirty() { bool d = scoresDirty_; scoresDirty_ = false; return d; }   // true once after the table changed
    // Test hook: shows the table with a name being typed (as after a finished game). expert = table B.
    void showNameEntryForTest(bool expert, int level, long score, const std::string& typed);
    bool quitRequested() const { return quit_; }       // Esc on the menu screen: the window layer should close the game
    Mode mode() const { return mode_; }
    int level() const { return level_; }
    bool expertMode() const { return expert_; }
    int player() const { return player_ + 1; }          // 1 or 2 (two-player mode: the players take turns on the shared stage sequence)
    int scoreOf(int player) const { return player == player_ + 1 ? score_ : playerScore_[player - 1]; }              // 1-based
    int score() const { return score_; }
    int heldPiece() const { return held_ ? int(held_->type) : -1; }
    int heldRotation() const { return held_ ? held_->rot : -1; }
    size_t placedCount() const { return placed_.size(); }
    int frames() const { return frames_; }
    int stageTick() const { return stageTick_; }
    void setScoreForTest(int s) { score_ = s; }   // frames per clock tick of the current stage
    void bonusTile(int i, int& x, int& y) const { x = 25 + 18 * (i % 10); y = 10 + 18 * (i / 10); }   // top-left corner of cell i (row*10+col, 0..99) on the bonus screen
    void setNextLevelForTest(int level) { nextLevel_ = level; }   // Expert picks the next stage at random; tests pin it
    void setStagesDoneForTest(int n) { stagesDone_[player_] = n; }   // stages already completed by the current player in this game
    void startBonusStage(int nextLevel);  // the bonus stage of the original (0x104c52)
    int bonusPairsLeft() const { return bonusLeft_ / 2; }
    int bonusPoints() const { return bonusPoints_; }
    int bonusIcon(int cell) const { return bonusGrid_[cell]; }   // 0 = empty, else sprite 1..50 (DAT record 32 + n)
    bool bonusFree(int cell) const;       // a tile that may be picked: on the edge of the 10x10 grid or next to an empty cell
    int bonusLayout() const { return bonusLayout_; }   // DAT record of the 10x10 layout mask in use (13..32)
    void startLevel(int level);           // also used by the password entry
    int introStage() const { return introStage_; }
    std::string passwordShown() const { return pwShown_; }
    int transitionFrame() const { return pwFrame_; }
    int closeFrame() const { return closeFrame_; }
    bool transitionBusy() const { return mode_ == Mode::Password && (pwFrame_ < kPwOpenStart + 6 * kPwOpenSteps || closeFrame_ >= 0); }

private:
    struct Held { PieceType type; int rot; int inst; };
    void render();
    void drawScene();
    void drawPanelOverlay();
    void drawMenuPanel(bool coverBoxes);
    void trayClick(int x, int y);
    void drop(int x, int y);
    void checkSolved();
    int remaining(PieceType t) const;
    bool pickPlaced(int x, int y);
    void drawPiece(PieceType t, int rot, int inst, int x, int y);
    void text(const DatPicture& font, int x, int y, const std::string& s, const Palette& pal);
    void fillPoly(const std::vector<Pt>& poly, Rgb colour);

    std::vector<DatPicture> pics_;
    Palette title_, play_;
    Image boardMenu_, boardPlay_, frame_;
    std::vector<Level> levels_;
    Passwords passwords_;
    Rgb silhouette_{}, black_{}, pieceColour_[7]{};   // 7 pieces: large, large, square, medium, small, small, parallelogram

    Mode mode_ = Mode::Menu;
    int level_ = 1, score_ = 0, frames_ = 0, mx_ = 160, my_ = 100;
    std::vector<Placed> placed_;
    std::vector<int> inst_;     // instance number (0/1) of each placed piece within its type; decides its colour
    bool holding_ = false;
    Held held_storage_{};
    const Held* held_ = nullptr;
    std::string typed_, pwShown_;
    Music music_[2];
    std::vector<uint8_t> musicImage_[2];
    int song_ = 0, songSel_ = 1, musicGen_ = 0, songGen_ = 0;
    std::vector<int16_t> wav_[2];   // recordings replacing the songs (interleaved stereo, 44100 Hz)
    size_t wavPos_ = 0;             // next frame of the playing recording
    bool musicOn_ = true;
    bool muted_ = false, quit_ = false, scoresDirty_ = false;
    // End of a game (Esc during a stage or the bonus stage, or the time running out): "PLAY WITH SCOREDISK" (YES / NO, no way back).
    // NO: the table (A for novice, B for expert: names and points only) is only shown. YES: "CREATE NEW SCORE DISK" -
    //   NO: the existing table is used; YES: "INSERT FORMATTED DISK" (YES: a fresh table is created; NO does nothing, as in the original).
    // A score that makes the table (ties rank above equal entries) is typed in place on its row (up to 8 characters, no time limit, Return
    // accepts, an empty name is allowed). The table is shown for about 6 s (a click skips it), fades out and the menu returns.
    // Finishing level 200 does the same as YES.
    HighScores scores_, defaults_;
    Image hsPic_; Palette hsPal_;
    int hsFrame_ = 0, entryRank_ = -1, hsLeave_ = -1;
    bool timedOut_ = false, askFromBonus_ = false;
    // 0x106a30: each scoredisk question fades in (0x104a74) and, once answered, fades out (0x1048cc) before what follows
    int askIn_ = 1 << 20, askOut_ = -1, askAction_ = 0;   // frames since the question appeared / since it was answered
    void askAnswered(int action);
    int stageTick_ = 76, expertX_ = 0;   // frames per clock tick of the current stage; Expert speed-up (never reset during a session)
    bool tablePeriodOnce_ = false;
    static constexpr int kPauseFxFrames = 46;   // pause panel blended in / out (effect 10 of the original)
    int pauseFx_ = -1, panelFx_ = -1;
    int bonusBase_ = 25;   // bonus counter at the start of the stage: 25 x stage, 0 when the stage was bought again
    void leaveTwoPlayer();
    void drawPointer(const DatPicture& pic, const Palette& pal);   // the mouse pointer with its shadow
    std::vector<size_t> drawOrder() const;   // placed pieces in the order they are drawn (slot order of the original)
    bool fitsBoard(int x, int y) const;      // the held piece may be put down at (x, y)
    void gameOver(bool fromBonus);   // end of a game: score disk question (first time only), then the table
    bool askedDisk_ = false, useDisk_ = false, pendingContinue_ = false;
    int pendingEntry_ = -1;   // two players: the player who enters his name after the first one
    int stagesDone_[2]{};   // stages completed by each player in this game (the bonus stage comes after every 10th)
    bool bonusDue_ = false;
    int toFrame_ = -1;   // frames since the time ran out (bonus count-down), -1 = not running
    int timeoutBlank() const { return (bonusBase_ > 0 ? (bonusBase_ / level_ - 1) * stageTick_ : 0) + 6; }   // the 25th step (bonus 0) comes after 24 ticks of the stage; then a short blank frame
    int timeoutEnd() const { return timeoutBlank() + 12; }
    std::string entryName_;
    static constexpr int kHsFrames = 298, kHsBlack = 25, kHsFadeIn = 4, kHsFadeOut = 8, kHsGap = 14;
    void endGame(bool withScoreDisk);
    void leaveScores() { if (hsLeave_ < 0) hsLeave_ = 0; }
    void drawHighScores();
    std::vector<ScoreEntry>& table() { return expert_ ? scores_.b : scores_.a; }
    // Song 1 plays without a break from the logo through the menu and all levels (checked against the reference port's register
    // trace). Song 2 is loaded by the original too but is not used there; the user says it belongs to the bonus stage (not implemented).
    // the original plays the chosen song everywhere (0x104c52 never touches the music); by the user's wish the bonus stage plays song 2
    int wantedSong() const { return !musicOn_ ? 0 : mode_ == Mode::Bonus ? 2 : songSel_; }
    Image thalion_;
    int introStage_ = 0, introFrame_ = 0, menuFade_ = 0;
    int startFade_ = -1;       // menu -> game: fade-out of the menu (frames), -1 = not running
    int fadeIn_ = 1000;        // frames since a new scene (first stage after the menu, bonus stage, stage after it, Expert after stage 200) began fading in from black
    bool paused_ = false, expert_ = false, twoPlayers_ = false;
    bool beyond_ = false;   // Novice game that has solved stage 200: continues like Expert (random stages, label EXP, wiped in from the left)
    // Bonus stage (after every 10th completed stage), from the code of the original (0x104c52..0x105e78, checked in the simulator):
    // - layout: 10x10 cells of 18 px, cell (row r, col c) tile at (25+18c, 10+18r); the k-th bonus stage of the player in this game
    //   (k = 1..20, stays 20 after that) uses the 10x10 picture DAT 12+k: a pixel of colour 15 = no tile there. The other cells get
    //   avail/2 pairs, each pair a random sprite 1..50, both tiles in random free cells.
    // - each tile: black 1-px frame around it, black 18x18 shadow at +4,+4, then the sprite.
    // - a click picks the tile under the mouse (column (x-6)/18, row (y+9)/18) only if it is on the edge of the grid or has an empty
    //   neighbour (left/right/up/down); it gets a red frame on its outer pixels. Second pick: same tile -> both frames go; same sprite ->
    //   both tiles vanish and the bonus counter gets stage+10 points; otherwise the frames go.
    // - the clock ticks every 200/4+1 = 51 frames (-$7b62 = 200, 20 less per bonus stage after the 20th); the stage ends when all
    //   tiles are gone or after 101 ticks. Then the remaining time runs into the bonus counter like after a stage (2 ticks per step,
    //   stage/2+1 points per step) and the counter goes to the score when the next stage starts.
    static constexpr int kBonusCells = 100, kBonusShadow = 4;
    int bonusGrid_[100]{}, bonusLeft_ = 0, bonusLayout_ = 13, bonusStages_[2]{}, bonusUnits_ = 200, bonusE_ = 0;
    int bonusSel_ = -1, bonusFrame_ = 0, bonusNext_ = 1, bonusPoints_ = 0, bonusEnd_ = -1, toBonusFade_ = -1, bonusPhase_ = 0, bonusTime_ = 100;
    int bonusTickFrames() const { return bonusUnits_ / 4 + 1; }
    // phases: 0 playing, 1 remaining time runs into the bonus counter, 3 fade out
    void bonusClick(int x, int y);
    // Two players: both play every stage (P1, then P2 on the same stage, then the next stage for P1; the password panel follows player 2), each keeps an
    // own score, the panel shows PLAYER1/PLAYER2 and the current player's score; each player has an own count of completed stages (bonus stage after his 10th, as in the code of the original; not seen in a real 2-player game); the end of the game (Esc / time) offers the
    // current player's score to the table.
    int player_ = 0, playerScore_[2] = {0, 0}, previewPlayer_ = -1;
    bool wipeMode() const { return expert_ || beyond_; }   // no password panel, no click: the next scene is wiped in from the left
    void nextStage();
    void startNewGame();
    int randomLevel();
    std::mt19937 rng_{std::random_device{}()};
    int gain_ = 0, scoreBefore_ = 0, timeAtEnd_ = 0;
    int dCounts_[kPieceTypes] = {-1, -1, -1, -1, -1}, dFill_ = -1;   // refill of the panel while a new stage is wiped in: shown piece counts (-1 = real) and time-bar columns (-1 = real)
    void refillPanel(int T, int fullBonus);
    int dScore_ = 0, dBonus_ = 0, dTime_ = 0, dE_ = -1;
    static constexpr int kRotDelay = 12, kRotRepeat = 6;   // right button held: repeat after 12 frames, then every 6 frames
    bool rmbHeld_ = false; int rmbFrames_ = 0;       // numbers shown by drawScene (differ from the real ones during the count-down)
    // Stage-complete transition (measured on the original, 50 Hz frames after the last piece): ~156 frames of bonus count-down,
    // then the password panel appears through vertical blinds (slat widths below, 6 frames each), then waits for a click and
    // closes the same way while the next stage shows through.
    int pwFrame_ = 0, closeFrame_ = -1, nextLevel_ = 1;
    // 0x1007e8: the next stage replaces the whole screen with one of the 11 effects of 0x10d074 (chosen at random, RND(11)+1) at
    // delay 5000; closeFrame_ counts its frames (Novice after the click on the password panel), the wipe of Expert / two players too
    int transFx_ = 1;
public:
    static int transitionFrames(int fx);                        // length of effect fx (1..11) in 50 Hz frames
    static bool transitionShowsNew(int fx, int x, int y, int frame);   // pixel (x,y) already shows the new picture at that frame
    int transitionEffect() const { return transFx_; }
    void setTransitionEffectForTest(int fx) { transFx_ = fx; forcedFx_ = fx; }
private:
    int forcedFx_ = 0;
    void pickTransition() { transFx_ = forcedFx_ ? forcedFx_ : 1 + int(rng_() % 11); }
    static constexpr int kXWipeStart = 225, kXWipeFrames = 300, kXWipeX0 = 22;   // Expert wipe (measured): starts 225 frames after the last piece, 300 frames long
    static constexpr int kStripY0 = 62, kStripY1 = 143;   // rows covered by the blinds (measured)
    static constexpr int kPwCount = 156, kPwOpenStart = 162, kPwOpenSteps = 8, kPwCloseSteps = 8;
    static constexpr int kOpenWidths[8] = {2, 4, 6, 8, 10, 12, 14, 16};
    static constexpr int kCloseWidths[8] = {14, 12, 10, 8, 6, 4, 2, 0};
};

}  // namespace tg
