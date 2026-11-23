#include "game.hpp"

#include <cstring>
#include "hs_levelcover.hpp"
#include <random>
#include "password_label.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "assets.hpp"
#include "copyright.hpp"
#include "panel_texture.hpp"
#include "font.hpp"
#include "layout.hpp"
#include "raster.hpp"
#include "scoring.hpp"
#include "sprite.hpp"

namespace tg {
namespace {

// Tray positions of the five piece types (screen pixels; measured in the running game).
// 0x101d78: pick-up areas of the tray pieces (x0, y0, x1, y1 inclusive; the original tests x > a AND x < b, y > c AND y < d), and the
// pixel under the pointer must have colour 0 (outlines, the count digits) or 20..24 (the grey pieces): the parchment does nothing
const int kTrayBox[kPieceTypes][4] = {{234, 5, 314, 45}, {234, 47, 274, 87}, {274, 47, 314, 87}, {234, 89, 274, 109}, {294, 89, 314, 149}};
const Pt kTray[kPieceTypes] = {{262, 22}, {255, 72}, {296, 70}, {258, 102}, {305, 104}};
// Start-up sequence (50 Hz frames): Thalion logo, title picture, credits, then the menu fades in.
// Hold times follow the Go port (2 x 2.2 s per picture, 4.5 s for the credits); each picture fades in and out.
constexpr int kFadeFrames = 25, kIntroHold[3] = {220, 220, 225};
constexpr int kBoardRight = 226;   // the figure area ends here; the right panel starts

Rgb nearest(const Palette& p, Rgb want) {
    int best = 1 << 30; Rgb r = p.c[0];
    for (const Rgb& c : p.c) {
        int d = (c.r - want.r) * (c.r - want.r) + (c.g - want.g) * (c.g - want.g) + (c.b - want.b) * (c.b - want.b);
        if (d < best) { best = d; r = c; }
    }
    return r;
}

std::string pad(int v, int w) {
    char b[16]; std::snprintf(b, sizeof b, "%0*d", w, v);
    return b;
}

}  // namespace

bool Game::init(const Adf& adf, std::string& err, bool intro) {
    const Bytes* dat = adf.file("TANGRAM.DAT");
    const Bytes* mlu = adf.file("TANGRAM.MLU");
    if (!dat || !mlu) { err = "TANGRAM.DAT / TANGRAM.MLU missing on the disk"; return false; }
    if (!parseDat(*dat, pics_, err) || pics_.size() < 85) { if (err.empty()) err = "bad TANGRAM.DAT"; return false; }
    title_ = mluTitlePalette(*mlu);
    if (!mluBoardPalette(*mlu, 0, play_, err)) return false;
    // the menu uses the title palette over the same picture; the play screen uses board palette 0
    {
        Image tmp;
        if (!mluBoardPicture(*mlu, 0, boardPlay_, err)) return false;
        const Bytes* tp = adf.file("TITLEPIC.LO5");
        if (!tp) { err = "TITLEPIC.LO5 missing on the disk"; return false; }
        if (!decodeLo5(*tp, title_, boardMenu_, err)) return false;   // menu background: the title picture
    }
    if (!parseLevelsFromMlu(*mlu, levels_, err)) return false;
    if (!passwords_.loadFromMlu(std::vector<unsigned char>(mlu->begin(), mlu->end()), err)) return false;
    silhouette_ = nearest(play_, {136, 34, 0});
    black_ = nearest(play_, {0, 0, 0});
    // flat piece colours sampled from the running game (a dark-to-light ramp over the 7 pieces)
    const Rgb want[7] = {{0, 0, 68}, {34, 34, 102}, {68, 34, 136}, {102, 68, 170}, {136, 102, 204}, {170, 136, 238}, {204, 170, 238}};
    for (int i = 0; i < 7; i++) pieceColour_[i] = nearest(play_, want[i]);
    for (int i = 0; i < 2; i++) {
        const Bytes* m = adf.file(i ? "MUSIC2.IMG" : "MUSIC1.IMG");
        if (!m) { err = "MUSIC image missing on the disk"; return false; }
        musicImage_[i] = *m;
        if (!music_[i].load(musicImage_[i], err)) return false;
    }
    {
        const Bytes* th = adf.file("THALION.LO5");
        if (!th || th->size() < 40064) { err = "THALION.LO5 missing on the disk"; return false; }
        if (!decodeLo5(*th, readPalette(th->data() + 40000), thalion_, err)) return false;   // the palette is stored after the planes
    }
    {   // high-score screen: its own picture drawn with board palette 1, default table from SCORE.DTA
        const Bytes* hp = adf.file("HIGHSCOREPIC.LO5");
        const Bytes* sd = adf.file("SCORE.DTA");
        if (!hp || !sd) { err = "HIGHSCOREPIC.LO5 / SCORE.DTA missing on the disk"; return false; }
        if (!mluBoardPalette(*mlu, 1, hsPal_, err) || !decodeLo5(*hp, hsPal_, hsPic_, err) || !scores_.parse(*sd, err)) return false;
        defaults_ = scores_;
    }
    frame_ = boardMenu_;
    mode_ = intro ? Mode::Intro : Mode::Menu;
    introStage_ = 0; introFrame_ = 0; menuFade_ = intro ? 0 : kFadeFrames;
    render();
    return true;
}

void Game::loadScores(const Bytes& data) {
    HighScores tmp; std::string err;
    if (tmp.parse(data, err)) scores_ = tmp;
}

void Game::endGame(bool withScoreDisk) {
    holding_ = false; held_ = nullptr; paused_ = false; hsFrame_ = 0; hsLeave_ = -1; entryName_.clear(); entryRank_ = -1;
    pendingEntry_ = -1;
    if (withScoreDisk && twoPlayers_ && !pendingContinue_) {   // 0x10088a: two players: player 1, then player 2 may enter a name, each if his score qualifies
        playerScore_[player_] = score_;
        int first = -1;
        for (int p = 0; p < 2; p++)
            if (playerScore_[p] > table().back().score) { if (first < 0) first = p; else pendingEntry_ = p; }   // 0x1008d8: VAL(12th) < score (a tie with the 12th does not qualify here)
        if (first >= 0) { score_ = playerScore_[first]; entryRank_ = HighScores::rankOf(table(), score_); }
    } else if (withScoreDisk) {
        entryRank_ = HighScores::rankOf(table(), score_);
    }
    mode_ = entryRank_ >= 0 ? Mode::NameEntry : Mode::HighScores;
}

void Game::showNameEntryForTest(bool expert, int level, long score, const std::string& typed) {
    expert_ = expert; level_ = level; score_ = int(score);
    endGame(true);
    entryName_ = typed; hsFrame_ = kHsBlack + kHsFadeIn + 10;
}

void Game::startNewGame() {   // Novice: stage 1 or the stage of the typed password; Expert: a random stage of the 200
    int lv = typed_.empty() ? 1 : passwords_.levelFor(typed_);   // an invalid code starts at stage 1, like the original
    // The original rejects the code of stage 200 (0x101b04 searches only FOR i = 1 TO 995 STEP 5, so typing 95039 starts stage 1 although
    // the game shows that code after stage 199). Fixed on the user's request: 95039 starts stage 200.
    if (lv > int(levels_.size())) lv = 0;
    if (expert_) lv = randomLevel();
    beyond_ = false; score_ = 0; playerScore_[0] = playerScore_[1] = 0; stagesDone_[0] = stagesDone_[1] = 0; bonusDue_ = false; bonusStages_[0] = bonusStages_[1] = 0; bonusUnits_ = 200; player_ = 0; startLevel(lv ? lv : 1); typed_.clear();
}

int Game::randomLevel() { return 1 + int(rng_() % levels_.size()); }

void Game::nextStage() {   // after a solved stage: the other player (two-player mode) plays the next stage
    if (twoPlayers_) { playerScore_[player_] = score_; player_ ^= 1; score_ = playerScore_[player_]; }
    startLevel(wipeMode() ? nextLevel_ : level_ + 1);
}

void Game::gameOver(bool fromBonus) {
    // 0x1034a2: the high-score routine asks "PLAY WITH SCOREDISK" only the first time in a session (flag a5-$78c3, set once at
    // start-up); after that the answer given then is used again
    askFromBonus_ = fromBonus;
    if (askedDisk_) endGame(useDisk_);
    else { mode_ = Mode::AskDisk; askIn_ = 0; askOut_ = -1; }
}

void Game::askAnswered(int action) {   // done when the question has faded out
    switch (action) {
    case 1: mode_ = Mode::AskNew; askIn_ = 0; break;                                                     // PLAY WITH SCOREDISK: yes
    case 2: askedDisk_ = true; useDisk_ = false; endGame(false); break;                                  // no
    case 3: mode_ = Mode::AskFormat; askIn_ = 0; break;                                                  // CREATE NEW SCORE DISK: yes
    case 4: askedDisk_ = true; useDisk_ = true; endGame(true); break;                                    // no
    case 5: scores_ = defaults_; scoresDirty_ = true; askedDisk_ = true; useDisk_ = true; endGame(true); break;   // INSERT FORMATTED DISK: yes
    default: break;
    }
}

void Game::leaveTwoPlayer() {
    // 0x103ec2 / 0x1040ee: in a two-player game the player whose time ran out (and who did not or could not buy the stage) drops
    // out; the other one goes on alone as player 1 with his score + the stage number, on the next stage, without a password panel
    const int other = player_ ^ 1;
    playerScore_[player_] = score_;
    score_ = playerScore_[other] + level_;
    twoPlayers_ = false; player_ = 0; playerScore_[0] = score_;
    timedOut_ = false; toFrame_ = -1;
    startLevel(level_ + 1); fadeIn_ = 0;
}

void Game::startBonusStage(int nextLevel) {
    bonusNext_ = nextLevel; fadeIn_ = 0;   // the bonus stage fades in from black
    std::mt19937 gen{std::random_device{}()};
    auto rnd = [&](int n) { return int((gen() >> 4) % uint32_t(n)); };
    int& k = bonusStages_[player_];
    if (++k > 20) { if (bonusUnits_ > 20) bonusUnits_ -= 20; k = 20; }   // 0x10077e..0x1007bc
    bonusLayout_ = 12 + k;
    const DatPicture& mask = pics_[size_t(bonusLayout_)];
    int avail = 0;
    for (int i = 0; i < kBonusCells; i++) {
        const int c = i % 10, r = i / 10;
        const bool blocked = c < mask.w && r < mask.h && mask.index[size_t(r) * mask.w + c] == 15;
        bonusGrid_[i] = blocked ? 255 : 0;
        avail += !blocked;
    }
    for (int p = 0; p < avail / 2; p++) {
        const int icon = rnd(50) + 1;
        for (int t = 0; t < 2; t++) {
            int i;
            do i = rnd(10) * 10 + rnd(10); while (bonusGrid_[i] != 0);
            bonusGrid_[i] = icon;
        }
    }
    for (int& g : bonusGrid_) if (g == 255) g = 0;
    bonusLeft_ = avail;
    bonusSel_ = -1; bonusFrame_ = 0; bonusE_ = 0; bonusPoints_ = 0; bonusEnd_ = -1; bonusPhase_ = 0; bonusTime_ = 100; holding_ = false; held_ = nullptr;
    mode_ = Mode::Bonus;
}

bool Game::bonusFree(int i) const {
    if (i < 0 || i >= kBonusCells || bonusGrid_[i] <= 0) return false;
    const int c = i % 10, r = i / 10;
    if (c == 0 || c == 9 || r == 0 || r == 9) return true;
    return bonusGrid_[i - 1] == 0 || bonusGrid_[i + 1] == 0 || bonusGrid_[i - 10] == 0 || bonusGrid_[i + 10] == 0;
}

void Game::bonusClick(int x, int y) {
    const int col = (x - 6) / 18, row = (y + 9) / 18;   // 1-based as in the original
    if (x < 6 || col < 1 || col > 10 || row < 1 || row > 10) return;
    const int i = (row - 1) * 10 + (col - 1);
    if (!bonusFree(i)) return;
    if (bonusSel_ < 0) { bonusSel_ = i; return; }
    if (bonusSel_ != i && bonusGrid_[bonusSel_] == bonusGrid_[i]) {
        bonusGrid_[bonusSel_] = bonusGrid_[i] = 0;
        bonusLeft_ -= 2;
        bonusPoints_ += level_ + 10;
    }
    bonusSel_ = -1;   // same tile again or another sprite: the frames go
}

void Game::startLevel(int level) {
    level_ = std::max(1, std::min(level, int(levels_.size())));
    // clock period (0x102c3a): Novice from the stage table; Expert: X (a5-$78d7, kept for the whole session) grows by 10 per stage up to
    // 100 and the period is 250-X units; frames measured in the simulator for X = 10..100. The first random stage after stage 200
    // still takes the period of its stage number.
    static const int kExpertTick[11] = {61, 61, 58, 56, 54, 51, 48, 46, 44, 41, 38};
    if (expert_ && !tablePeriodOnce_) {
        if (expertX_ < 100) expertX_ += 10;
        stageTick_ = kExpertTick[expertX_ / 10];
    } else {
        stageTick_ = tickFrames(level_);
    }
    tablePeriodOnce_ = false;
    bonusBase_ = startBonus(level_);
    placed_.clear(); inst_.clear(); holding_ = false; held_ = nullptr; frames_ = 0; paused_ = false; pauseFx_ = -1; panelFx_ = -1; timedOut_ = false; toFrame_ = -1;
    mode_ = Mode::Play;
}

int Game::remaining(PieceType t) const {
    int used = 0;
    for (const Placed& p : placed_) used += p.type == t;
    return pieceCount(t) - used - ((holding_ && held_storage_.type == t) ? 1 : 0);
}

// The original keeps the 7 pieces in fixed slots (large, large, square, medium, small, small, parallelogram). A piece put down is
// drawn on top of what is on the screen (0x102410); picking one up (0x10259c) redraws the figure and the others in slot order
// (0x102958). So placed_ is kept in drawing order: appended on a drop, re-sorted by slot on a pick-up.
static int pieceSlot(PieceType t, int inst) {
    static const int kBase[kPieceTypes] = {0, 2, 3, 4, 6};
    return kBase[t] + (t == kLargeTriangle || t == kSmallTriangle ? inst : 0);
}

std::vector<size_t> Game::drawOrder() const {
    std::vector<size_t> o(placed_.size());
    for (size_t i = 0; i < o.size(); i++) o[i] = i;
    return o;
}

bool Game::pickPlaced(int x, int y) {
    const std::vector<size_t> order = drawOrder();   // 0x10259c picks the piece whose colour is under the pointer: the top one
    for (size_t k = order.size(); k-- > 0;) {
        const size_t i = order[k];
        const Placed& p = placed_[i];
        // exactly the pixels the piece covers on the screen (the original looks at the colour under the pointer)
        std::vector<uint8_t> m(size_t(kBoardW) * kBoardH, 0);
        fillPolygonDrawn(placedOutline(p.type, p.rotation, p.x, p.y), kBoardW, kBoardH, m);
        if (x >= 0 && y >= 0 && x < kBoardW && y < kBoardH && m[size_t(y) * kBoardW + size_t(x)]) {
            held_storage_ = {p.type, p.rotation, inst_[i]};
            placed_.erase(placed_.begin() + long(i));
            inst_.erase(inst_.begin() + long(i));
            std::vector<size_t> o(placed_.size());   // the board is redrawn in slot order
            for (size_t k = 0; k < o.size(); k++) o[k] = k;
            std::stable_sort(o.begin(), o.end(), [&](size_t a, size_t b) { return pieceSlot(placed_[a].type, inst_[a]) < pieceSlot(placed_[b].type, inst_[b]); });
            std::vector<Placed> np; std::vector<int> ni;
            for (size_t k : o) { np.push_back(placed_[k]); ni.push_back(inst_[k]); }
            placed_ = np; inst_ = ni;
            holding_ = true; held_ = &held_storage_;
            return true;
        }
    }
    return false;
}

void Game::trayClick(int x, int y) {
    if (x < 0 || y < 0 || x >= 320 || y >= 200) return;
    drawScene();   // the screen without the pointer (the original takes the pointer off before reading the input)
    const int c = frame_.index[size_t(y) * 320 + size_t(x)];
    if (c != 0 && (c < 20 || c > 24)) return;
    for (int t = 0; t < kPieceTypes; t++) {
        if (x >= kTrayBox[t][0] && x <= kTrayBox[t][2] && y >= kTrayBox[t][1] && y <= kTrayBox[t][3] && remaining(PieceType(t)) > 0) {
            int inst = 0;   // lowest instance of this type that is not on the board
            for (; inst < pieceCount(PieceType(t)); inst++) {
                bool used = false;
                for (size_t k = 0; k < placed_.size(); k++) used |= placed_[k].type == PieceType(t) && inst_[k] == inst;
                if (!used) break;
            }
            held_storage_ = {PieceType(t), 0, inst};
            holding_ = true; held_ = &held_storage_;
            return;
        }
    }
}

bool Game::fitsBoard(int x, int y) const {
    // 0x10242a: a piece is put down only if every corner lies within x 6..221, y 6..193; otherwise the click does nothing
    for (const Pt& p : placedOutline(held_storage_.type, held_storage_.rot, x, y))
        if (p.x < 6 || p.x > 221 || p.y < 6 || p.y > 193) return false;
    return true;
}

void Game::drop(int x, int y) {
    if (x >= kBoardRight || !fitsBoard(x, y)) return;   // pieces can only be put down wholly on the figure area
    placed_.push_back({held_storage_.type, held_storage_.rot, x, y});
    inst_.push_back(held_storage_.inst);
    holding_ = false; held_ = nullptr;
    checkSolved();
}

void Game::checkSolved() {
    if (holding_) return;
    if (placed_.size() != 7 || !solved(levels_[size_t(level_ - 1)], placed_)) return;   // the original runs its test only with all 7 pieces down
    // the original counts the stages each player completes in this game (0x100756, reset at every game start, also from a password);
    // the 10th one is followed by the bonus stage and the count starts again (checked in the simulator: start at 009 -> bonus after 018)
    bonusDue_ = ++stagesDone_[player_] == 10;
    if (bonusDue_) stagesDone_[player_] = 0;
    gain_ = toFrame_ >= 0 ? std::max(0, bonusBase_ - level_ * (toFrame_ / stageTick_ + 1))   // solved after the time-out: what is left of the bonus
                          : bonusBase_ + stageGain(level_, (frames_ / stageTick_)) - startBonus(level_);
    toFrame_ = -1;   // the bonus counter starts at 25 x stage (0 after PLAY AGAIN)
    scoreBefore_ = score_;
    timeAtEnd_ = std::max(0, 100 - (frames_ / stageTick_));
    score_ += gain_;
    pwShown_ = passwords_.codeFor(level_ + 1);
    if (!expert_ && level_ >= int(levels_.size())) beyond_ = true;   // the original does not end after stage 200: random stages go on (found with the Go simulator)
    nextLevel_ = (expert_ || beyond_) ? randomLevel() : level_ + 1;
    pwFrame_ = 0; closeFrame_ = -1;
    mode_ = Mode::Password;
}

void Game::audio(int16_t* out, int frames) {
    int want = wantedSong();
    if (want != song_ || musicGen_ != songGen_) {   // M / F2 stop a song or start one from its beginning (entry +4 / +0 of the player)
        if (song_) music_[song_ - 1].stop();
        song_ = want; songGen_ = musicGen_;
        std::string err;
        wavPos_ = 0;
        if (song_ && wav_[song_ - 1].empty() && (!music_[song_ - 1].load(musicImage_[song_ - 1], err) || !music_[song_ - 1].start(err))) song_ = 0;   // reload = song from the start
    }
    if (!song_) { std::fill(out, out + 2 * size_t(frames), int16_t(0)); return; }
    if (!wav_[song_ - 1].empty()) {   // recording: 25% like the emulated song, from the start again at its end
        const std::vector<int16_t>& w = wav_[song_ - 1];
        const size_t n = w.size() / 2;
        for (int i = 0; i < frames; i++) {
            if (wavPos_ >= n) wavPos_ = 0;
            out[2 * i] = int16_t(w[2 * wavPos_] / 4); out[2 * i + 1] = int16_t(w[2 * wavPos_ + 1] / 4);
            wavPos_++;
        }
        return;
    }
    music_[song_ - 1].setGain(0.25);
    music_[song_ - 1].render(out, frames);
}

bool Game::loadMusicWav(int song, const std::vector<uint8_t>& d, std::string& err) {
    if (song < 1 || song > 2) { err = "song must be 1 or 2"; return false; }
    auto u16 = [&](size_t o) { return unsigned(d[o]) | unsigned(d[o + 1]) << 8; };
    auto u32 = [&](size_t o) { return uint32_t(u16(o)) | uint32_t(u16(o + 2)) << 16; };
    if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4)) { err = "not a WAV file"; return false; }
    unsigned channels = 0, rate = 0, bits = 0, format = 0; size_t data = 0, dataLen = 0;
    for (size_t o = 12; o + 8 <= d.size();) {
        const uint32_t len = u32(o + 4);
        if (!std::memcmp(d.data() + o, "fmt ", 4) && o + 24 <= d.size()) { format = u16(o + 8); channels = u16(o + 10); rate = u32(o + 12); bits = u16(o + 22); }
        else if (!std::memcmp(d.data() + o, "data", 4)) { data = o + 8; dataLen = std::min<size_t>(len, d.size() - data); break; }
        o += 8 + len + (len & 1);
    }
    if (format != 1 || bits != 16 || channels < 1 || channels > 2 || rate < 8000 || !data) { err = "WAV must be 16-bit PCM, mono or stereo"; return false; }
    const size_t frames = dataLen / (2 * channels);
    if (!frames) { err = "WAV has no sound"; return false; }
    auto sample = [&](size_t f, unsigned c) { return int16_t(u16(data + (f * channels + std::min(c, channels - 1)) * 2)); };
    std::vector<int16_t> out;
    if (rate == 44100) {
        out.resize(frames * 2);
        for (size_t f = 0; f < frames; f++) { out[2 * f] = sample(f, 0); out[2 * f + 1] = sample(f, 1); }
    } else {   // linear resampling to 44100 Hz
        const size_t n = size_t(double(frames) * 44100.0 / rate);
        out.resize(n * 2);
        for (size_t i = 0; i < n; i++) {
            const double t = double(i) * rate / 44100.0; const size_t f = std::min(frames - 1, size_t(t)), g = std::min(frames - 1, f + 1);
            const double a = t - double(f);
            for (unsigned c = 0; c < 2; c++) out[2 * i + c] = int16_t(std::lround(sample(f, c) * (1 - a) + sample(g, c) * a));
        }
    }
    wav_[song - 1] = std::move(out);
    musicGen_++;   // a song that is playing starts again from the recording
    return true;
}

