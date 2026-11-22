// test_levels <tests/data dir>: checks the level table and the polygon filler against the real game.
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

#include "../src/levels.hpp"
#include "../src/raster.hpp"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "tests/data";
    std::vector<tg::Level> lv;
    std::string err;
    CHECK(tg::loadLevelsFile(dir + "/levels.bin", lv, err));
    CHECK(lv.size() == 200);
    if (lv.size() != 200) return 1;

    // table sanity
    int minT = 999, maxT = 0;
    for (size_t i = 0; i < lv.size(); i++) {
        const auto& l = lv[i];
        CHECK(l.time >= 30 && l.time <= 100);
        CHECK(l.coords.size() % 2 == 0 && l.coords.size() >= 8 && l.coords.size() <= 52);
        for (int16_t v : l.coords) CHECK(v >= -100 && v <= 100);
        CHECK(l.time % 5 == 0);
        minT = std::min(minT, l.time); maxT = std::max(maxT, l.time);
    }
    CHECK(lv[0].time == 100 && lv[199].time == 30);
    CHECK(lv[0].coords.size() == 42);
    CHECK(lv[0].coords[0] == -90 && lv[0].coords[1] == 18);
    CHECK(lv[199].coords.size() == 16);  // the last figure is a square

    // level 1 against the red silhouette taken from the running game
    std::ifstream f(dir + "/level1_mask.raw", std::ios::binary);
    std::vector<uint8_t> ref((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    CHECK(ref.size() == 320 * 200);
    std::vector<tg::Pt> poly;
    for (size_t k = 0; k + 1 < lv[0].coords.size(); k += 2)
        poly.push_back({tg::kCx + lv[0].coords[k], tg::kCy - lv[0].coords[k + 1]});
    std::vector<uint8_t> mine(320 * 200, 0);
    tg::fillPolygon(poly, 320, 200, mine);
    long both = 0, either = 0, onlyMine = 0, onlyRef = 0;
    for (size_t i = 0; i < mine.size(); i++) {
        both += mine[i] && ref[i]; either += mine[i] || ref[i];
        onlyMine += mine[i] && !ref[i]; onlyRef += !mine[i] && ref[i];
    }
    double iou = either ? double(both) / either : 0;
    std::printf("level 1: %ld px shared, %ld only ours, %ld only game, IoU %.4f\n", both, onlyMine, onlyRef, iou);
    CHECK(iou > 0.99);

    // a square: 56x56 area
    std::vector<uint8_t> sq(320 * 200, 0);
    tg::fillPolygon({{10, 10}, {20, 10}, {20, 20}, {10, 20}}, 320, 200, sq);
    long a = 0; for (uint8_t v : sq) a += v;
    CHECK(a == 110);  // 10 rows of 11 pixels (spans include the right end)

    // parser errors
    std::vector<tg::Level> tmp;
    CHECK(!tg::parseLevels(tg::Bytes{'X', 'X', 'X', 'X', 0, 0}, tmp, err));
    CHECK(!tg::parseLevels(tg::Bytes{'T', 'G', 'L', 'V', 0, 5}, tmp, err));  // claims 5 levels, has none
    CHECK(!tg::parseLevels(tg::Bytes{'T', 'G', 'L', 'V', 0, 1, 0, 50, 0xFF, 0xFF}, tmp, err));  // absurd n

    std::printf(failures ? "%d FAILURES\n" : "all level tests passed\n", failures);
    return failures ? 1 : 0;
}
