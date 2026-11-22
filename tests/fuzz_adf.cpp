#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include "../src/adf.hpp"
int main(int argc, char** argv) {
    (void)argc;
    std::ifstream f(argv[1], std::ios::binary);
    tg::Bytes base((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    srand(12345);
    int ok = 0, rejected = 0;
    for (int it = 0; it < 3000; it++) {
        tg::Bytes d = base;
        int n = 1 + rand() % 200;
        for (int k = 0; k < n; k++) {
            // bias towards the metadata blocks (root + headers around the middle of the disk)
            size_t pos = (rand() % 3) ? 880 * 512 / 2 * 1 + rand() % (40 * 512) : size_t(rand()) % d.size();
            if (pos < d.size()) d[pos] = uint8_t(rand());
        }
        tg::Adf a; std::string err;
        if (a.load(d, err)) { ok++; for (auto& nm : a.names()) (void)a.file(nm); } else rejected++;
    }
    std::printf("fuzz: %d parsed, %d rejected, no crashes\n", ok, rejected);
}