void Game::event(const Event& e) {
    // music keys (0x1005fc / 0x100612) are read only in the play loop of a stage (not in the menu, the bonus stage or while paused):
    // M stops the song, M again starts it from the beginning; F2 (the original takes any CSI key) stops it and starts the other song
    const bool musicKeys = mode_ == Mode::Play && !paused_ && pauseFx_ < 0 && panelFx_ < 0 && toFrame_ < timeoutBlank();
    if (e.type == Event::Key && e.key == 'M') { if (musicKeys) { musicOn_ = !musicOn_; musicGen_++; } return; }
    if (e.type == Event::Key && e.key == kKeyF1 + 1) {
        if (musicKeys) { songSel_ = songSel_ == 1 ? 2 : 1; musicOn_ = true; musicGen_++; }
        return;
    }
    if (mode_ == Mode::Intro) {
        if (e.type == Event::Down) {   // a click skips the picture that is on screen (starts its fade-out)
            introFrame_ = std::max(introFrame_, kFadeFrames + kIntroHold[introStage_]);
        } else if (e.type == Event::Key && (e.key == kKeyEsc || e.key == kKeyEnter)) {
            mode_ = Mode::Menu; menuFade_ = kFadeFrames;
        }
        return;
    }
    if (e.type == Event::Move || e.type == Event::Down || e.type == Event::Up) { mx_ = e.x; my_ = e.y; }
    switch (mode_) {
    case Mode::Intro: break;   // handled above
    case Mode::Menu:
        if (startFade_ >= 0) break;   // the menu is fading out: the game is about to start
        if (e.type == Event::Key && e.key == kKeyEsc) { quit_ = true; break; }   // Esc on the level / players screen leaves the game
        if (e.type == Event::Down && e.button == 0) {
            // 0x101824: a click anywhere on the NOVICE / EXPERT pair toggles between them, a click on the 1PLAYER / 2PLAYER pair toggles
            // those (the original checks y twice there instead of x, so that pair has no right-hand limit)
            if (e.y > 82 && e.y < 108 && e.x > 230 && e.x < 292) expert_ = !expert_;
            if (e.y > 115 && e.y < 141 && e.x > 230) twoPlayers_ = !twoPlayers_;
            // like the original, the game starts only with the right mouse button or Enter (a left click on the dragon does nothing)
        } else if (e.type == Event::Down && e.button == 1) {
            startFade_ = 0;
        } else if (e.type == Event::Key) {
            if (e.key >= '0' && e.key <= '9' && typed_.size() < 5) typed_ += char(e.key);
            else if (e.key == kKeyBackspace && !typed_.empty()) typed_.pop_back();
            else if (e.key == kKeyEnter) {
                startFade_ = 0;
            }
        }
        break;
    case Mode::Play:
        if (toFrame_ >= timeoutBlank()) break;   // the screen goes blank just before the question
        // after the time-out the stage goes on (the original never looks at the clock outside 0x102f2e): pieces, pause and Esc still work
        if (pauseFx_ >= 0 || panelFx_ >= 0) break;   // the panel is being blended in or out: the original waits for it
        // pause (0x10433e): Space; it ends with Space or a mouse button. While paused nothing else is read (no Esc).
        if (paused_ && ((e.type == Event::Key && (e.key == ' ' || e.key == 'P')) || (e.type == Event::Down))) { paused_ = false; pauseFx_ = 0; break; }
        if (paused_) break;
        if (e.type == Event::Key) {
            if (e.key == ' ' || e.key == 'P') { paused_ = true; pauseFx_ = 0; holding_ = false; held_ = nullptr; }
            else if (e.key == kKeyEsc || e.key == 'E') { holding_ = false; held_ = nullptr; gameOver(false); }   // 0x10067e: E in the original, Esc kept (user)
            break;
        }
        if (paused_) break;
        // drag and drop: the left button picks a piece up, releasing it puts it down (only over the figure area; a release
        // outside the figure area sends the piece back to the tray); the right button rotates the held piece
        if (e.type == Event::Down && e.button == 1) { rmbHeld_ = true; rmbFrames_ = 0; if (holding_) held_storage_.rot = (held_storage_.rot + 1) % kRotations; }
        else if (e.type == Event::Up && e.button == 1) rmbHeld_ = false;
        else if (e.type == Event::Down && e.button == 0 && !holding_) { if (!pickPlaced(e.x, e.y)) trayClick(e.x, e.y); }
        else if (e.type == Event::Up && e.button == 0 && holding_) {
            if (e.x < kBoardRight && fitsBoard(e.x, e.y)) drop(e.x, e.y);
            else { holding_ = false; held_ = nullptr; }   // released outside the figure area (or sticking out of it): the piece goes back to the tray
        }
        break;
    case Mode::AskReplay:
        if (panelFx_ < 0 && e.type == Event::Down && e.button == 0 && e.y > 105 && e.y < 114) {
            if (e.x > 85 && e.x < 111) {   // YES: pay and play the stage again, the bonus counter starts at 0
                score_ -= 2 * startBonus(level_);
                placed_.clear(); inst_.clear(); holding_ = false; held_ = nullptr; frames_ = 0; timedOut_ = false; toFrame_ = -1;
                bonusBase_ = 0; mode_ = Mode::Play; panelFx_ = 0;
            } else if (e.x > 117 && e.x < 143) {   // NO
                if (twoPlayers_) leaveTwoPlayer(); else gameOver(false);
            }
        }
        break;
    case Mode::AskDisk: case Mode::AskNew: case Mode::AskFormat: {
        // 0x106b70: left button with the pointer in y 106..113, x 86..110 = YES, x 118..142 = NO (not during the fades)
        if (e.type == Event::Down && e.button == 0 && askIn_ >= kFadeFrames && askOut_ < 0 && e.y > 105 && e.y < 114) {
            const bool yes = e.x > 85 && e.x < 111, no = e.x > 117 && e.x < 143;
            int action = 0;
            if (mode_ == Mode::AskDisk) action = yes ? 1 : no ? 2 : 0;
            else if (mode_ == Mode::AskNew) action = yes ? 3 : no ? 4 : 0;
            else action = yes ? 5 : no ? 6 : 0;   // INSERT FORMATTED DISK: NO only asks again (fades out and in)
            if (action) { askAction_ = action; askOut_ = 0; }
        }
        break;   // Esc / Return do nothing here in the original
    }
    case Mode::HighScores:
        // 0x103c02: Return (INKEY$ = CHR$(13)) or any mouse button (MOUSEK); Esc does nothing here
        if (e.type == Event::Down || (e.type == Event::Key && e.key == kKeyEnter)) leaveScores();
        break;
    case Mode::NameEntry:
        if (e.type != Event::Key || hsLeave_ >= 0) break;
        if (e.key == kKeyEnter) {
            auto& t = table();
            HighScores::insert(t, entryName_, score_, level_);
            scoresDirty_ = true;
            if (pendingEntry_ >= 0 && playerScore_[pendingEntry_] > t.back().score) {   // the second player's turn: the screen starts again
                score_ = playerScore_[pendingEntry_]; pendingEntry_ = -1;
                entryName_.clear(); entryRank_ = HighScores::rankOf(t, score_); hsFrame_ = 0;
            } else {
                pendingEntry_ = -1; leaveScores();
            }
        } else if (e.key == kKeyBackspace) { if (!entryName_.empty()) entryName_.pop_back(); }
        else if (entryName_.size() < 8 && e.key >= 32 && e.key <= 95) entryName_ += char(e.key);   // anything the keyboard types (letters in capitals)
        break;
    case Mode::Bonus:
        if (e.type == Event::Key && e.key == kKeyEsc && bonusPhase_ < 3) { bonusSel_ = -1; gameOver(true); }
        else if (e.type == Event::Down && e.button == 0 && bonusPhase_ == 0) bonusClick(e.x, e.y);
        break;
    case Mode::Password:
        if ((e.type == Event::Down || e.type == Event::Key) &&   // 0x104320: any mouse button or any key closes the password panel
            !wipeMode() && closeFrame_ < 0 && toBonusFade_ < 0 && pwFrame_ >= kPwOpenStart + 6 * kPwOpenSteps) {
            if (bonusDue_ && level_ < int(levels_.size())) toBonusFade_ = 0;   // the bonus stage follows: fade to black first
            else { closeFrame_ = 0; pickTransition(); }
        }   // the panel is rolled up; the next stage appears through the closing blinds
        break;
    }
}

