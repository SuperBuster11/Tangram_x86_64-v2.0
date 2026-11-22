// render_board <game.adf> <out.png> [palette 0..2] [scale]: the board picture stored in TANGRAM.MLU.
#include <cstdio>
#include <cstdlib>
#include "../src/assets.hpp"
int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: render_board game.adf out.png [palette] [scale]\n"); return 2; }
    tg::Adf adf; std::string err;
    if (!adf.loadFile(argv[1], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    const tg::Bytes* mlu = adf.file("TANGRAM.MLU");
    if (!mlu) { std::fprintf(stderr, "no TANGRAM.MLU\n"); return 1; }
    tg::Image img;
    if (!tg::mluBoardPicture(*mlu, argc > 3 ? std::atoi(argv[3]) : 0, img, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    if (!tg::writePng(argv[2], img, argc > 4 ? std::atoi(argv[4]) : 1)) { std::fprintf(stderr, "cannot write png\n"); return 1; }
    return 0;
}
