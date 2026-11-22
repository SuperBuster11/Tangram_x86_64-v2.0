// render_music <game.adf> <1|2> <seconds> <out.wav> [log.txt]: runs the original music player and writes a WAV (and optionally the
// sequence of Paula / CIA register writes, for comparison with the reference port).
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include "../src/adf.hpp"
#include "../src/music.hpp"
static void le32(std::ofstream& f, uint32_t v) { for (int i = 0; i < 4; i++) f.put(char(v >> (8 * i))); }
static void le16(std::ofstream& f, uint16_t v) { f.put(char(v)); f.put(char(v >> 8)); }
int main(int argc, char** argv) {
    if (argc < 5) { std::fprintf(stderr, "usage: render_music game.adf 1|2 seconds out.wav [log.txt]\n"); return 2; }
    tg::Adf adf; std::string err;
    if (!adf.loadFile(argv[1], err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    const tg::Bytes* img = adf.file(argv[2][0] == '2' ? "MUSIC2.IMG" : "MUSIC1.IMG");
    if (!img) { std::fprintf(stderr, "music image missing\n"); return 1; }
    tg::Music m;
    if (argc > 5) m.enableLog(true);
    if (!m.load(*img, err) || !m.start(err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    int frames = int(std::atof(argv[3]) * tg::Music::kRate);
    std::vector<int16_t> pcm(size_t(frames) * 2);
    m.render(pcm.data(), frames);
    if (!m.error().empty()) std::fprintf(stderr, "player error: %s\n", m.error().c_str());
    std::ofstream f(argv[4], std::ios::binary);
    f.write("RIFF", 4); le32(f, 36 + uint32_t(pcm.size() * 2)); f.write("WAVEfmt ", 8); le32(f, 16); le16(f, 1); le16(f, 2);
    le32(f, tg::Music::kRate); le32(f, tg::Music::kRate * 4); le16(f, 4); le16(f, 16); f.write("data", 4); le32(f, uint32_t(pcm.size() * 2));
    f.write(reinterpret_cast<const char*>(pcm.data()), std::streamsize(pcm.size() * 2));
    if (argc > 5) { std::ofstream l(argv[5]); l << m.takeLog(); }
    return 0;
}