void Game::step() {
    if (mode_ == Mode::Intro) {
        if (++introFrame_ >= 2 * kFadeFrames + kIntroHold[introStage_]) {
            introFrame_ = 0;
            if (++introStage_ >= 3) { mode_ = Mode::Menu; menuFade_ = 0; }
        }
    } else if (mode_ == Mode::Menu && menuFade_ < kFadeFrames) {
        menuFade_++;
    }
    if (mode_ == Mode::Menu && startFade_ >= 0 && ++startFade_ >= kFadeFrames) { startFade_ = -1; startNewGame(); fadeIn_ = 0; }
    else if (fadeIn_ < kFadeFrames) fadeIn_++;
    if (mode_ == Mode::Play && pauseFx_ >= 0 && ++pauseFx_ >= kPauseFxFrames) pauseFx_ = -1;
    if ((mode_ == Mode::Play || mode_ == Mode::AskReplay) && panelFx_ >= 0 && ++panelFx_ >= kPauseFxFrames) panelFx_ = -1;
    if (mode_ == Mode::Play && !paused_ && pauseFx_ < 0 && panelFx_ < 0 && fadeIn_ >= kFadeFrames) frames_++;
    // right button held: the piece keeps turning (the user's wish; in this version's code 0x101bf4 the repeat never starts, its counter
    // stops at 3 before the intended 8): first step on the press, then one step every kRotRepeat frames after kRotDelay frames
    if (mode_ == Mode::Play && rmbHeld_ && !paused_ && pauseFx_ < 0 && panelFx_ < 0) {
        ++rmbFrames_;
        if (holding_ && rmbFrames_ >= kRotDelay && (rmbFrames_ - kRotDelay) % kRotRepeat == 0) held_storage_.rot = (held_storage_.rot + 1) % kRotations;
    } else if (mode_ != Mode::Play) rmbHeld_ = false;   // the clock starts when the stage has faded in
    if (mode_ == Mode::Play && !paused_ && toFrame_ < 0 && (frames_ / stageTick_) >= 100) { toFrame_ = 0; }   // time is up: the bonus starts counting down
    else if (mode_ == Mode::Play && toFrame_ >= 0 && !paused_ && pauseFx_ < 0) {   // measured in the original: one step per tick (75 frames), the first right at the time-out; blank frame; then the question
        if (++toFrame_ >= timeoutEnd()) {
            toFrame_ = -1; timedOut_ = true; askFromBonus_ = false;
            // 0x103c7e: with at least 2 x (25 x stage) points the player may buy the stage again ("PLAY AGAIN FOR nnnn POINTS")
            if (score_ >= 2 * startBonus(level_)) { mode_ = Mode::AskReplay; panelFx_ = 0; }
            else if (twoPlayers_) { pendingContinue_ = true; gameOver(false); }   // 0x1040ee: the table for the player who drops out, then the other one goes on
            else gameOver(false);
        }
    }
    if (mode_ == Mode::AskDisk || mode_ == Mode::AskNew || mode_ == Mode::AskFormat) {
        if (askIn_ < kFadeFrames) askIn_++;
        if (askOut_ >= 0 && ++askOut_ >= kFadeFrames) {
            askOut_ = -1;
            if (askAction_ == 6) askIn_ = 0; else askAnswered(askAction_);
        }
    }
    if (mode_ == Mode::HighScores || mode_ == Mode::NameEntry) {
        if (hsLeave_ >= 0) {
            if (++hsLeave_ >= kHsFadeOut + kHsGap) {
                hsLeave_ = -1;
                if (pendingContinue_) { pendingContinue_ = false; leaveTwoPlayer(); }   // two players: the other one goes on alone
                else { mode_ = Mode::Menu; menuFade_ = kFadeFrames - 4; typed_.clear(); }
            }
        } else {
            hsFrame_++;
            if (mode_ == Mode::HighScores && hsFrame_ >= kHsFrames) leaveScores();
        }
    }
    if (mode_ == Mode::Bonus) {
        if (bonusPhase_ == 0) {
            if (fadeIn_ >= kFadeFrames) bonusE_ = ++bonusFrame_ / bonusTickFrames();
            bonusTime_ = std::max(0, 100 - bonusE_);
            if (bonusLeft_ == 0 || bonusE_ >= 101) { bonusSel_ = -1; bonusPhase_ = 1; }
        } else if (bonusPhase_ == 1) {   // 0x1030e8: the time left runs into the bonus counter, stage/2+1 per step of 2 ticks
            if (bonusE_ < 101) { bonusE_ += bonusE_ == 98 ? 1 : 2; bonusPoints_ += bonusPerStep(level_); bonusTime_ = std::max(0, 100 - bonusE_); }
            else { bonusPhase_ = 3; bonusEnd_ = 0; }
        } else if (++bonusEnd_ >= kFadeFrames) {
            score_ += bonusPoints_; bonusPoints_ = 0;   // 0x102d2a: the bonus counter goes to the score when the next stage is set up
            startLevel(bonusNext_); fadeIn_ = 0;   // the next stage fades in from black
        }
    }
    if (mode_ == Mode::Password && toBonusFade_ >= 0) {
        if (++toBonusFade_ >= kFadeFrames) { toBonusFade_ = -1; startBonusStage(level_ + 1); }
    } else if (mode_ == Mode::Password && beyond_) {   // stage 200 solved: the count-down, then fade to black and Expert begins
        if (++pwFrame_ >= kPwCount + kFadeFrames) { beyond_ = false; expert_ = true; tablePeriodOnce_ = true; nextStage(); fadeIn_ = 0; }
    } else if (mode_ == Mode::Password && wipeMode()) {   // Expert: no password panel and no click; after a hold the next random stage is wiped in from the left
        if (pwFrame_ == kXWipeStart - 1) pickTransition();
        if (++pwFrame_ >= kXWipeStart + transitionFrames(transFx_)) nextStage();
    } else if (mode_ == Mode::Password) {
        if (closeFrame_ >= 0) {
            if (++closeFrame_ >= transitionFrames(transFx_)) {
                closeFrame_ = -1;
                if (level_ >= int(levels_.size())) { askFromBonus_ = false; endGame(true); }
                else if (bonusDue_) startBonusStage(level_ + 1);
                else nextStage();
            }
        } else if (pwFrame_ < 100000) {
            pwFrame_++;
        }
    }
    render();
}

