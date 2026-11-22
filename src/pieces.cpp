#include "pieces.hpp"

#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace tg {
namespace {

// Offsets from the cursor at rotation 0 (taken from the running game; y grows downwards).
const Pt kShape[kPieceTypes][5] = {
    {{-40, -20}, {40, -20}, {0, 20}, {0, 0}, {0, 0}},          // large right triangle, hypotenuse 80
    {{0, -20}, {20, 0}, {0, 20}, {-20, 0}, {0, 0}},            // square (diagonal 40)
    {{10, 10}, {-30, 10}, {10, -30}, {0, 0}, {0, 0}},          // medium right triangle, legs 40
    {{20, 10}, {-20, 10}, {0, -10}, {0, 0}, {0, 0}},           // small right triangle, hypotenuse 40
    {{10, -30}, {10, 10}, {-10, 30}, {-10, -10}, {0, 0}},      // parallelogram
};
const int kVerts[kPieceTypes] = {3, 4, 3, 3, 4};

}  // namespace

int pieceCount(PieceType t) {
    switch (t) {
        case kLargeTriangle: return 2;
        case kSmallTriangle: return 2;
        default: return 1;
    }
}

std::vector<Pt> pieceOutline(PieceType t, int rotation) {
    const double th = -rotation * (M_PI / 8.0);  // 22.5 degrees; negative in y-down screen coordinates
    const double c = std::cos(th), s = std::sin(th);
    std::vector<Pt> out;
    for (int i = 0; i < kVerts[t]; i++) {
        double x = kShape[t][i].x * c - kShape[t][i].y * s;
        double y = kShape[t][i].x * s + kShape[t][i].y * c;
        out.push_back({int(std::lround(x)), int(std::lround(y))});
    }
    return out;
}

std::vector<Pt> placedOutline(PieceType t, int rotation, int cx, int cy) {
    std::vector<Pt> p = pieceOutline(t, ((rotation % kRotations) + kRotations) % kRotations);
    for (auto& q : p) { q.x += cx; q.y += cy; }
    return p;
}

}  // namespace tg
