// test_pieces <tests/data>: pieces against vertices captured from the running game, and the board model.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "../src/board.hpp"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "tests/data";
    std::ifstream f(dir + "/pieces_capture.txt");
    CHECK(bool(f));
    std::string line;
    int checked = 0, wrong = 0;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream is(line);
        int type, rot;
        is >> type >> rot;
        std::vector<tg::Pt> want;
        int x, y;
        while (is >> x >> y) want.push_back({x, y});
        auto got = tg::placedOutline(tg::PieceType(type), rot, 120, 130);
        CHECK(got.size() == want.size());
        for (size_t i = 0; i < want.size() && i < got.size(); i++) {
            checked++;
            if (got[i].x != want[i].x || got[i].y != want[i].y) wrong++;
        }
    }
    std::printf("piece vertices checked: %d, wrong: %d\n", checked, wrong);
    CHECK(checked == 272);  // 5 types x 16 rotations: 3 types with 3 vertices... = 272 vertices in total
    CHECK(wrong == 0);

    // 16 steps are a full turn
    for (int t = 0; t < tg::kPieceTypes; t++) {
        auto a = tg::pieceOutline(tg::PieceType(t), 0), b = tg::pieceOutline(tg::PieceType(t), 16 % tg::kRotations);
        CHECK(a.size() == b.size());
        for (size_t i = 0; i < a.size(); i++) CHECK(a[i].x == b[i].x && a[i].y == b[i].y);
    }
    int total = 0;
    for (int t = 0; t < tg::kPieceTypes; t++) total += tg::pieceCount(tg::PieceType(t));
    CHECK(total == 7);

    // board model: a figure that is exactly one large triangle
    tg::Level lv;
    lv.time = 100;
    auto tri = tg::pieceOutline(tg::kLargeTriangle, 0);  // at the cursor (0,0)
    for (auto& p : tri) { lv.coords.push_back(int16_t(p.x + 20 - tg::kCx)); lv.coords.push_back(int16_t(tg::kCy - (p.y + 30))); }
    lv.coords.push_back(lv.coords[0]); lv.coords.push_back(lv.coords[1]);   // closed like the real figures (last pair = first)
    tg::Coverage c = tg::measureCoverage(lv, {{tg::kLargeTriangle, 0, 20, 30}});
    CHECK(c.figurePixels > 1000);
    CHECK(tg::solved(lv, {{tg::kLargeTriangle, 0, 20, 30}}));
    tg::Coverage off = tg::measureCoverage(lv, {{tg::kLargeTriangle, 0, 25, 30}});
    CHECK(!tg::solved(lv, {{tg::kLargeTriangle, 0, 25, 30}}));
    tg::Coverage rot = tg::measureCoverage(lv, {{tg::kLargeTriangle, 4, 20, 30}});
    CHECK(rot.uncovered > 100);
    tg::Coverage two = tg::measureCoverage(lv, {{tg::kLargeTriangle, 0, 20, 30}, {tg::kLargeTriangle, 0, 20, 30}});
    CHECK(two.overlap == two.figurePixels);

    // the solve test of the original (0x10d3c6) on 150 screens of the simulator running the original code: must agree on every one
    {
        std::ifstream f(dir + "/solve_test_screens.txt"); std::string l; int total = 0, agree = 0;
        while (std::getline(f, l)) {
            if (l.empty() || l[0] == '#') continue;
            std::istringstream is(l); int ok; is >> ok;
            std::vector<uint8_t> vis(size_t(tg::kBoardW) * tg::kBoardH, 0); std::string xy;
            while (is >> xy) { int x = 0, y = 0; std::sscanf(xy.c_str(), "%d,%d", &x, &y); vis[size_t(y) * tg::kBoardW + size_t(x)] = 1; }
            total++; agree += tg::originalSolveTest(vis, tg::kBoardW) == (ok != 0);
        }
        std::printf("solve test vs the original: %d/%d\n", agree, total);
        CHECK(total == 150 && agree == total);
    }
    // the solutions used in the tests and the user's stage-10 layout are accepted; a thin sliver passes, a 2-pixel-thick strip does not
    {
        std::vector<tg::Level> lv; std::string err;
        CHECK(tg::loadLevelsFile(dir + "/levels.bin", lv, err));
        auto load = [&](const std::string& n) { std::vector<tg::Placed> v; std::ifstream sf(dir + "/" + n); int ty, ro, px, py;
                                                while (sf >> ty >> ro >> px >> py) v.push_back({tg::PieceType(ty), ro, px, py}); return v; };
        CHECK(tg::solved(lv[0], load("level1_solution.txt")));
        CHECK(tg::solved(lv[1], load("level2_solution.txt")));
        CHECK(tg::solved(lv[9], load("level10_solution.txt")));
        auto u = load("level10_user.txt");
        CHECK(tg::solved(lv[9], u));
        u[4].x -= 1; CHECK(tg::solved(lv[9], u));   // the parallelogram one pixel off (the user's case)
        std::vector<uint8_t> vis(size_t(tg::kBoardW) * tg::kBoardH, 0);
        for (int x = 50; x < 110; x++) vis[size_t(60) * tg::kBoardW + size_t(x)] = 1;   // 60 pixels in one row
        CHECK(tg::originalSolveTest(vis, tg::kBoardW));
        for (int x = 50; x < 54; x++) vis[size_t(61) * tg::kBoardW + size_t(x)] = 1;    // a 2x4 block in it
        CHECK(!tg::originalSolveTest(vis, tg::kBoardW));
    }
    std::printf(failures ? "%d FAILURES\n" : "all piece tests passed\n", failures);
    return failures ? 1 : 0;
}