void Game::text(const DatPicture& font, int x, int y, const std::string& s, const Palette& pal) {
    if (&font != &pics_[kFontLight]) { drawText(frame_, font, x, y, s, pal, &font == &pics_[kFontUi]); return; }
    // record 5 (title/credits lettering): only index 0 is ink (black); the rest of each cell is transparent
    for (char ch : s) {
        int cell = glyphCell(ch);
        if (cell >= 0)
            for (int gy = 0; gy < kGlyphH; gy++)
                for (int gx = 0; gx < kGlyphW; gx++) {
                    int px = x + gx, py = y + gy;
                    if ((ch != '@' && font.index[size_t(gy) * font.w + cell * kGlyphW + gx] != 0) || px < 0 || py < 0 || px >= frame_.w || py >= frame_.h) continue;
                    frame_.index[size_t(py) * frame_.w + px] = 0;
                    frame_.rgb[size_t(py) * frame_.w + px] = pal.c[0];
                }
        x += kGlyphW;
    }
}

// The scroll panel; on the credits screen its button boxes are covered with parchment texture.
void Game::drawMenuPanel(bool coverBoxes) {
    blitPicture(frame_, pics_[kDatMenuPanel], kMenuPanel.x, kMenuPanel.y, title_);
    if (!coverBoxes) return;
    for (int y = 0; y < kPanelTexH; y++)
        for (int x = 0; x < kPanelTexW; x++) {
            char ch = kPanelTex[y][x];
            size_t i = size_t(kPanelTexY + y) * 320 + kPanelTexX + x;
            if (ch == '?') { frame_.index[i] = boardMenu_.index[i]; frame_.rgb[i] = boardMenu_.rgb[i]; continue; }
            int v = ch <= '9' ? ch - '0' : 10 + (ch - 'a');
            frame_.index[i] = uint8_t(v); frame_.rgb[i] = title_.c[v];
        }
}

