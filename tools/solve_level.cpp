// solve_level <levels.bin> <index> [tol]: brute-force solution of a figure with the 7 pieces (test aid).
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <set>
#include <cmath>
#include "../src/board.hpp"
using namespace tg;
// Exact rasteriser: a pixel belongs to the polygon when its centre is inside (even-odd), so abutting pieces
// neither overlap nor leave gaps.
static void fillCentres(const std::vector<tg::Pt>& poly, std::vector<uint8_t>& out) {
    int n = int(poly.size());
    for (int y = 0; y < tg::kBoardH; y++) {
        double yc = y + 0.5; std::vector<double> xs;
        for (int i = 0; i < n; i++) {
            tg::Pt a = poly[i], b = poly[(i + 1) % n];
            if ((a.y <= yc) != (b.y <= yc)) xs.push_back(a.x + (yc - a.y) * double(b.x - a.x) / (b.y - a.y));
        }
        std::sort(xs.begin(), xs.end());
        for (size_t k = 0; k + 1 < xs.size(); k += 2)
            for (int x = int(std::ceil(xs[k] - 0.5)); x <= int(std::ceil(xs[k + 1] - 0.5)) - 1; x++)
                if (x >= 0 && x < tg::kBoardW) out[y * tg::kBoardW + x] = 1;
    }
}
namespace tg { static bool operator<(const Pt& a, const Pt& b) { return a.y != b.y ? a.y < b.y : a.x < b.x; } }
struct Cand { int type, rot, cx, cy; std::vector<int> px; };
static std::vector<Cand> cands;
static std::vector<std::vector<int>> bypix;
static std::vector<uint8_t> fig, cov;
static int cnt[5] = {2, 1, 1, 2, 1}, tol = 8, figArea = 0, coveredN = 0;
static int gapBudget = 62, waived = 0;
static long nodes = 0; static size_t bestDepth = 0; static std::vector<int> bestSol;
static std::vector<int> sol;
static bool dfs() {
    if (++nodes > 200000000L) return false;
    int first = -1;
    for (int i = 0; i < kBoardW * kBoardH; i++) if (fig[i] && !cov[i]) { first = i; break; }
    int left = 0; for (int c : cnt) left += c;
    if (sol.size() > bestDepth) { bestDepth = sol.size(); bestSol = sol; }
    if (left == 0) return figArea - coveredN <= gapBudget;
    if (first < 0) return false;
    for (int k : bypix[first]) {
        const Cand& c = cands[k];
        if (!cnt[c.type]) continue;
        int ov = 0; for (int i : c.px) ov += cov[i];
        if (ov > tol) continue;
        std::vector<int> added;
        for (int i : c.px) if (!cov[i]) { cov[i] = 1; added.push_back(i); }
        coveredN += added.size(); cnt[c.type]--; sol.push_back(k);
        if (dfs()) return true;
        sol.pop_back(); cnt[c.type]++; coveredN -= added.size();
        for (int i : added) cov[i] = 0;
    }
    if (waived < gapBudget) { cov[first] = 1; waived++; coveredN++; bool r = dfs(); coveredN--; waived--; cov[first] = 0; if (r) return true; }
    return false;
}
int main(int argc, char** argv) {
    std::vector<Level> lv; std::string err; if (!loadLevelsFile(argv[1], lv, err)) { fprintf(stderr, "%s\n", err.c_str()); return 2; }
    const Level& L = lv.at(atoi(argv[2]));
    if (argc > 3) tol = atoi(argv[3]);
    if (argc > 4) gapBudget = atoi(argv[4]);
    std::vector<Pt> poly;
    for (size_t k = 0; k + 3 < L.coords.size(); k += 2) poly.push_back({kCx + L.coords[k], kCy - L.coords[k + 1]});
    fig.assign(kBoardW * kBoardH, 0); fillCentres(poly, fig);
    for (auto v : fig) figArea += v;
    for (int t = 0; t < 5; t++) {
        std::set<std::vector<Pt>> seen;
        for (int r = 0; r < 16; r++) {
            auto o = pieceOutline(PieceType(t), r);
            std::vector<uint8_t> m(kBoardW * kBoardH, 0);
            std::vector<Pt> p0; for (auto q : o) p0.push_back({q.x + 160, q.y + 100});
            fillCentres(p0, m);
            std::vector<Pt> rel;
            for (int y = 0; y < kBoardH; y++) for (int x = 0; x < kBoardW; x++) if (m[y * kBoardW + x]) rel.push_back({x - 160, y - 100});
            if (!seen.insert(rel).second) continue;
            for (int cy = 0; cy < kBoardH; cy++) for (int cx = 0; cx < kBoardW; cx++) {
                int out = 0; bool ok = true;
                for (auto q : rel) {
                    int x = cx + q.x, y = cy + q.y;
                    if (x < 0 || y < 0 || x >= kBoardW || y >= kBoardH || !fig[y * kBoardW + x]) { if (++out > tol) { ok = false; break; } }
                }
                if (!ok) continue;
                Cand c{t, r, cx, cy, {}};
                for (auto q : rel) { int x = cx + q.x, y = cy + q.y; if (x >= 0 && y >= 0 && x < kBoardW && y < kBoardH && fig[y * kBoardW + x]) c.px.push_back(y * kBoardW + x); }
                cands.push_back(std::move(c));
            }
        }
    }
    bypix.assign(kBoardW * kBoardH, {});
    for (size_t k = 0; k < cands.size(); k++) for (int i : cands[k].px) bypix[i].push_back(int(k));
    fprintf(stderr, "%zu candidates, area %d\n", cands.size(), figArea);
    cov.assign(kBoardW * kBoardH, 0);
    if (dfs()) { for (int k : sol) printf("%d %d %d %d\n", cands[k].type, cands[k].rot, cands[k].cx, cands[k].cy); return 0; }
    printf("none (nodes %ld) best depth %zu\n", nodes, bestDepth); for (int k : bestSol) printf("%d %d %d %d\n", cands[k].type, cands[k].rot, cands[k].cx, cands[k].cy); return 1;
}
