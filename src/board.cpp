#include "board.hpp"

namespace tg {

Coverage measureCoverage(const Level& level, const std::vector<Placed>& pieces) {
    std::vector<Pt> fig;
    for (size_t k = 0; k + 1 < level.coords.size(); k += 2)
        fig.push_back({kCx + level.coords[k], kCy - level.coords[k + 1]});
    std::vector<uint8_t> figMask(size_t(kBoardW) * kBoardH, 0);
    fillPolygon(fig, kBoardW, kBoardH, figMask);

    std::vector<int> count(size_t(kBoardW) * kBoardH, 0);
    for (const Placed& p : pieces) {
        std::vector<uint8_t> m(size_t(kBoardW) * kBoardH, 0);
        fillPolygon(placedOutline(p.type, p.rotation, p.x, p.y), kBoardW, kBoardH, m);
        for (size_t i = 0; i < m.size(); i++) count[i] += m[i];
    }
    Coverage c;
    for (size_t i = 0; i < figMask.size(); i++) {
        if (figMask[i]) {
            c.figurePixels++;
            if (!count[i]) c.uncovered++;
        } else if (count[i]) {
            c.outside++;
        }
        if (count[i] > 1) c.overlap++;
    }
    return c;
}

std::vector<uint8_t> uncoveredMask(const Level& level, const std::vector<Placed>& pieces) {
    std::vector<Pt> fig;
    for (size_t k = 0; k + 3 < level.coords.size(); k += 2)   // as drawn (the last pair repeats the first)
        fig.push_back({kCx + level.coords[k], kCy - level.coords[k + 1]});
    std::vector<uint8_t> vis(size_t(kBoardW) * kBoardH, 0), cov(vis.size(), 0);
    fillPolygonDrawn(fig, kBoardW, kBoardH, vis);
    for (const Placed& p : pieces) fillPolygonDrawn(placedOutline(p.type, p.rotation, p.x, p.y), kBoardW, kBoardH, cov);
    for (size_t i = 0; i < vis.size(); i++) vis[i] = vis[i] && !cov[i];
    return vis;
}

bool originalSolveTest(const std::vector<uint8_t>& visible, int w) {
    // row buffer of the original: 40 bytes (10 longs) per row, 7 longs used; two zero rows in front
    const int kRows = 200, kLongs = 10;
    std::vector<uint32_t> buf(size_t(kRows + 2) * kLongs, 0);
    auto at = [&](int row, int col) -> uint32_t& { return buf[size_t(row + 2) * kLongs + size_t(col)]; };
    auto add = [](uint32_t& sum, uint32_t& carry, uint32_t v) {   // 0x10f3d8: bit-sliced counter, returns the overflow bits
        uint32_t c0 = sum & v; sum ^= v; uint32_t over = c0 & carry; carry ^= c0; return over;
    };
    int budget = 75;
    for (int row = 0; row < kRows; row++) {
        for (int col = 0; col < 7; col++) {
            uint32_t m = 0;
            for (int b = 0; b < 32; b++) {
                int x = col * 32 + b;
                if (x < w && visible[size_t(row) * w + x]) m |= 0x80000000u >> b;
            }
            at(row, col) = m;
            uint32_t d5 = at(row - 1, col);   // the row above is examined (one row behind)
            if (!d5) continue;
            // counting loop of the original: lsr.l #1,d4 / subx.w d6,d7 / beq -> the Z flag of SUBX is only cleared, never set,
            // so the test fires only if the counter reaches 0 just as the last visible pixel of the word is shifted out
            // (bits go from the right end of the word to the left); otherwise the counter wraps and never stops the stage
            for (int b = 0; b < 32; b++) {
                const uint32_t bit = (d5 >> b) & 1u, rest = b == 31 ? 0u : d5 >> (b + 1);
                budget = (budget - int(bit)) & 0xffff;
                if (budget == 0 && rest == 0) return false;
            }
            uint32_t carry = 0;
            // right neighbour: next long's top bit shifted in
            add(d5, carry, (at(row - 1, col) << 1) | (at(row - 1, col + 1) >> 31));
            // left neighbour: the low word of the previous long is shifted right in place, its lost bit comes in on top
            uint32_t left = 0;
            if (col > 0) {
                uint32_t& prev = at(row - 1, col - 1);
                uint32_t lo = prev & 0xffffu;
                left = (lo & 1u) << 31;
                prev = (prev & 0xffff0000u) | (lo >> 1);
            } else {   // the word before column 0 is the unused end of the row before (always 0)
                left = 0;
            }
            add(d5, carry, (at(row - 1, col) >> 1) | left);
            if (add(d5, carry, at(row - 2, col))) return false;   // above
            if (add(d5, carry, at(row, col))) return false;       // below
        }
    }
    return true;
}

bool solved(const Level& level, const std::vector<Placed>& pieces) {
    return originalSolveTest(uncoveredMask(level, pieces), kBoardW);
}

}  // namespace tg