void Game::fillPoly(const std::vector<Pt>& poly, Rgb colour) {
    std::vector<uint8_t> mask(size_t(frame_.w) * frame_.h, 0);
    fillPolygonDrawn(poly, frame_.w, frame_.h, mask);
    for (size_t i = 0; i < mask.size(); i++) if (mask[i]) frame_.rgb[i] = colour;
}

void Game::drawPiece(PieceType t, int rot, int inst, int x, int y) {
    static const int kBase[kPieceTypes] = {0, 2, 3, 4, 6};   // first colour slot of each type
    fillPoly(placedOutline(t, rot, x, y), pieceColour_[kBase[t] + (t == kLargeTriangle || t == kSmallTriangle ? inst : 0)]);
}

// Vertical blinds: slats repeat every 16 columns; a slat of (even) width w is centred on column boundary b (+16k).
// The screen effects of 0x10d074 (modes 1..11), timed with a 68000 cycle count at 7.09 MHz (141800 cycles per frame) for the delay
// 5000 used between stages: the busy loop (dbf, 10 cycles) = 0.353 frames per step, copying a row of 5 planes = 0.033, a pixel column
// of 5 planes = 0.61 (0.47 with a pattern), a whole-screen pass of modes 10/11 = 5.5 (this model gives the 46 frames measured for
// the pause panel, effect 10 at delay 500). The "new" picture is copied over the old one:
//   1 rows 0..199 with a 4-pixel checkerboard ($F0F0, inverted every row), then rows 199..0 with the rest   2 rows top to bottom
//   3 rows from the top and the bottom to the middle   4 rows bottom to top   5 columns left to right   6 right to left
//   7 columns from both sides to the middle   8 rows from the top with $F0F0 and at the same time from the bottom with $0F0F
//   9 columns from the left on odd rows and from the right on even rows   10 each 16-pixel column opens from its middle (8 passes)
//   11 in each 32 pixels two columns per pass move in from the edges (even rows) or out from the middle (odd rows), 16 passes
static double transitionTime(int fx, int x, int y) {
    const bool inF0 = ((x & 15) >> 2) % 2 == 0;   // pixel set in the pattern $F0F0
    switch (fx) {
    case 1: return (inF0 == (y % 2 == 0) ? y : 200 + 199 - y) * 0.386;
    case 2: return y * 0.386;
    case 3: return std::min(y, 199 - y) * 0.419;
    case 4: return (199 - y) * 0.386;
    case 5: return x * 0.786;
    case 6: return (319 - x) * 0.786;
    case 7: return std::min(x, 319 - x) * 1.396;
    case 8: return (inF0 ? y : 199 - y) * 0.419;
    case 9: return (y % 2 ? x : 319 - x) * 1.116;
    case 10: { const int c = x & 15; return (c <= 7 ? 7 - c : c - 8) * 5.88 + y * 5.53 / 200; }
    default: { const int p = x & 31; const int pass = y % 2 == 0 ? (p < 16 ? p : 31 - p) : (p < 16 ? 15 - p : p - 16); return pass * 5.93 + y * 5.57 / 200; }
    }
}
int Game::transitionFrames(int fx) {
    static const double kStep[12] = {0, 0.386, 0.386, 0.419, 0.386, 0.786, 0.786, 1.396, 0.419, 1.116, 5.88, 5.93};
    static int cache[12] = {};
    fx = std::max(1, std::min(11, fx));
    if (!cache[fx]) {
        double t = 0;
        for (int y = 0; y < 200; y++) for (int x = 0; x < 320; x++) t = std::max(t, transitionTime(fx, x, y));
        cache[fx] = int(std::ceil(t + kStep[fx]));
    }
    return cache[fx];
}
bool Game::transitionShowsNew(int fx, int x, int y, int frame) { return transitionTime(fx, x, y) <= frame; }

// 0x1073d2 → 0x10f48a: the pointer is a bob drawn with BltMaskBitMapRastPort (minterm $E0) through a mask that also covers the
// pointer's shape moved 2 px left and 6 px down; there the bob's own pixels are colour 0, so the pointer casts a black shadow
// (measured on the real screen: the pointer at mouse 90,130 on the scoredisk panel)
void Game::drawPointer(const DatPicture& pic, const Palette& pal) {
    const int x0 = mx_ + kCursorOffset.x, y0 = my_ + kCursorOffset.y;
    for (int gy = 0; gy < pic.h; gy++)
        for (int gx = 0; gx < pic.w; gx++) {
            if (!pic.index[size_t(gy) * pic.w + gx]) continue;
            const int sx = gx - 2, sy = gy + 6;   // shadow pixel, inside the 32 x 24 bob
            if (sx < 0 || sy >= pic.h) continue;
            if (pic.index[size_t(sy) * pic.w + sx]) continue;   // the pointer itself is drawn there
            const int px = x0 + sx, py = y0 + sy;
            if (px < 0 || py < 0 || px >= 320 || py >= 200) continue;
            frame_.index[size_t(py) * 320 + px] = 0; frame_.rgb[size_t(py) * 320 + px] = pal.c[0];
        }
    blitPicture(frame_, pic, x0, y0, pal);
}

static bool slatVisible(int x, int b, int w) {
    if (w >= 16) return true;
    int rr = ((x - b + 8) % 16 + 16) % 16 - 8;   // -8..7
    return rr >= -w / 2 && rr < w / 2;
}

