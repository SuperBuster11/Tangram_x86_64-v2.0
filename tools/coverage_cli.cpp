// coverage_cli <levels.bin> <index>: reads "type rot x y" lines from stdin, prints figure/uncovered/outside/overlap.
#include <cstdio>
#include <cstdlib>
#include "../src/board.hpp"
int main(int argc, char** argv) {
    std::vector<tg::Level> lv; std::string err;
    if (argc < 3 || !tg::loadLevelsFile(argv[1], lv, err)) return 2;
    std::vector<tg::Placed> p; int t, r, x, y;
    while (scanf("%d %d %d %d", &t, &r, &x, &y) == 4) p.push_back({tg::PieceType(t), r, x, y});
    tg::Coverage c = tg::measureCoverage(lv.at(atoi(argv[2])), p);
    printf("%d %d %d %d\n", c.figurePixels, c.uncovered, c.outside, c.overlap);
}
