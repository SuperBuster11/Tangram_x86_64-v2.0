// dump_adf <disk.adf> <outdir>: lists the files of the disk and writes them out.
#include <cstdio>
#include <fstream>
#include <string>

#include "../src/adf.hpp"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: dump_adf disk.adf outdir\n");
        return 2;
    }
    tg::Adf adf;
    std::string err;
    if (!adf.loadFile(argv[1], err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }
    std::printf("label: %s\n", adf.label().c_str());
    for (auto& n : adf.names()) {
        const tg::Bytes* d = adf.file(n);
        std::printf("%-24s %8zu bytes\n", n.c_str(), d->size());
        std::string path = std::string(argv[2]) + "/" + n;
        std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(d->data()), d->size());
    }
}