void Game::drawScene() {
    frame_ = boardPlay_;
    const Level& lv = levels_[size_t(level_ - 1)];
    std::vector<Pt> poly;
    for (size_t k = 0; k + 3 < lv.coords.size(); k += 2) poly.push_back({kCx + lv.coords[k], kCy - lv.coords[k + 1]});
    const bool hide = (mode_ == Mode::Play && paused_) || mode_ == Mode::AskDisk || mode_ == Mode::AskNew || mode_ == Mode::AskFormat || mode_ == Mode::AskReplay;   // the figure and the pieces are hidden while the game waits
    if ((mode_ == Mode::Play || mode_ == Mode::Password) && !hide) {
        std::vector<Pt> sh = poly; for (auto& p : sh) { p.x += kShadow; p.y += kShadow; }
        fillPoly(sh, black_);
        fillPoly(poly, silhouette_);
    }
    if (!hide) {
        for (size_t i : drawOrder()) drawPiece(placed_[i].type, placed_[i].rotation, inst_[i], placed_[i].x, placed_[i].y);
    }
    {   // remaining pieces per type, printed on the tray
        static const Pt kCount[kPieceTypes] = {{244, 25}, {268, 49}, {295, 49}, {272, 95}, {292, 95}};
        for (int t = 0; t < kPieceTypes; t++) text(pics_[kFontUi], kCount[t].x, kCount[t].y, std::to_string(dCounts_[t] >= 0 ? dCounts_[t] : remaining(PieceType(t))), play_);
    }
    text(pics_[kFontUi], kScoreRow.x, kScoreRow.y, pad(dScore_, 7), play_);
    text(pics_[kFontUi], kBonusRow.x, kBonusRow.y, pad(dBonus_, 7), play_);
    text(pics_[kFontUi], kTimeRow.x, kTimeRow.y, pad(dTime_, 3), play_);
    text(pics_[kFontUi], kLevelLabel.x, kLevelLabel.y, expert_ ? std::string("EXP") : pad(level_, 3), play_);   // Expert shows no stage number
    text(pics_[kFontUi], kPlayerNo.x, kPlayerNo.y, std::to_string((previewPlayer_ >= 0 ? previewPlayer_ : player_) + 1), play_);
    {   // time bar: the picture is shown from the left; E/2+1 columns are consumed from the right (E = elapsed ticks + 1)
        const DatPicture& bar = pics_[kDatTimeBar];
        const int barE = dE_ >= 0 ? dE_ : frames_ / stageTick_;
        int keep = std::max(0, bar.w - ((barE + 1) / 2 + 1)), from = 0;
        if (dFill_ >= 0) { keep = bar.w; from = std::max(0, bar.w - dFill_); }   // refill: the bar is uncovered from its right end towards the left
        for (int gy = 0; gy < bar.h; gy++)
            for (int gx = from; gx < keep; gx++) {
                uint8_t v = bar.index[size_t(gy) * bar.w + gx];
                size_t i = size_t(kTimeBarPos.y + gy) * frame_.w + kTimeBarPos.x + gx;
                frame_.index[i] = v; frame_.rgb[i] = play_.c[v & 31];
            }
    }
    if (!hide && holding_) drawPiece(held_storage_.type, held_storage_.rot, held_storage_.inst, mx_, my_);   // the piece in hand is above the panel text too
}

// The panel refill after a solved stage in wipe mode (Expert, or Novice after stage 200), measured in the original with 8-frame shots:
// T = frames since the start of the wipe. Piece counts come back one type at a time, the time bar is uncovered from its right end at 7/15 column
// per frame, time and bonus jump to their start values,.
void Game::refillPanel(int T, int fullBonus) {
    static const int kFinal[kPieceTypes] = {2, 1, 1, 2, 1}, kAt[kPieceTypes] = {107, 51, -5, 43, 3};
    for (int t = 0; t < kPieceTypes; t++) dCounts_[t] = T >= kAt[t] ? kFinal[t] : 0;
    dFill_ = std::min(52, std::max(0, (T + 10) * 7 / 15));
    if (T >= 0) {
        dTime_ = T >= 59 ? 100 : 0;
        dBonus_ = T >= 75 ? fullBonus : (T >= 59 ? fullBonus / 6 / 25 * 25 : 0);
    }
}

void Game::drawHighScores() {
    frame_ = hsPic_;
    std::vector<ScoreEntry> rows = table();
    if (mode_ == Mode::NameEntry && entryRank_ >= 0) HighScores::insert(rows, entryName_, score_, level_);   // the new entry is typed in place
    const bool withLevel = !expert_;
    if (!withLevel)   // table B has no level column: the "LEVEL" header is replaced by plain parchment
        for (int y = 0; y < kHsCoverH; y++) for (int x = 0; x < kHsCoverW; x++) {
            unsigned v = unsigned(std::strtoul(std::string(kHsCover[y] + 6 * x, 6).c_str(), nullptr, 16));
            frame_.rgb[size_t(kHsCoverY + y) * 320 + kHsCoverX + x] = Rgb{uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)};
        }
    for (size_t r = 0; r < rows.size() && r < 12; r++) {   // pixel-identical to the original: record 5 cells drawn opaque, 8 px pitch
        int y = 51 + 9 * int(r);
        char lv[8], sc[16];
        std::snprintf(lv, sizeof lv, withLevel ? "%3d" : "   ", rows[r].level);
        std::snprintf(sc, sizeof sc, "%7ld", rows[r].score);
        drawText(frame_, pics_[kFontLight], 76, y, rows[r].name + std::string(rows[r].name.size() < 8 ? 8 - rows[r].name.size() : 0, ' '), hsPal_, true);
        drawText(frame_, pics_[kFontLight], 150, y, lv, hsPal_, true);
        drawText(frame_, pics_[kFontLight], 188, y, sc, hsPal_, true);
    }
    // black while the picture "loads", quick fade in; fade out when leaving
    auto dim = [&](int num, int den) {
        for (Rgb& c : frame_.rgb) { c.r = uint8_t(c.r * num / den); c.g = uint8_t(c.g * num / den); c.b = uint8_t(c.b * num / den); }
    };
    if (hsLeave_ >= 0) { if (hsLeave_ >= kHsFadeOut) dim(0, 1); else dim(kHsFadeOut - hsLeave_, kHsFadeOut); }
    else if (hsFrame_ < kHsBlack) dim(0, 1);
    else if (hsFrame_ < kHsBlack + kHsFadeIn) dim(hsFrame_ - kHsBlack + 1, kHsFadeIn + 1);
}

void Game::drawPanelOverlay() {
    if (mode_ == Mode::AskDisk || mode_ == Mode::AskNew || mode_ == Mode::AskFormat) {
        blitPicture(frame_, pics_[84], kPausePanel.x, kPausePanel.y, play_, false);   // pasted with minterm $C0 (plain copy): colour 0 = black, that is the shadow
        // 0x1063d8 / 0x106402 / 0x10643a: x 38 for the 19-character question, x 30 for the two 21-character ones
        text(pics_[kFontUi], mode_ == Mode::AskDisk ? 38 : 30, 93, mode_ == Mode::AskDisk ? "PLAY WITH SCOREDISK" : mode_ == Mode::AskNew ? "CREATE NEW SCORE DISK" : "INSERT FORMATTED DISK", play_);
        for (int rec : {kDatYes, kDatNo}) {   // the YES / NO button pictures are drawn opaque
            const DatPicture& b = pics_[size_t(rec)]; TextPos pos = rec == kDatYes ? kYesPos : kNoPos;
            for (int gy = 0; gy < b.h; gy++)
                for (int gx = 0; gx < b.w; gx++)
                    { uint8_t v = b.index[size_t(gy) * b.w + gx]; size_t i = size_t(pos.y + gy) * 320 + pos.x + gx; frame_.index[i] = v; frame_.rgb[i] = play_.c[v]; }
        }
    } else if (mode_ == Mode::AskReplay) {   // 0x103cbc: panel, two sprites, "PLAY AGAIN FOR" / "nnnn POINTS", YES / NO
        blitPicture(frame_, pics_[84], kPausePanel.x, kPausePanel.y, play_, false);   // pasted with minterm $C0 (plain copy): colour 0 = black, that is the shadow
        blitPicture(frame_, pics_[54], 35, 90, play_, false);
        blitPicture(frame_, pics_[54], 176, 90, play_, false);
        text(pics_[kFontUi], 59, 84, "PLAY AGAIN FOR", play_);
        std::string n = std::to_string(2 * startBonus(level_)); while (n.size() < 4) n = "0" + n;
        text(pics_[kFontUi], 71, 93, n + " POINTS", play_);
        for (int rec : {kDatYes, kDatNo}) {
            const DatPicture& b = pics_[size_t(rec)]; TextPos pos = rec == kDatYes ? kYesPos : kNoPos;
            for (int gy = 0; gy < b.h; gy++)
                for (int gx = 0; gx < b.w; gx++)
                    { uint8_t v = b.index[size_t(gy) * b.w + gx]; size_t i = size_t(pos.y + gy) * 320 + pos.x + gx; frame_.index[i] = v; frame_.rgb[i] = play_.c[v]; }
        }
    } else if (mode_ == Mode::Password) {
        blitPicture(frame_, pics_[84], kPausePanel.x, kPausePanel.y, play_, false);   // pasted with minterm $C0 (plain copy): colour 0 = black, that is the shadow
        text(pics_[kFontUi], kPwPassword.x, kPwPassword.y, "PASSWORD", play_);
        text(pics_[kFontUi], kPwFor.x, kPwFor.y, "FOR", play_);
        text(pics_[kFontUi], kPwLevel.x, kPwLevel.y, "LEVEL", play_);
        text(pics_[kFontUi], kPwLevelNo.x, kPwLevelNo.y, pad(level_ + 1, 3), play_);
        text(pics_[kFontUi], kPwCode.x, kPwCode.y, pwShown_, play_);
    } else if (paused_) {
        // the lettering of the panel is colour 0 of the picture: black, not transparent
        const DatPicture& pp = pics_[kDatPaused];
        blitPicture(frame_, pp, kPausePanel.x, kPausePanel.y, play_, false);
        for (int gy = 0; gy < pp.h; gy++) for (int gx = 0; gx < pp.w; gx++) {
            int px = kPausePanel.x + gx, py = kPausePanel.y + gy;
            if (!pp.index[size_t(gy) * pp.w + gx] && px >= 0 && py >= 0 && px < frame_.w && py < frame_.h) frame_.rgb[size_t(py) * frame_.w + px] = black_;
        }
    }
}

