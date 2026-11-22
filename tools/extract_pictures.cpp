// extract_pictures <disk.adf> <outdir>: writes the three title pictures as PNG files.
#include <cstdio>
#include <string>

#include "../src/adf.hpp"
#include "../src/image.hpp"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: extract_pictures disk.adf outdir\n");
        return 2;
    }
    tg::Adf adf;
    std::string err;
    if (!adf.loadFile(argv[1], err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }
    const tg::Bytes* mlu = adf.file("tangram.mlu");
    if (!mlu || mlu->size() < 64) {
        std::fprintf(stderr, "tangram.mlu missing\n");
        return 1;
    }
    tg::Palette title = tg::readPalette(mlu->data());  // first 32 words: title palette
    for (const char* name : {"titlepic.lo5", "highscorepic.lo5"}) {
        const tg::Bytes* d = adf.file(name);
        tg::Image img;
        if (!d || !tg::decodeLo5(*d, title, img, err)) {
            std::fprintf(stderr, "%s: %s\n", name, err.c_str());
            continue;
        }
        std::string out = std::string(argv[2]) + "/" + name + ".png";
        tg::writePng(out, img, 1);
        std::printf("wrote %s\n", out.c_str());
    }
    // Thalion logo carries its own palette in the last 64 bytes
    if (const tg::Bytes* d = adf.file("thalion.lo5")) {
        tg::Palette pal = tg::readPalette(d->data() + 40000);
        tg::Image img;
        if (tg::decodeLo5(*d, pal, img, err)) {
            std::string out = std::string(argv[2]) + "/thalion.lo5.png";
            tg::writePng(out, img, 1);
            std::printf("wrote %s\n", out.c_str());
        }
    }
}
