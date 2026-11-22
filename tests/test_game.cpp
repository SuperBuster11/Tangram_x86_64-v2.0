// test_game <game.adf> <tests/data>: drives Game headlessly with Events: menu -> password -> solve levels with the stored solutions.
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include "../src/game.hpp"
#include "../src/scoring.hpp"
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
using namespace tg;
static std::vector<char> slurp(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<char>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); }
static double matchPercent(const Image& im, const std::vector<char>& shot) {
    size_t same = 0;
    for (int i = 0; i < 320 * 200; i++) same += im.rgb[i] == Rgb{uint8_t(shot[i * 3]), uint8_t(shot[i * 3 + 1]), uint8_t(shot[i * 3 + 2])};
    return 100.0 * same / (320 * 200);
}
static void click(Game& g, int x, int y, int b = 0) {
    Event m; m.type = Event::Move; m.x = x; m.y = y; g.event(m);
    Event d; d.type = Event::Down; d.x = x; d.y = y; d.button = b; g.event(d);
    Event u = d; u.type = Event::Up; g.event(u);
}
// scoredisk question: wait for the fade in, click, wait for the fade out
static void askClick(Game& g, int x, int y) { for (int i = 0; i < 26; i++) g.step(); click(g, x, y, 0); for (int i = 0; i < 26; i++) g.step(); }
static void press(Game& g, int x, int y) {   // left button down (picks a piece up)
    Event m; m.type = Event::Move; m.x = x; m.y = y; g.event(m);
    Event d; d.type = Event::Down; d.x = x; d.y = y; d.button = 0; g.event(d);
}
static void release(Game& g, int x, int y) {   // move and left button up (puts the piece down)
    Event m; m.type = Event::Move; m.x = x; m.y = y; g.event(m);
    Event u; u.type = Event::Up; u.x = x; u.y = y; u.button = 0; g.event(u);
}
static void settle(Game& g) { for (int i = 0; i < 30 && g.mode() == Game::Mode::Menu; i++) g.step(); for (int i = 0; i < 26 && g.mode() != Game::Mode::Menu; i++) g.step(); }   // the menu fades out, the game fades in (the stage clock starts after that)
static void typeKeys(Game& g, const std::string& s) { for (char c : s) { Event k; k.type = Event::Key; k.key = c; g.event(k); } }
static const int kTray[5][2] = {{262, 22}, {255, 72}, {296, 70}, {258, 102}, {305, 104}};
// solution lines: type rot cx cy
static void play(Game& g, const std::string& file) {
    std::ifstream f(file); int t, r, x, y;
    while (f >> t >> r >> x >> y) {
        press(g, kTray[t][0], kTray[t][1]);
        CHECK(g.heldPiece() == t);
        for (int i = 0; i < r; i++) click(g, x, y, 1);
        release(g, x, y);
        for (int i = 0; i < 10; i++) g.step();
    }
}
int main(int argc, char** argv) {
    const char* adfPath = argc >= 2 && argv[1][0] ? argv[1] : std::getenv("TANGRAM_ADF");
    std::string dir = argc >= 3 ? argv[2] : "tests/data", err;
    if (!adfPath) { std::printf("SKIP: set TANGRAM_ADF\n"); return 0; }
    Adf adf; CHECK(adf.loadFile(adfPath, err));
    Game g; CHECK(g.init(adf, err, false));
    CHECK(g.mode() == Game::Mode::Menu);
    g.step();
    double menuPct = matchPercent(g.frame(), slurp(dir + "/screen_menu.rgb"));
    std::printf("menu frame equals screen_menu.rgb: %.3f%%\n", menuPct);
    CHECK(menuPct > 97.5);
    // level 1, six pieces placed, 17 ticks (time 083): the whole frame must be nearly identical to the real game
    {
        Game g3; CHECK(g3.init(adf, err, false));
        Event e0; e0.type = Event::Key; e0.key = kKeyEnter; g3.event(e0); settle(g3);
        std::ifstream f(dir + "/level1_solution.txt"); int t, r, x, y, n = 0;
        while (n < 6 && f >> t >> r >> x >> y) {
            press(g3, kTray[t][0], kTray[t][1]);
            for (int i = 0; i < r; i++) click(g3, x, y, 1);
            release(g3, x, y); n++;
        }
        for (int i = 0; i < 17 * tickFrames(1) + 1; i++) g3.step();
        auto shot = slurp(dir + "/screen_6pieces.rgb");
        double pct = matchPercent(g3.frame(), shot);
        std::printf("6-piece play frame equals the real game in %.2f%% of pixels\n", pct);
        CHECK(pct > 99.5);
    }
    // start-up sequence: logo -> title -> credits -> menu; frames at full brightness must match the real game
    {
        Game gi; CHECK(gi.init(adf, err));
        CHECK(gi.mode() == Game::Mode::Intro && gi.introStage() == 0);
        for (int i = 0; i < 60; i++) gi.step();
        double p0 = matchPercent(gi.frame(), slurp(dir + "/intro_logo.rgb"));
        for (int i = 0; i < 2 * 25 + 220; i++) gi.step();
        CHECK(gi.introStage() == 1);
        for (int i = 0; i < 60; i++) gi.step();
        double p1 = matchPercent(gi.frame(), slurp(dir + "/intro_title.rgb"));
        Event click; click.type = Event::Down; click.x = 100; click.y = 100; gi.event(click);   // a click skips to the fade-out
        for (int i = 0; i < 2 * 25 + 5; i++) gi.step();
        CHECK(gi.introStage() == 2);
        for (int i = 0; i < 60; i++) gi.step();
        double p2 = matchPercent(gi.frame(), slurp(dir + "/intro_credits.rgb"));
        std::printf("intro frames equal the real game: logo %.2f%%, title %.2f%%, credits %.2f%%\n", p0, p1, p2);
        CHECK(p0 > 99.9 && p1 > 99.0 && p2 > 98.5);
        for (int i = 0; i < 400; i++) gi.step();
        CHECK(gi.mode() == Game::Mode::Menu);
        Game gs; CHECK(gs.init(adf, err));
        Event esc; esc.type = Event::Key; esc.key = kKeyEsc; gs.event(esc);   // Esc during the intro skips to the menu
        CHECK(gs.mode() == Game::Mode::Menu);
    }
    {   // menu: only the right mouse button or Enter starts the game; the buttons only select and a left click on the dragon does nothing
        Game gm; CHECK(gm.init(adf, err, false));
        click(gm, 240, 129);   // 2PLAYER
        CHECK(gm.mode() == Game::Mode::Menu);
        click(gm, 100, 120);   // dragon, left button
        CHECK(gm.mode() == Game::Mode::Menu);
        click(gm, 100, 120, 1);   // right button: the menu fades out first
        CHECK(gm.mode() == Game::Mode::Menu);
        for (int i = 0; i < 30 && gm.mode() == Game::Mode::Menu; i++) gm.step();
        CHECK(gm.mode() == Game::Mode::Play && gm.level() == 1);
    }
    // level 1 from the menu
    {
        Game g2; CHECK(g2.init(adf, err, false));
        Event e0; e0.type = Event::Key; e0.key = kKeyEnter; g2.event(e0); settle(g2);
        for (int i = 0; i < 5 * tickFrames(1); i++) g2.step();   // screen_play.rgb was captured with the time counter at 095
        auto shot = slurp(dir + "/screen_play.rgb");
        // counters of the panel: score, bonus, time, level label must be pixel-identical to the real play screen
        const int rows[4][3] = {{234, 119, 56}, {234, 133, 56}, {263, 174, 24}, {236, 189, 24}};
        int tot = 0, same = 0;
        for (auto& r : rows) for (int y = r[1]; y < r[1] + 7; y++) for (int x = r[0]; x < r[0] + r[2]; x++) {
            size_t i = size_t(y * 320 + x); tot++;
            same += g2.frame().rgb[i] == Rgb{uint8_t(shot[i * 3]), uint8_t(shot[i * 3 + 1]), uint8_t(shot[i * 3 + 2])};
        }
        std::printf("panel counters identical: %d of %d pixels\n", same, tot);
        CHECK(same == tot);
    }
    {   // typed password appears under the PASSWORD label, and the label itself stays where it is
        Game gm; CHECK(gm.init(adf, err, false));
        auto inkCount = [&](int x0, int y0, int x1, int y1) { int n = 0; for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) n += gm.frame().index[size_t(y * 320 + x)] == 0; return n; };
        int label0 = inkCount(232, 151, 290, 158), code0 = inkCount(242, 162, 284, 169);
        typeKeys(gm, "12345"); gm.step();
        CHECK(label0 > 50 && inkCount(232, 151, 290, 158) == label0);
        CHECK(inkCount(242, 162, 284, 169) > code0 + 20);
    }
    {   // music (0x1005fc / 0x100612): song 1 from start-up; M and F2 work only during a stage: M stops / restarts, F2 switches songs
        Game gs; CHECK(gs.init(adf, err, false));
        CHECK(!gs.musicMuted() && gs.musicSong() == 1);
        int16_t buf[2 * 2000];
        gs.audio(buf, 2000); CHECK(gs.song() == 1);
        Event m; m.type = Event::Key; m.key = 'M';
        Event f; f.type = Event::Key; f.key = kKeyF1 + 1;
        gs.event(m); gs.event(f); CHECK(!gs.musicMuted() && gs.musicSong() == 1);   // on the menu: nothing
        Game gp; CHECK(gp.init(adf, err, false));
        Event en0; en0.type = Event::Key; en0.key = kKeyEnter; gp.event(en0); settle(gp);
        gp.audio(buf, 2000); CHECK(gp.song() == 1);
        gp.event(m); gp.audio(buf, 2000); CHECK(gp.musicMuted() && gp.song() == 0);
        gp.event(m); gp.audio(buf, 2000); CHECK(!gp.musicMuted() && gp.song() == 1);
        gp.event(f); gp.audio(buf, 2000); CHECK(gp.musicSong() == 2 && gp.song() == 2);
        gp.event(f); gp.audio(buf, 2000); CHECK(gp.musicSong() == 1 && gp.song() == 1);
        gp.startBonusStage(2); gp.audio(buf, 2000); CHECK(gp.song() == 2);   // the bonus stage plays song 2 (user's wish; not in the original code)
        {   // a recording replaces a song: played at 25% from its start, repeated at its end; a bad file is refused
            auto wav = [](unsigned rate, unsigned ch, const std::vector<int16_t>& smp) {
                std::vector<uint8_t> d; auto p32 = [&](uint32_t v) { for (int i = 0; i < 4; i++) d.push_back(uint8_t(v >> (8 * i))); };
                auto p16 = [&](unsigned v) { d.push_back(uint8_t(v)); d.push_back(uint8_t(v >> 8)); };
                d.insert(d.end(), {'R', 'I', 'F', 'F'}); p32(uint32_t(36 + smp.size() * 2)); d.insert(d.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
                p32(16); p16(1); p16(ch); p32(rate); p32(rate * ch * 2); p16(ch * 2); p16(16); d.insert(d.end(), {'d', 'a', 't', 'a'}); p32(uint32_t(smp.size() * 2));
                for (int16_t v : smp) p16(uint16_t(v));
                return d;
            };
            Game gw; CHECK(gw.init(adf, err, false));
            CHECK(gw.loadMusicWav(1, wav(44100, 2, {400, -800, 1200, -1600, 2000, -2400}), err) && gw.musicFromWav(1) && !gw.musicFromWav(2));
            int16_t o[2 * 5]; gw.audio(o, 5);
            CHECK(o[0] == 100 && o[1] == -200 && o[4] == 500 && o[5] == -600 && o[6] == 100 && o[9] == -400);   // 3 frames, then from the start
            CHECK(gw.loadMusicWav(2, wav(22050, 1, {4000, 8000}), err));   // mono, half the rate: resampled
            CHECK(!gw.loadMusicWav(1, std::vector<uint8_t>(40, 0), err));
        }
        for (int k : {0, 2, 3, 4, 5, 6, 7, 8, 9}) { Event o; o.type = Event::Key; o.key = kKeyF1 + k; gs.event(o); }   // other F keys do nothing
        CHECK(gs.musicSong() == 1);
        CHECK(!gs.quitRequested());
        Event esc; esc.type = Event::Key; esc.key = kKeyEsc; gs.event(esc);   // Esc on the menu quits
        CHECK(gs.quitRequested());
    }
    {   // every one of the 200 codes (and the player's own list) selects its level
        Passwords pw; CHECK(pw.loadFromMlu(std::vector<unsigned char>(adf.file("TANGRAM.MLU")->begin(), adf.file("TANGRAM.MLU")->end()), err));
        for (int n = 1; n <= 200; n++) CHECK(pw.levelFor(pw.codeFor(n)) == n);
        std::ifstream cf(dir + "/cheat_list_from_user.txt"); int lv, code;
        while (cf >> lv >> code) { char b[8]; std::snprintf(b, sizeof b, "%d", code); int got = pw.levelFor(b);   /* as printed in the list: no leading zeros */ CHECK(got == lv || got == 170 || got == 141); }   // two list entries equal real codes of levels 170 and 141
    }
    {   // the code of stage 200 is rejected like in the original (stage 1); 75258 = stage 199; after stage 200 random EXP stages go on
        Game g2; CHECK(g2.init(adf, err, false));
        Event en2; en2.type = Event::Key; en2.key = kKeyEnter;
        typeKeys(g2, "95039"); g2.event(en2); settle(g2);
        CHECK(g2.mode() == Game::Mode::Play && g2.level() == 1);
        Game g3; CHECK(g3.init(adf, err, false));
        typeKeys(g3, "75258"); g3.event(en2); settle(g3);
        CHECK(g3.mode() == Game::Mode::Play && g3.level() == 199);
        for (int i = 0; i < 100; i++) g3.step();
        play(g3, dir + "/level199_solution.txt");
        CHECK(g3.mode() == Game::Mode::Password && g3.passwordShown() == "95039");
        for (int i = 0; i < 400; i++) g3.step();
        g3.event(en2);
        for (int i = 0; i < 400 && g3.mode() == Game::Mode::Password; i++) g3.step();   // the next stage comes in with one of the 11 effects
        CHECK(g3.mode() == Game::Mode::Play && g3.level() == 200);
        for (int i = 0; i < 100; i++) g3.step();
        play(g3, dir + "/level200_solution.txt");
        CHECK(g3.mode() == Game::Mode::Password);
        const int before = g3.score();
        for (int i = 0; i < 100; i++) g3.step();   // the bonus is counted, no password panel, no click
        CHECK(g3.mode() == Game::Mode::Password && g3.level() == 200);
        for (int i = 0; i < 100; i++) g3.step();   // fade to black, then Expert begins with a random stage that fades in
        CHECK(g3.mode() == Game::Mode::Play && g3.level() >= 1 && g3.level() <= 200 && g3.placedCount() == 0 && g3.expertMode());
        CHECK(g3.score() == before && !g3.quitRequested());
        std::printf("after stage 200: stage %d, score %d\n", g3.level(), g3.score());
    }
    {   // fades: to black at every change of scene (menu -> stage, bonus stage start, Expert after stage 200), none between two stages
        auto bright = [](const Game& gg) { long s = 0; for (const Rgb& c : gg.frame().rgb) s += c.r + c.g + c.b; return double(s) / (64000.0 * 765.0); };
        Game gf; CHECK(gf.init(adf, err, false));
        for (int i = 0; i < 400; i++) gf.step();
        double menuB = bright(gf), minB = 1;
        Event enf; enf.type = Event::Key; enf.key = kKeyEnter; gf.event(enf);
        for (int i = 0; i < 70; i++) { gf.step(); minB = std::min(minB, bright(gf)); }
        CHECK(gf.mode() == Game::Mode::Play && minB < 0.1 * menuB);   // the menu fades out, the stage fades in
        double playB = bright(gf); CHECK(playB > 0.1);
        play(gf, dir + "/level1_solution.txt");
        double minStage = 1;
        for (int i = 0; i < 400; i++) { gf.step(); minStage = std::min(minStage, bright(gf)); }
        gf.event(enf);
        for (int i = 0; i < 400 && gf.mode() == Game::Mode::Password; i++) { gf.step(); minStage = std::min(minStage, bright(gf)); }
        CHECK(gf.mode() == Game::Mode::Play && gf.level() == 2 && minStage > 0.6 * playB);   // no fade from stage to stage
        Game gb2; CHECK(gb2.init(adf, err, false));   // stage 10 -> bonus stage: fade out, fade in
        typeKeys(gb2, "07274"); gb2.event(enf); settle(gb2);
        gb2.setStagesDoneForTest(9);   // as if stages 1..9 had been played in this game
        play(gb2, dir + "/level10_solution.txt");
        for (int i = 0; i < 400; i++) gb2.step();
        gb2.event(enf);
        double minBonus = 1; bool sawBonus = false;
        for (int i = 0; i < 200; i++) { gb2.step(); minBonus = std::min(minBonus, bright(gb2)); sawBonus |= gb2.mode() == Game::Mode::Bonus; }
        CHECK(sawBonus && minBonus < 0.05);
        Game gx2; CHECK(gx2.init(adf, err, false));   // stage 200 -> Expert: fade to black first
        typeKeys(gx2, "75258"); gx2.event(enf); settle(gx2);
        play(gx2, dir + "/level199_solution.txt");
        for (int i = 0; i < 400; i++) gx2.step();
        gx2.event(enf);
        for (int i = 0; i < 400 && gx2.mode() == Game::Mode::Password; i++) gx2.step();   // the next stage comes in with one of the 11 effects
        play(gx2, dir + "/level200_solution.txt");
        double min200 = 1; for (int i = 0; i < 260; i++) { gx2.step(); min200 = std::min(min200, bright(gx2)); }
        CHECK(gx2.mode() == Game::Mode::Play && gx2.expertMode() && min200 < 0.05);
    }
    typeKeys(g, "99999");           // invalid code -> level 1 (like the original)
    Event en; en.type = Event::Key; en.key = kKeyEnter; g.event(en); settle(g);
    CHECK(g.mode() == Game::Mode::Play && g.level() == 1);
    for (int i = 0; i < 100; i++) g.step();
    play(g, dir + "/level1_solution.txt");
    CHECK(g.mode() == Game::Mode::Password);
    CHECK(g.passwordShown() == "03797");   // password of level 2
    int expect = stageGain(1, elapsedTicks(g.frames(), 1));
    CHECK(g.score() == expect);
    std::printf("level 1 solved, score %d, next code %s\n", g.score(), g.passwordShown().c_str());
    {   // blinds: compare the panel rectangle with real frames (k = frames after the start of the count-down + 156)
        auto cmpPanel = [&](const Game& gg, const std::string& file) {
            auto shot = slurp(dir + "/" + file);
            int tot = 0, same = 0;
            for (int y = 62; y < 144; y++) for (int x = 12; x < 218; x++) {
                size_t i = size_t(y * 320 + x); tot++;
                same += gg.frame().rgb[i] == Rgb{uint8_t(shot[i * 3]), uint8_t(shot[i * 3 + 1]), uint8_t(shot[i * 3 + 2])};
            }
            return 100.0 * same / tot;
        };
        for (int k : {10, 20, 30, 40, 60}) {
            while (g.transitionFrame() < 156 + k) g.step();
            char name[64]; std::snprintf(name, sizeof name, "blinds_open_%02d.rgb", k);
            double pct = cmpPanel(g, name);
            std::printf("blinds opening k=%d: panel %.1f%% identical\n", k, pct);
            CHECK(pct > 93.0);
        }
        // a click before the panel is complete is ignored, afterwards it starts the closing blinds
        CHECK(g.mode() == Game::Mode::Password && g.transitionBusy() == false);
    }
    g.event(en);
    CHECK(g.mode() == Game::Mode::Password && g.closeFrame() == 0);
    for (int i = 0; i < 400 && g.mode() == Game::Mode::Password; i++) g.step();   // the next stage comes in with one of the 11 effects
    CHECK(g.mode() == Game::Mode::Play && g.level() == 2);
    // password entry: level 10 and its stored solution
    Event esc; esc.type = Event::Key; esc.key = kKeyEsc; g.event(esc);
    CHECK(g.mode() == Game::Mode::AskDisk);   // "PLAY WITH SCOREDISK"
    {
        // panel with the question: compare the panel rectangle with the real screen (text and buttons only; the shadow under the panel differs)
        { Event mv; mv.type = Event::Move; mv.x = 90; mv.y = 130; g.event(mv); }   // the pointer was here in the real screenshot
        for (int i = 0; i < 26; i++) g.step();   // 0x104a74: the question fades in
        auto shot = slurp(dir + "/screen_scoredisk.rgb"); int tot = 0, same = 0;
        for (int y = 70; y < 117; y++) for (int x = 12; x < 218; x++) { size_t i = size_t(y * 320 + x); tot++;
            same += g.frame().rgb[i] == Rgb{uint8_t(shot[i * 3]), uint8_t(shot[i * 3 + 1]), uint8_t(shot[i * 3 + 2])}; }
        std::printf("scoredisk panel equals the real game in %.2f%% of pixels\n", 100.0 * same / tot);
        CHECK(100.0 * same / tot > 99.0);
        g.event(esc);   // Esc does nothing on this panel (as in the original)
        CHECK(g.mode() == Game::Mode::AskDisk);
        click(g, 130, 108);   // NO: the table is only shown (after the question has faded out)
        CHECK(g.mode() == Game::Mode::AskDisk);
        for (int i = 0; i < 25; i++) g.step();
        CHECK(g.mode() == Game::Mode::HighScores);
        for (int i = 0; i < 35; i++) g.step();   // black while the picture loads, then a short fade in
        auto hs = slurp(dir + "/screen_highscores.rgb"); size_t same2 = 0;
        for (int i = 0; i < 320 * 200; i++) same2 += g.frame().rgb[size_t(i)] == Rgb{uint8_t(hs[size_t(i) * 3]), uint8_t(hs[size_t(i) * 3 + 1]), uint8_t(hs[size_t(i) * 3 + 2])};
        g.step();
        for (size_t i = 0; i < 320 * 200; i++) { (void)i; break; }
        same2 = 0; for (int i = 0; i < 320 * 200; i++) same2 += g.frame().rgb[size_t(i)] == Rgb{uint8_t(hs[size_t(i) * 3]), uint8_t(hs[size_t(i) * 3 + 1]), uint8_t(hs[size_t(i) * 3 + 2])};
        std::printf("high-score screen equals the real game in %.3f%% of pixels\n", 100.0 * same2 / 64000);
        CHECK(same2 > 63990);
        for (int i = 0; i < 340; i++) g.step();   // shown for about 6 s, fades out, then the menu
    }
    CHECK(g.mode() == Game::Mode::Menu);
    {   // the bonus stage follows the 10th stage completed in this game, not stage 10 (checked in the simulator): start at 10, solve it -> stage 11
        Game gn; CHECK(gn.init(adf, err, false));
        typeKeys(gn, "07274"); gn.event(en); settle(gn);
        gn.setStagesDoneForTest(1);   // as if the game had been started at 9 and stage 9 solved
        play(gn, dir + "/level10_solution.txt");
        for (int i = 0; i < 400; i++) gn.step();
        gn.event(en);
        for (int i = 0; i < 400 && gn.mode() == Game::Mode::Password; i++) gn.step();   // the next stage comes in with one of the 11 effects
        CHECK(gn.mode() == Game::Mode::Play && gn.level() == 11);
    }
    typeKeys(g, "07274"); g.event(en); settle(g);
    CHECK(g.mode() == Game::Mode::Play && g.level() == 10);
    g.setStagesDoneForTest(9);   // as if stages 1..9 had been played in this game
    play(g, dir + "/level10_solution.txt");
    CHECK(g.mode() == Game::Mode::Password && g.level() == 10);
    CHECK(g.passwordShown() == "37322");
    for (int i = 0; i < 400; i++) g.step();
    g.event(en);
    for (int i = 0; i < 400 && g.mode() == Game::Mode::Password; i++) g.step();   // the next stage comes in with one of the 11 effects
    CHECK(g.mode() == Game::Mode::Bonus);   // every 10th completed stage is followed by the bonus stage
    for (int i = 0; i < 5400; i++) g.step();   // time runs out (101 ticks of 51 frames) -> level 11
    // wrong placement is not accepted: level 11, one piece only
    CHECK(g.level() == 11);
    press(g, kTray[1][0], kTray[1][1]); release(g, 100, 100);
    CHECK(g.mode() == Game::Mode::Play && g.placedCount() == 1);
    {   // drag and drop: press picks the piece up, release puts it down
        Event d; d.type = Event::Down; d.x = 100; d.y = 100; d.button = 0; g.event(d);
        CHECK(g.heldPiece() == 1 && g.placedCount() == 0);
        Event m; m.type = Event::Move; m.x = 60; m.y = 80; g.event(m);
        CHECK(g.heldPiece() == 1);
        Event u; u.type = Event::Up; u.x = 60; u.y = 80; u.button = 0; g.event(u);
        CHECK(g.heldPiece() == -1 && g.placedCount() == 1);
        Event d2; d2.type = Event::Down; d2.x = kTray[0][0]; d2.y = kTray[0][1]; d2.button = 0; g.event(d2);
        Event u2 = d2; u2.type = Event::Up; g.event(u2);   // released outside the figure area: back to the tray
        CHECK(g.heldPiece() == -1 && g.placedCount() == 1);
        // every piece, not only the first, needs the button held: a plain click does not leave it in hand
        press(g, kTray[0][0], kTray[0][1]); CHECK(g.heldPiece() == 0);
        release(g, kTray[0][0], kTray[0][1]); CHECK(g.heldPiece() == -1);
        press(g, kTray[3][0], kTray[3][1]); release(g, 50, 60); press(g, kTray[0][0], kTray[0][1]);
        CHECK(g.heldPiece() == 0); release(g, 70, 90); CHECK(g.placedCount() == 3);
    }
    {   // bonus stage (code of the original): first one of the game uses the checkerboard layout (DAT 13), 25 pairs, all tiles free;
        // a pair gives stage+10 points; afterwards the time left runs in like after a stage; the counter goes to the score at the next stage
        Game gb; CHECK(gb.init(adf, err, false));
        gb.startBonusStage(11);
        const int lv = gb.level(), perPair = lv + 10, timeSteps = stageGain(lv, 0) - startBonus(lv);
        CHECK(gb.mode() == Game::Mode::Bonus && gb.bonusPairsLeft() == 25 && gb.bonusLayout() == 13);
        for (int i = 0; i < 100; i++) CHECK((gb.bonusIcon(i) != 0) == (((i / 10) + (i % 10)) % 2 == 0));   // the checkerboard
        gb.step();
        auto clickTile = [&](Game& g, int i) { int x, y; g.bonusTile(i, x, y); click(g, x + 8, y + 8); };
        clickTile(gb, 0); clickTile(gb, 0);   // select, deselect
        CHECK(gb.bonusPairsLeft() == 25 && gb.bonusPoints() == 0);
        int firstPair = -1, other = -1;
        for (int b2 = 1; b2 < 100; b2++) if (gb.bonusIcon(b2) && gb.bonusIcon(0) == gb.bonusIcon(b2)) { firstPair = b2; break; }
        for (int b2 = 1; b2 < 100; b2++) if (gb.bonusIcon(b2) && gb.bonusIcon(b2) != gb.bonusIcon(0)) { other = b2; break; }
        clickTile(gb, 0); clickTile(gb, other);   // another sprite: nothing happens
        CHECK(gb.bonusPairsLeft() == 25 && gb.bonusPoints() == 0);
        clickTile(gb, 0); clickTile(gb, firstPair);
        CHECK(gb.bonusPairsLeft() == 24 && gb.bonusPoints() == perPair);
        for (int a2 = 0; a2 < 100; a2++) for (int b2 = a2 + 1; b2 < 100; b2++) if (gb.bonusIcon(a2) && gb.bonusIcon(a2) == gb.bonusIcon(b2)) { clickTile(gb, a2); clickTile(gb, b2); }
        CHECK(gb.bonusPairsLeft() == 0 && gb.bonusPoints() == 25 * perPair);
        for (int i = 0; i < 400 && gb.mode() == Game::Mode::Bonus; i++) gb.step();
        CHECK(gb.mode() == Game::Mode::Play && gb.level() == 11 && gb.score() == 25 * perPair + timeSteps);
        // the second bonus stage of the game uses the next layout (DAT 14: a spiral, only the edge tiles may be picked at first)
        gb.startBonusStage(12);
        CHECK(gb.bonusLayout() == 14 && gb.bonusPairsLeft() > 0);
        for (int i = 0; i < 100; i++) if (gb.bonusIcon(i)) { const int r = i / 10, c = i % 10; if (r > 0 && r < 9 && c > 0 && c < 9 && gb.bonusIcon(i - 1) && gb.bonusIcon(i + 1) && gb.bonusIcon(i - 10) && gb.bonusIcon(i + 10)) CHECK(!gb.bonusFree(i)); }
        // time limit: 101 ticks of 51 frames
        Game gt; CHECK(gt.init(adf, err, false)); gt.startBonusStage(21);
        for (int i = 0; i < 25 + 101 * 51 - 2; i++) gt.step();
        CHECK(gt.mode() == Game::Mode::Bonus);
        for (int i = 0; i < 100 && gt.mode() == Game::Mode::Bonus; i++) gt.step();
        CHECK(gt.mode() == Game::Mode::Play && gt.level() == 21 && gt.score() == 0);   // time out, no pairs, no time left
    }
    {   // score disk: YES + a score that qualifies -> the name is typed into the table (and the table is flagged for saving)
        Game gh; CHECK(gh.init(adf, err, false));
        const int lvh = gh.level();
        gh.startBonusStage(11);
        auto clickTile = [&](int i) { int x, y; gh.bonusTile(i, x, y); click(gh, x + 8, y + 8); };
        for (int a2 = 0; a2 < 100; a2++) for (int b2 = a2 + 1; b2 < 100; b2++) if (gh.bonusIcon(a2) && gh.bonusIcon(a2) == gh.bonusIcon(b2)) { clickTile(a2); clickTile(b2); }
        for (int i = 0; i < 400 && gh.mode() == Game::Mode::Bonus; i++) gh.step();
        CHECK(gh.mode() == Game::Mode::Play && gh.score() == 25 * (lvh + 10) + stageGain(lvh, 0) - startBonus(lvh));
        const int scH = gh.score();
        Event e0; e0.type = Event::Key; e0.key = kKeyEsc; gh.event(e0);
        CHECK(gh.mode() == Game::Mode::AskDisk);
        gh.event(e0); CHECK(gh.mode() == Game::Mode::AskDisk);   // Esc again: nothing, there is no way back
        click(gh, 95, 108); CHECK(gh.mode() == Game::Mode::AskDisk);   // during the fade in: ignored
        for (int i = 0; i < 25; i++) gh.step();
        click(gh, 95, 108);   // YES -> "CREATE NEW SCORE DISK" (fade out, fade in)
        for (int i = 0; i < 25; i++) gh.step();
        CHECK(gh.mode() == Game::Mode::AskNew);
        for (int i = 0; i < 25; i++) gh.step();
        click(gh, 130, 108);   // NO: the existing table is used
        for (int i = 0; i < 25; i++) gh.step();
        CHECK(gh.mode() == Game::Mode::NameEntry);
        typeKeys(gh, "ANNA"); Event bs; bs.type = Event::Key; bs.key = kKeyBackspace; gh.event(bs); typeKeys(gh, "A1");
        Event en2; en2.type = Event::Key; en2.key = kKeyEnter; gh.event(en2);
        for (int i = 0; i < 40 && gh.mode() != Game::Mode::Menu; i++) gh.step();   // fades out, then the menu
        CHECK(gh.mode() == Game::Mode::Menu && gh.takeScoresDirty() && !gh.takeScoresDirty());
        bool found = false; for (const auto& r : gh.highScores().a) found |= r.name == "ANNA1" && r.score == scH && r.level == 11;   // stored as typed, not padded
        CHECK(found);
        Game g3; CHECK(g3.init(adf, err, false)); g3.loadScores(gh.saveScores());   // persistence round trip
        found = false; for (const auto& r : g3.highScores().a) found |= r.name == "ANNA1";
        CHECK(found);
        // a score of 0 does not qualify: the table is shown without a name entry
        Game g4; CHECK(g4.init(adf, err, false)); g4.startLevel(1);
        Event e4; e4.type = Event::Key; e4.key = kKeyEsc; g4.event(e4); askClick(g4, 95, 108); askClick(g4, 130, 108);
        CHECK(g4.mode() == Game::Mode::HighScores);
        // YES, YES: "INSERT FORMATTED DISK" - NO does nothing, YES creates a fresh table
        Game g5; CHECK(g5.init(adf, err, false)); g5.startLevel(1);
        g5.event(e4); askClick(g5, 95, 108); askClick(g5, 95, 108);
        CHECK(g5.mode() == Game::Mode::AskFormat);
        askClick(g5, 130, 108); g5.event(e4); CHECK(g5.mode() == Game::Mode::AskFormat);
        CHECK(!g5.takeScoresDirty());
        askClick(g5, 95, 108);
        CHECK(g5.mode() == Game::Mode::HighScores && g5.takeScoresDirty());
        // the time running out ends the game like Esc (the bonus counter is lost)
        Game g6; CHECK(g6.init(adf, err, false)); g6.startLevel(1);
        for (int i = 0; i < tickFrames(1) * 100 - 1; i++) g6.step();
        CHECK(g6.mode() == Game::Mode::Play);
        for (int i = 0; i < 3; i++) g6.step();
        // time is up: the bonus counts down (measured in the original: 1 point per 75 frames for stage 1), the stage is not over yet
        CHECK(g6.mode() == Game::Mode::Play);
        for (int i = 0; i < 24 * tickFrames(1); i++) g6.step();
        CHECK(g6.mode() == Game::Mode::Play);
        for (int i = 0; i < 30; i++) g6.step();
        CHECK(g6.mode() == Game::Mode::AskDisk);
        CHECK(g6.score() == 0);
        // Esc during the bonus stage also asks about the score disk
        Game g7; CHECK(g7.init(adf, err, false)); g7.startBonusStage(11);
        g7.event(e4); CHECK(g7.mode() == Game::Mode::AskDisk);
        for (int i = 0; i < 3; i++) g7.step();
        askClick(g7, 130, 108); CHECK(g7.mode() == Game::Mode::HighScores);
    }
    {   // time out with at least 50 x stage points: "PLAY AGAIN FOR nnnn POINTS" (0x103c7e); YES pays and restarts the stage with bonus 0, NO -> score disk question
        for (int choice = 0; choice < 2; choice++) {
            Game gr; CHECK(gr.init(adf, err, false));
            Event en0; en0.type = Event::Key; en0.key = kKeyEnter; gr.event(en0); settle(gr);
            gr.setScoreForTest(120);
            for (int i = 0; i < 100 * gr.stageTick() + 25 * gr.stageTick() + 100 && gr.mode() == Game::Mode::Play; i++) gr.step();
            CHECK(gr.mode() == Game::Mode::AskReplay);
            for (int i = 0; i < 50; i++) gr.step();
            if (choice == 0) {
                click(gr, 98, 109);
                CHECK(gr.mode() == Game::Mode::Play && gr.score() == 70 && gr.level() == 1);
                for (int i = 0; i < 50; i++) gr.step();
                play(gr, dir + "/level1_solution.txt");
                CHECK(gr.mode() == Game::Mode::Password);
                CHECK(gr.score() == 70 + stageGain(1, gr.frames() / gr.stageTick()) - startBonus(1));   // no 25 x stage this time
            } else {
                click(gr, 130, 109);
                CHECK(gr.mode() == Game::Mode::AskDisk);
            }
        }
        {   // after the time-out the stage goes on while the bonus runs down: pieces can still be put down (solved -> what is left of the bonus) and Esc works
            Game go; CHECK(go.init(adf, err, false));
            Event en0; en0.type = Event::Key; en0.key = kKeyEnter; go.event(en0); settle(go);
            for (int i = 0; i < 100 * go.stageTick() + 5 * go.stageTick() + 10; i++) go.step();   // 5 ticks into the count-down
            CHECK(go.mode() == Game::Mode::Play);
            play(go, dir + "/level1_solution.txt");
            CHECK(go.mode() == Game::Mode::Password && go.score() > 0 && go.score() <= startBonus(1) - 5);
            Game gq; CHECK(gq.init(adf, err, false)); gq.event(en0); settle(gq);
            for (int i = 0; i < 100 * gq.stageTick() + 3 * gq.stageTick(); i++) gq.step();
            Event esc; esc.type = Event::Key; esc.key = kKeyEsc; gq.event(esc);
            CHECK(gq.mode() == Game::Mode::AskDisk);
        }
        {   // too few points: straight to the score disk question
            Game gs; CHECK(gs.init(adf, err, false));
            Event en0; en0.type = Event::Key; en0.key = kKeyEnter; gs.event(en0); settle(gs);
            gs.setScoreForTest(49);
            for (int i = 0; i < 130 * gs.stageTick() && gs.mode() == Game::Mode::Play; i++) gs.step();
            CHECK(gs.mode() == Game::Mode::AskDisk);
        }
    }
    {   // two players, time out with too few points (0x1040ee): the player who drops out sees the table, then the other one goes on alone on the next stage
        Game g2; CHECK(g2.init(adf, err, false));
        click(g2, 240, 129);   // 2PLAYER
        Event en0; en0.type = Event::Key; en0.key = kKeyEnter; g2.event(en0); settle(g2);
        CHECK(g2.player() == 1 && g2.level() == 1);
        for (int i = 0; i < 130 * g2.stageTick() && g2.mode() == Game::Mode::Play; i++) g2.step();
        CHECK(g2.mode() == Game::Mode::AskDisk);   // first game end of the session: the score disk question
        for (int i = 0; i < 3; i++) g2.step();
        askClick(g2, 130, 108);   // NO -> the table
        CHECK(g2.mode() == Game::Mode::HighScores);
        click(g2, 100, 100);
        for (int i = 0; i < 200 && g2.mode() == Game::Mode::HighScores; i++) g2.step();
        CHECK(g2.mode() == Game::Mode::Play && g2.level() == 2 && g2.player() == 1);
        // the next game end in this session: no question any more, straight to the table (checked in the simulator)
        Event esc; esc.type = Event::Key; esc.key = kKeyEsc;
        for (int i = 0; i < 30; i++) g2.step();
        g2.event(esc);
        CHECK(g2.mode() == Game::Mode::HighScores);
    }
    {   // a placed piece can be picked up on every pixel it covers, also the bottom corner of the square (diamond)
        Game gq; CHECK(gq.init(adf, err, false));
        Event en0; en0.type = Event::Key; en0.key = kKeyEnter; gq.event(en0); settle(gq);
        press(gq, kTray[1][0], kTray[1][1]); release(gq, 100, 100);   // square: corners (100,80) (120,100) (100,120) (80,100)
        CHECK(gq.placedCount() == 1);
        press(gq, 100, 120); CHECK(gq.heldPiece() == 1 && gq.placedCount() == 0);
        release(gq, 100, 100);
        press(gq, 120, 100); CHECK(gq.heldPiece() == 1);   // right corner
        release(gq, 100, 100);
    }
    {   // the parallelogram on the tray can be taken by its bottom corner (y 141..148)
        Game gt; CHECK(gt.init(adf, err, false));
        Event en0; en0.type = Event::Key; en0.key = kKeyEnter; gt.event(en0); settle(gt);
        press(gt, 297, 146); CHECK(gt.heldPiece() == 4);
    }
    {   // 0x10242a: a piece sticking out of x 6..221 / y 6..193 is not put down (here it goes back to the tray)
        Game gb; CHECK(gb.init(adf, err, false));
        Event en0; en0.type = Event::Key; en0.key = kKeyEnter; gb.event(en0); settle(gb);
        press(gb, kTray[1][0], kTray[1][1]); release(gb, 12, 100);   // the square (half width 20) would stick out on the left
        CHECK(gb.placedCount() == 0 && gb.heldPiece() == -1);
        press(gb, kTray[1][0], kTray[1][1]); release(gb, 40, 100);
        CHECK(gb.placedCount() == 1);
    }
    {   // pause (0x10433e): Space; the panel is blended in through opening columns (46 frames); Esc is ignored; a click or Space ends it; no clock meanwhile
        Game gp; CHECK(gp.init(adf, err, false));
        Event en0; en0.type = Event::Key; en0.key = kKeyEnter; gp.event(en0); settle(gp);
        for (int i = 0; i < 10; i++) gp.step();
        const int f0 = gp.frames();
        Event sp; sp.type = Event::Key; sp.key = ' '; gp.event(sp);
        gp.step(); const Image a = gp.frame();
        for (int i = 0; i < 20; i++) gp.step();
        const Image b = gp.frame();
        for (int i = 0; i < 40; i++) gp.step();
        const Image c = gp.frame();
        int ab = 0, bc = 0; for (int i = 0; i < 64000; i++) { ab += !(a.rgb[i] == b.rgb[i]); bc += !(b.rgb[i] == c.rgb[i]); }
        CHECK(ab > 0 && bc > 0);   // still changing halfway, finished later
        Event esc; esc.type = Event::Key; esc.key = kKeyEsc; gp.event(esc); CHECK(gp.mode() == Game::Mode::Play);
        click(gp, 100, 100);
        for (int i = 0; i < 60; i++) gp.step();
        CHECK(gp.frames() <= f0 + 1 + 60 - 45);   // the clock stood still while paused and while the panel went away
    }
    {   // Expert: a random stage (bonus = 25 x stage), the label shows EXP instead of the stage number
        Game ge; CHECK(ge.init(adf, err, false));
        click(ge, 245, 100);   // EXPERT
        click(ge, 100, 120, 1); settle(ge);   // right button
        CHECK(ge.mode() == Game::Mode::Play && ge.level() >= 1 && ge.level() <= 200);
        CHECK(ge.stageTick() == 61);   // Expert clock (measured in the simulator): 61 frames on the first stage, then faster every stage down to 38
        ge.startLevel(189); ge.step();
        CHECK(ge.stageTick() == 58);
        auto shot = slurp(dir + "/screen_expert189.rgb"); int tot = 0, same = 0;
        for (int y = 0; y < 200; y++) for (int x = 0; x < 225; x++) { size_t k = size_t(y * 320 + x); tot++;   // figure area of the real screenshot (stage 189)
            same += ge.frame().rgb[k] == Rgb{uint8_t(shot[k * 3]), uint8_t(shot[k * 3 + 1]), uint8_t(shot[k * 3 + 2])}; }
        std::printf("expert figure area equals the real game in %.2f%% of pixels\n", 100.0 * same / tot);
        CHECK(100.0 * same / tot > 98.0);
        tot = same = 0;
        for (int y = 112; y < 200; y++) for (int x = 232; x < 318; x++) { if (y > 140 && y < 186) continue; size_t k = size_t(y * 320 + x); tot++;   // counters and label
            same += ge.frame().rgb[k] == Rgb{uint8_t(shot[k * 3]), uint8_t(shot[k * 3 + 1]), uint8_t(shot[k * 3 + 2])}; }
        std::printf("expert panel counters equal the real game in %.2f%% of pixels\n", 100.0 * same / tot);
        CHECK(100.0 * same / tot > 97.0);
    }
    {   // 0x103120: count-down after a solved stage: time runs out while the bonus counter grows (score unchanged), then the bonus goes
        // over to the score in steps of 1000/100/10/1
        Game gc; CHECK(gc.init(adf, err, false)); gc.startLevel(1);
        for (int i = 0; i < 30 * gc.stageTick(); i++) gc.step();   // 30 ticks gone
        play(gc, dir + "/level1_solution.txt");
        CHECK(gc.mode() == Game::Mode::Password);
        std::vector<std::array<int, 3>> v;
        for (int i = 0; i < 160; i++) { v.push_back(gc.shownNumbers()); gc.step(); }
        const int s0 = v[0][2]; bool phase1 = true, ok = true; int maxB = 0;
        for (size_t i = 1; i < v.size(); i++) {
            if (phase1 && v[i][0] == 0 && v[i][1] < v[i - 1][1]) phase1 = false;
            if (phase1) ok &= v[i][2] == s0 && v[i][1] >= v[i - 1][1] && v[i][0] <= v[i - 1][0];
            else ok &= v[i][1] <= v[i - 1][1] && v[i][2] >= v[i - 1][2] && v[i][1] + v[i][2] == v[i - 1][1] + v[i - 1][2];
            maxB = std::max(maxB, v[i][1]);
        }
        CHECK(ok && !phase1 && v.back()[1] == 0 && v.back()[2] == gc.score() && maxB == gc.score() - s0 && v[0][1] >= 25 && v[0][1] < 30);
    }
    {   // right button held: one step on the press, the next after 12 frames, then every 6 frames; released: no more
        Game gr; CHECK(gr.init(adf, err, false)); gr.startLevel(1);
        for (int i = 0; i < 30; i++) gr.step();
        press(gr, kTray[0][0], kTray[0][1]);
        Event d; d.type = Event::Down; d.button = 1; d.x = 100; d.y = 100; gr.event(d);
        CHECK(gr.heldRotation() == 1);
        for (int i = 0; i < 11; i++) gr.step();
        CHECK(gr.heldRotation() == 1);
        gr.step(); CHECK(gr.heldRotation() == 2);
        for (int i = 0; i < 6; i++) gr.step();
        CHECK(gr.heldRotation() == 3);
        Event u = d; u.type = Event::Up; gr.event(u);
        for (int i = 0; i < 30; i++) gr.step();
        CHECK(gr.heldRotation() == 3);
    }
    {   // 0x10d074 effects 1..11 (the next stage over the whole screen): nothing new at the start, everything new at the end, half way a mix
        for (int fx = 1; fx <= 11; fx++) {
            const int n = Game::transitionFrames(fx);
            int at0 = 0, half = 0, end = 0;
            for (int y = 0; y < 200; y++) for (int x = 0; x < 320; x++) {
                at0 += Game::transitionShowsNew(fx, x, y, -1); half += Game::transitionShowsNew(fx, x, y, n / 2); end += Game::transitionShowsNew(fx, x, y, n);
            }
            std::printf("effect %d: %d frames, half way %.0f%% new\n", fx, n, half / 640.0);
            CHECK(at0 == 0 && end == 64000 && half > 64000 / 5 && half < 64000 * 4 / 5);
        }
    }
    {   // 0x101d78: a tray piece is picked up inside its area where the pixel is grey (colours 20..24) or black (0), e.g. the lower tip of
        // the diamond (y 87) and the bottom corner of the parallelogram (296,147); the parchment around the pieces does nothing
        Game gp; CHECK(gp.init(adf, err, false)); gp.startLevel(1);
        const int pts[][3] = {{254, 87, 1}, {255, 50, 1}, {238, 68, 1}, {272, 68, 1}, {262, 6, 0}, {300, 86, 2}, {258, 105, 3}, {300, 134, 4}, {312, 128, 4}, {296, 147, 4},
                              {305, 140, -1}, {254, 88, -1}, {300, 40, -1}};
        for (const auto& pt : pts) { press(gp, pt[0], pt[1]); if (gp.heldPiece() != pt[2]) std::printf("tray point %d,%d -> %d\n", pt[0], pt[1], gp.heldPiece()); CHECK(gp.heldPiece() == pt[2]); release(gp, pt[0], pt[1]); }
    }
    {   // after any stage the figure and the pieces are gone outside the blinds strip once the password panel opens (tall figures, stages 10 and 20)
        std::vector<Rgb> ref;
        for (int lvl : {10, 20}) {
            Game gt; CHECK(gt.init(adf, err, false));
            gt.startLevel(lvl);
            play(gt, dir + "/level" + std::to_string(lvl) + "_solution.txt");
            CHECK(gt.mode() == Game::Mode::Password);
            while (gt.transitionFrame() < 162 + 6 * 8) gt.step();
            std::vector<Rgb> out;
            for (int y = 0; y < 200; y++) if (y < 62 || y > 143) for (int x = 12; x < 218; x++) out.push_back(gt.frame().rgb[size_t(y * 320 + x)]);
            if (ref.empty()) ref = out;
            else { size_t same = 0; for (size_t k = 0; k < out.size(); k++) same += out[k] == ref[k]; CHECK(same == out.size()); }
        }
    }
    {   // Expert after a solved stage: no password panel, no click; the next random stage is wiped in from the left (stage 172 in the real run)
        Game gx; CHECK(gx.init(adf, err, false));
        click(gx, 245, 100); click(gx, 100, 120);
        gx.startLevel(1);
        play(gx, dir + "/level1_solution.txt");
        CHECK(gx.mode() == Game::Mode::Password);
        gx.setNextLevelForTest(172);
        gx.setTransitionEffectForTest(5);   // the real run showed effect 5 (columns from the left)
        Event enx; enx.type = Event::Key; enx.key = kKeyEnter; gx.event(enx);   // clicks and keys do nothing here
        {   // the panel refill against real frames (shots 8 frames apart in the original): piece counts, time bar, time and stage label
            const struct { const char* file; int pw; } shots[] = {{"screen_beyond200_i24.rgb", 249}, {"screen_beyond200_i30.rgb", 297}, {"screen_beyond200_i36.rgb", 345}};
            for (auto& s : shots) {
                while (gx.transitionFrame() < s.pw) gx.step();
                auto ref = slurp(dir + "/" + s.file); int bad = 0;
                const int regs[2][4] = {{230, 320, 18, 112}, {230, 320, 148, 178}};
                for (auto& r : regs) for (int y = r[2]; y < r[3]; y++) for (int x = r[0]; x < r[1]; x++) { size_t k = size_t(y * 320 + x);
                    bad += !(gx.frame().rgb[k] == Rgb{uint8_t(ref[k * 3]), uint8_t(ref[k * 3 + 1]), uint8_t(ref[k * 3 + 2])}); }
                std::printf("panel refill at %d: %d differing pixels\n", s.pw, bad);
                CHECK(bad <= 60);
            }
        }
        // the effect against two simulator frames, in a second game (the refill shots above went past these moments)
        Game gw; CHECK(gw.init(adf, err, false));
        click(gw, 245, 100); click(gw, 100, 120);
        gw.startLevel(1);
        play(gw, dir + "/level1_solution.txt");
        gw.setNextLevelForTest(172); gw.setTransitionEffectForTest(5);
        int at = 0;
        for (const char* file : {"screen_expert_wipe100.rgb", "screen_expert_wipe244.rgb"}) {
            int want = at ? 244 : 100;
            // the frames came from the simulator, whose CPU is slower: same column reached (x = 22 + 7/15 px per simulator frame)
            const int col = 22 + want * 7 / 15;
            while (!Game::transitionShowsNew(5, col, 100, gw.transitionFrame() - 225)) gw.step();
            at = 1;
            auto shot = slurp(dir + "/" + file); int tot = 0, same = 0;
            for (int y = 0; y < 200; y++) for (int x = 12; x < 218; x++) { size_t k = size_t(y * 320 + x); tot++;
                same += gw.frame().rgb[k] == Rgb{uint8_t(shot[k * 3]), uint8_t(shot[k * 3 + 1]), uint8_t(shot[k * 3 + 2])}; }
            std::printf("expert wipe t=%d: figure area %.2f%% identical\n", want, 100.0 * same / tot);
            CHECK(100.0 * same / tot > 96.0);
        }
        while (gx.mode() == Game::Mode::Password) gx.step();
        CHECK(gx.mode() == Game::Mode::Play && gx.level() == 172 && gx.score() > 0);
    }
    {   // two players (as in the original): player 1 plays stage 1, player 2 the next stage (2), then player 1 stage 3 ... (own scores kept), no bonus stage after stage 10
        Game g2p; CHECK(g2p.init(adf, err, false));
        click(g2p, 240, 129);   // 2PLAYER
        click(g2p, 100, 120, 1); settle(g2p);   // right button
        CHECK(g2p.mode() == Game::Mode::Play && g2p.level() == 1 && g2p.player() == 1);
        play(g2p, dir + "/level1_solution.txt");
        CHECK(g2p.mode() == Game::Mode::Password && g2p.passwordShown() == "03797");
        int p1 = g2p.score(); CHECK(p1 > 0);
        for (int i = 0; i < 400; i++) g2p.step();
        Event en3; en3.type = Event::Key; en3.key = kKeyEnter; g2p.event(en3);
        for (int i = 0; i < 400 && g2p.mode() == Game::Mode::Password; i++) g2p.step();   // the next stage comes in with one of the 11 effects
        CHECK(g2p.mode() == Game::Mode::Play && g2p.level() == 2 && g2p.player() == 2 && g2p.score() == 0 && g2p.scoreOf(1) == p1);
        Game& g2r = g2p;   // player 2 is on stage 2 (solution found from the user's screenshot); the score of player 1 is kept
        CHECK(g2r.scoreOf(1) == p1);
        play(g2r, dir + "/level2_solution.txt");
        CHECK(g2r.mode() == Game::Mode::Password);
        const int p2 = g2r.score(); CHECK(p2 > 0 && g2r.scoreOf(1) == p1);
        for (int i = 0; i < 400; i++) g2r.step();
        g2r.event(en3);
        for (int i = 0; i < 400 && g2r.mode() == Game::Mode::Password; i++) g2r.step();   // the next stage comes in with one of the 11 effects
        // back to player 1 on stage 3: his score continues, the score of player 2 is kept
        CHECK(g2r.mode() == Game::Mode::Play && g2r.level() == 3 && g2r.player() == 1 && g2r.score() == p1 && g2r.scoreOf(2) == p2);
        {   // stage 10 as laid by the user (one piece a pixel off, ~1.1% uncovered): must be accepted
            Game gu; CHECK(gu.init(adf, err, false));
            click(gu, 240, 129); typeKeys(gu, "07274"); gu.event(en3); settle(gu);
            CHECK(gu.level() == 10);
            std::ifstream f(dir + "/level10_user.txt"); int t, r, x, y; std::vector<std::array<int,4>> v;
            while (f >> t >> r >> x >> y) v.push_back({t, r, x, y});
            v[4][2] -= 1;   // parallelogram one pixel to the left
            // the shifted layout goes to a temporary file (play() reads a file) that is removed again
            const std::string tmp = (std::filesystem::temp_directory_path() / "tangram_level10_user_shift.txt").string();
            std::ofstream o(tmp); for (auto& a : v) o << a[0] << ' ' << a[1] << ' ' << a[2] << ' ' << a[3] << '\n'; o.close();
            play(gu, tmp);
            std::remove(tmp.c_str());
            CHECK(gu.mode() == Game::Mode::Password);
        }
        Game g2q; CHECK(g2q.init(adf, err, false));   // stage 10 in two-player mode: no bonus stage
        click(g2q, 240, 129); typeKeys(g2q, "07274"); g2q.event(en3); settle(g2q);
        CHECK(g2q.level() == 10);
        play(g2q, dir + "/level10_solution.txt");
        for (int i = 0; i < 400; i++) g2q.step();
        g2q.event(en3);
        for (int i = 0; i < 400 && g2q.mode() == Game::Mode::Password; i++) g2q.step();   // the next stage comes in with one of the 11 effects
        CHECK(g2q.mode() == Game::Mode::Play && g2q.level() == 11 && g2q.player() == 2);
    }
    {   // name entry on the table, against the real game (table A with the last entry lowered to 20, table B to 0 so that a score qualifies)
        auto lowered = [&](const char* to) {
            Game gx; CHECK(gx.init(adf, err, false));
            Bytes d = gx.saveScores(); const std::string from = "    200";
            for (size_t i = 0; i + from.size() <= d.size(); i++) if (std::equal(from.begin(), from.end(), d.begin() + long(i))) std::copy(to, to + from.size(), d.begin() + long(i));
            return d;
        };
        struct Case { const char* file; bool expert; const char* last; long score; const char* typed; };
        const Case cases[] = {{"screen_name_a_empty.rgb", false, "     20", 65, ""}, {"screen_name_a_abcdefgh.rgb", false, "     20", 65, "ABCDEFGH"},
                              {"screen_name_a_chars.rgb", false, "     20", 65, "A -.@0Q"}, {"screen_name_b_empty.rgb", true, "      0", 0, ""}};
        for (const Case& c : cases) {
            Game gn; CHECK(gn.init(adf, err, false)); gn.loadScores(lowered(c.last));
            gn.showNameEntryForTest(c.expert, 2, c.score, c.typed);
            CHECK(gn.mode() == Game::Mode::NameEntry);
            gn.step();
            auto shot = slurp(dir + "/" + c.file); size_t same = 0;
            for (int i = 0; i < 320 * 200; i++) same += gn.frame().rgb[size_t(i)] == Rgb{uint8_t(shot[size_t(i) * 3]), uint8_t(shot[size_t(i) * 3 + 1]), uint8_t(shot[size_t(i) * 3 + 2])};
            std::printf("name entry %s equals the real game in %.3f%% of pixels\n", c.file, 100.0 * double(same) / 64000);
            CHECK(same == 64000);
        }
    }
    std::printf(failures ? "FAILED\n" : "OK\n");
    return failures ? 1 : 0;
}