void Game::render() {
    // Fades of the original (0x1048cc out, 0x104a74 in): 15 steps, each takes 1 from every 4-bit colour component (clamped at 0),
    // so all components drop by the same amount and dark colours reach black first. num/den = how far the fade has come back to full.
    auto fade = [&](int num, int den) {
        if (num >= den) return;
        const int sub = 15 - (15 * std::max(0, num) + den / 2) / den;   // 15 = black, 0 = full palette
        auto f = [sub](uint8_t v) { const int c = (v + 8) / 17 - sub; return uint8_t(c > 0 ? c * 17 : 0); };
        for (Rgb& c : frame_.rgb) { c.r = f(c.r); c.g = f(c.g); c.b = f(c.b); }
    };
    auto askFade = [&] {   // scoredisk questions: fade in, and fade out after the answer
        if (askOut_ >= 0) fade(kFadeFrames - askOut_, kFadeFrames);
        else if (askIn_ < kFadeFrames) fade(askIn_, kFadeFrames);
    };
    if (mode_ == Mode::Intro) {
        const int len = 2 * kFadeFrames + kIntroHold[introStage_];
        if (introStage_ == 0) {
            frame_ = thalion_;
        } else {
            frame_ = boardMenu_;
            if (introFrame_ >= kFadeFrames) {
                for (int y = 0; y < kCopyrightRows; y++)
                    for (int x = 0; kCopyright[y][x]; x++)
                        if (kCopyright[y][x] == '#') frame_.rgb[size_t(kCopyrightY + y) * 320 + kCopyrightX + x] = title_.c[0];
            }
            if (introStage_ == 2) {
                drawMenuPanel(true);   // the credits screen shows the scroll without the button boxes
                const DatPicture& f5 = pics_[kFontLight];
                text(f5, kCreditDesign.x, kCreditDesign.y, "DESIGN", title_);
                text(f5, kCreditMutschall.x, kCreditMutschall.y, "MUTSCHALL", title_);
                text(f5, kCreditMusic.x, kCreditMusic.y, "MUSIC", title_);
                text(f5, kCreditHippel.x, kCreditHippel.y, "HIPPEL", title_);
                text(f5, kCreditAmiga.x, kCreditAmiga.y, "AMIGA", title_);
                text(f5, kCreditVersion.x, kCreditVersion.y, "VERSION", title_);
                text(f5, kCreditBehrends.x, kCreditBehrends.y, "BEHRENDS", title_);
                text(f5, kCreditMaack.x, kCreditMaack.y, "MAACK", title_);
            }
        }
        if (introFrame_ < kFadeFrames) fade(introFrame_, kFadeFrames);
        else if (introFrame_ >= len - kFadeFrames) fade(len - introFrame_, kFadeFrames);
        return;
    }
    if (mode_ == Mode::Menu) {
        frame_ = boardMenu_;
        drawMenuPanel(false);
        // the selected choice shows its teal button; the other one only its black lettering (index 0 = black ink)
        auto button = [&](int rec, TextPos pos, bool selected) {
            const DatPicture& b = pics_[rec];
            for (int gy = 0; gy < b.h; gy++)
                for (int gx = 0; gx < b.w; gx++) {
                    uint8_t v = b.index[size_t(gy) * b.w + gx];
                    if (!selected && v) continue;
                    size_t i = size_t(pos.y + gy) * frame_.w + pos.x + gx;
                    frame_.index[i] = v; frame_.rgb[i] = title_.c[v & 31];
                }
        };
        button(kDatNovice, kMenuNovice, !expert_);
        button(kDatExpert, kMenuExpert, expert_);
        button(kDatOnePlayer, kMenuOnePlayer, !twoPlayers_);
        button(kDatTwoPlayer, kMenuTwoPlayer, twoPlayers_);
        for (int y = 0; y < kPwLabelH; y++)   // label: bitmap cut from the real screenshot (its letters are narrower than the DAT fonts)
            for (int x = 0; x < kPwLabelW; x++)
                if (kPwLabel[y][x] == '#') { size_t i = size_t(kPwLabelY + y) * 320 + kPwLabelX + x; frame_.index[i] = 0; frame_.rgb[i] = title_.c[0]; }
        auto ink = [&](TextPos pos, const std::string& str) {   // digits advance 8 px (the DAT cell width) (the DAT cells are 8 wide)
            // record 5 cells carry the parchment texture around the ink, so they are drawn opaque (identical to the original)
            for (size_t k = 0; k < str.size(); k++) drawText(frame_, pics_[kFontLight], pos.x + int(k) * kPwDigitPitch, pos.y, std::string(1, str[k]), title_, true);
        };
        if (!typed_.empty()) ink(kMenuPasswordCode, typed_);
        drawPointer(pics_[kDatMenuCursor], title_);
        fade(startFade_ >= 0 ? kFadeFrames - startFade_ : menuFade_, kFadeFrames);
        return;
    }
    if (mode_ == Mode::HighScores || mode_ == Mode::NameEntry) { drawHighScores(); return; }
    const bool askMode = (mode_ == Mode::AskDisk || mode_ == Mode::AskNew || mode_ == Mode::AskFormat);
    if (mode_ == Mode::Bonus || (askMode && askFromBonus_)) {
        frame_ = boardPlay_;
        auto black = [&](int x0, int y0, int x1, int y1, bool fill) {
            for (int sy = y0; sy <= y1; sy++)
                for (int sx = x0; sx <= x1; sx++) {
                    if (!fill && sy != y0 && sy != y1 && sx != x0 && sx != x1) continue;
                    if (sx < 0 || sy < 0 || sx >= 320 || sy >= 200) continue;
                    size_t k = size_t(sy) * 320 + size_t(sx); frame_.index[k] = 0; frame_.rgb[k] = black_;
                }
        };
        for (int i = 0; i < kBonusCells; i++) {   // frames and shadows first, then the tiles
            if (bonusGrid_[i] <= 0 || mode_ != Mode::Bonus) continue;
            int x, y; bonusTile(i, x, y);
            black(x - 1, y - 1, x + 16, y + 16, false);
            black(x + kBonusShadow, y + kBonusShadow, x + kBonusShadow + 17, y + kBonusShadow + 17, true);
        }
        for (int i = 0; i < kBonusCells; i++) {
            if (bonusGrid_[i] <= 0 || mode_ != Mode::Bonus) continue;
            int x, y; bonusTile(i, x, y);
            blitPicture(frame_, pics_[size_t(32 + bonusGrid_[i])], x, y, play_, false);   // $C0 copy, opaque
            if (i == bonusSel_)   // red frame on the outer pixels of the selected tile
                for (int k = 0; k < 16; k++)
                    for (auto pr : {std::pair<int, int>{x + k, y}, std::pair<int, int>{x + k, y + 15}, std::pair<int, int>{x, y + k}, std::pair<int, int>{x + 15, y + k}})
                        frame_.rgb[size_t(pr.second) * 320 + size_t(pr.first)] = Rgb{170, 51, 17};
        }
        static const Pt kCount[kPieceTypes] = {{244, 25}, {268, 49}, {295, 49}, {272, 95}, {292, 95}};
        for (int t = 0; t < kPieceTypes; t++) text(pics_[kFontUi], kCount[t].x, kCount[t].y, "0", play_);   // all pieces were used up
        text(pics_[kFontUi], kScoreRow.x, kScoreRow.y, pad(score_, 7), play_);
        text(pics_[kFontUi], kBonusRow.x, kBonusRow.y, pad(bonusPoints_, 7), play_);
        text(pics_[kFontUi], kTimeRow.x, kTimeRow.y, pad(bonusTime_, 3), play_);
        text(pics_[kFontUi], kLevelLabel.x, kLevelLabel.y, pad(bonusNext_ - 1, 3), play_);
        text(pics_[kFontUi], kPlayerNo.x, kPlayerNo.y, "1", play_);
        const DatPicture& bar = pics_[kDatTimeBar];
        int keep = std::max(0, bar.w - ((bonusE_ + 1) / 2 + 1));   // as in a stage: E/2+1 columns consumed from the right
        for (int gy = 0; gy < bar.h; gy++) for (int gx = 0; gx < keep; gx++) {
            uint8_t v = bar.index[size_t(gy) * bar.w + gx]; size_t i = size_t(kTimeBarPos.y + gy) * 320 + kTimeBarPos.x + gx;
            frame_.index[i] = v; frame_.rgb[i] = play_.c[v & 31];
        }
        if (askMode) drawPanelOverlay();
        drawPointer(pics_[kDatCursor], play_);
        if (askMode) { askFade(); return; }
        if (bonusPhase_ == 3) fade(kFadeFrames - bonusEnd_, kFadeFrames);
        else if (fadeIn_ < kFadeFrames) fade(fadeIn_, kFadeFrames);
        return;
    }
    dE_ = -1;
    dScore_ = score_; dBonus_ = timedOut_ ? 0 : toFrame_ >= 0 ? std::max(0, bonusBase_ - level_ * (toFrame_ / stageTick_ + 1)) : bonusBase_; dTime_ = std::max(0, 100 - (frames_ / stageTick_));
    if (mode_ == Mode::Password) {
        if (pwFrame_ < kPwCount) {
            // 0x103120..0x10347c: first the remaining time runs out in steps of 2 (1 at 98) while every step adds stage/2+1 to the
            // bonus counter (the score stays); then the bonus counter goes over to the score in steps of 1000, 100, 10 or 1 (the
            // largest that fits). The original takes as long as its CPU needs; here the steps are spread over the measured 156 frames.
            std::vector<std::array<int, 4>> seq;   // time shown, bonus counter, score, E after each step
            int e = timeAtEnd_ > 0 ? 100 - timeAtEnd_ : 101, b = gain_;   // solved after the time-out: no time left, only the transfer
            {   // bonus counter at the start of the count-down: the total less the steps still to come
                int steps = 0; for (int k = e; k < 101; k += (k == 98) ? 1 : 2) steps++;
                b = gain_ - steps * bonusPerStep(level_);
            }
            seq.push_back({std::max(0, 100 - e), b, scoreBefore_, e});
            while (e < 101) { e += (e == 98) ? 1 : 2; b += bonusPerStep(level_); seq.push_back({std::max(0, 100 - e), b, scoreBefore_, e}); }
            int sc = scoreBefore_;
            while (b > 0) { const int c = b >= 1000 ? 1000 : b >= 100 ? 100 : b >= 10 ? 10 : 1; b -= c; sc += c; seq.push_back({0, b, sc, e}); }
            const size_t k = std::min(seq.size() - 1, size_t(pwFrame_) * seq.size() / size_t(kPwCount * 4 / 5));   // done after 4/5 of the time, then held
            dTime_ = seq[k][0]; dBonus_ = seq[k][1]; dScore_ = seq[k][2]; dE_ = seq[k][3];   // the time bar runs out with E too
        } else {
            dBonus_ = 0; dTime_ = 0; dE_ = 101;
        }
        if (beyond_) {   // after stage 200: the finished scene fades to black once the bonus has been counted
            if (pwFrame_ >= kPwCount) { dBonus_ = 0; dTime_ = 0; }
            drawScene();
            if (pwFrame_ >= kPwCount) fade(kFadeFrames - (pwFrame_ - kPwCount), kFadeFrames);
        } else if (wipeMode()) {
            if (pwFrame_ < kPwCount) { drawScene(); }
            else {
                dBonus_ = 0; dTime_ = 0;
                refillPanel(pwFrame_ - kXWipeStart, 0);   // starts a little before the wipe
                drawScene();
                dFill_ = -1; for (int& c : dCounts_) c = -1;
                if (pwFrame_ >= kXWipeStart) {   // the new stage's scene appears from the left edge at 7/15 px per frame; the panel is already the new one
                    Image oldScene = frame_;
                    std::vector<Placed> sp; std::vector<int> si;
                    std::swap(sp, placed_); std::swap(si, inst_);
                    int sl = level_, sf = frames_; bool sh = holding_; Mode sm = mode_;
                    level_ = nextLevel_; frames_ = 0; holding_ = false; mode_ = Mode::Play;
                    dScore_ = twoPlayers_ ? playerScore_[player_ ^ 1] : score_; dBonus_ = startBonus(level_); dTime_ = 100;
                    previewPlayer_ = twoPlayers_ ? (player_ ^ 1) : -1;
                    refillPanel(pwFrame_ - kXWipeStart, dBonus_);
                    drawScene();
                    dFill_ = -1; for (int& c : dCounts_) c = -1;
                    previewPlayer_ = -1;
                    level_ = sl; frames_ = sf; holding_ = sh; mode_ = sm;
                    std::swap(sp, placed_); std::swap(si, inst_);
                    for (int y = 0; y < 200; y++)   // the effect copies the new screen over the old one
                        for (int x = 0; x < 320; x++)
                            if (!transitionShowsNew(transFx_, x, y, pwFrame_ - kXWipeStart)) { size_t i = size_t(y) * 320 + x; frame_.index[i] = oldScene.index[i]; frame_.rgb[i] = oldScene.rgb[i]; }
                }
            }
        } else if (pwFrame_ < kPwCount) { drawScene(); }
        else {
            drawScene();
            // The slats are strips over the whole height of the figure area (rows kStripY0..kStripY1): where a slat is open the
            // clean background and the panel show, elsewhere the finished figure is still there.
            Image scene = frame_;
            frame_ = boardPlay_;
            drawPanelOverlay();
            Image layer = frame_;
            frame_ = scene;
            if (closeFrame_ < 0) {   // blinds opening: slats grow around x = 24 + 16k
                int step = std::min(kPwOpenSteps - 1, (pwFrame_ - kPwOpenStart) / 6);
                int w = pwFrame_ < kPwOpenStart ? 0 : kOpenWidths[step];
                if (pwFrame_ >= kPwOpenStart)   // taller figures stick out of the strip: outside it the figure and the pieces are gone at once
                    for (int y = 0; y < 200; y++)
                        if (y < kStripY0 || y > kStripY1)
                            for (int x = 12; x < 218; x++) { size_t i = size_t(y) * 320 + x; frame_.index[i] = layer.index[i]; frame_.rgb[i] = layer.rgb[i]; }
                for (int y = kStripY0; y <= kStripY1; y++)
                    for (int x = 12; x < 218; x++)
                        if (slatVisible(x, 24, w)) { size_t i = size_t(y) * 320 + x; frame_.index[i] = layer.index[i]; frame_.rgb[i] = layer.rgb[i]; }
            } else {
                Image old = layer;
                // new stage behind the closing blinds
                if (level_ >= int(levels_.size())) {
                    frame_ = boardMenu_; drawMenuPanel(false);
                } else {
                    std::vector<Placed> sp; std::vector<int> si;
                    std::swap(sp, placed_); std::swap(si, inst_);
                    int sl = level_, sf = frames_; bool sh = holding_; Mode sm = mode_;
                    level_++; frames_ = 0; holding_ = false; mode_ = Mode::Play;
                    dScore_ = twoPlayers_ ? playerScore_[player_ ^ 1] : score_; dBonus_ = startBonus(level_); dTime_ = 100;
                    previewPlayer_ = twoPlayers_ ? (player_ ^ 1) : -1;
                    drawScene();
                    previewPlayer_ = -1;
                    level_ = sl; frames_ = sf; holding_ = sh; mode_ = sm;
                    std::swap(sp, placed_); std::swap(si, inst_);
                }
                for (int y = 0; y < 200; y++)   // the effect copies the new screen over the panel scene
                    for (int x = 0; x < 320; x++)
                        if (!transitionShowsNew(transFx_, x, y, closeFrame_)) { size_t i = size_t(y) * 320 + x; frame_.index[i] = old.index[i]; frame_.rgb[i] = old.rgb[i]; }
            }
        }
    } else if ((mode_ == Mode::AskReplay || mode_ == Mode::Play) && panelFx_ >= 0) {
        // the PLAY AGAIN panel comes and goes with effect 10 too (from the empty board, back to the restarted stage)
        const Mode now = mode_;
        mode_ = now == Mode::AskReplay ? Mode::AskDisk : Mode::AskReplay; drawScene();
        if (now == Mode::Play) drawPanelOverlay();
        const Image old = frame_;
        mode_ = now; drawScene(); drawPanelOverlay();
        int passes = 0; for (int k = 1; k <= 8; k++) passes += panelFx_ >= 6 * k - 3;
        for (int y = 0; y < 200; y++)
            for (int x = 0; x < 320; x++) {
                const int c = x % 16;
                if (c >= 8 - passes && c <= 7 + passes) continue;
                const size_t i = size_t(y) * 320 + size_t(x); frame_.index[i] = old.index[i]; frame_.rgb[i] = old.rgb[i];
            }
    } else if (mode_ == Mode::AskReplay) {
        drawScene(); drawPanelOverlay();
    } else if (mode_ == Mode::Play && pauseFx_ >= 0) {
        // effect 10 of the original (0x10d1ea): the new screen is copied in through a mask that opens from the middle of every
        // 16-pixel column, 2 more columns per pass; 8 passes, one every 6 frames (measured in the simulator)
        paused_ = !paused_; drawScene(); drawPanelOverlay(); const Image old = frame_;
        paused_ = !paused_; drawScene(); drawPanelOverlay();
        int passes = 0; for (int k = 1; k <= 8; k++) passes += pauseFx_ >= 6 * k - 3;
        for (int y = 0; y < 200; y++)
            for (int x = 0; x < 320; x++) {
                const int c = x % 16;
                if (c >= 8 - passes && c <= 7 + passes) continue;
                const size_t i = size_t(y) * 320 + size_t(x); frame_.index[i] = old.index[i]; frame_.rgb[i] = old.rgb[i];
            }
    } else {
        drawScene();
        drawPanelOverlay();
    }
    if (mode_ == Mode::Password && toBonusFade_ >= 0) fade(kFadeFrames - toBonusFade_, kFadeFrames);
    if (mode_ == Mode::Play && fadeIn_ < kFadeFrames) fade(fadeIn_, kFadeFrames);
    if (mode_ == Mode::Play && toFrame_ >= timeoutBlank()) { fade(0, 1); return; }   // blank frame before the question
    if (mode_ != Mode::Password && !(mode_ == Mode::Play && holding_ && !paused_))   // hidden from the end of the stage until the next one; 0x102872: a piece in hand replaces the pointer
        drawPointer(pics_[kDatCursor], play_);
    if (mode_ == Mode::AskDisk || mode_ == Mode::AskNew || mode_ == Mode::AskFormat) askFade();
}

}  // namespace tg
