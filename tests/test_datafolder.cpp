// test_datafolder <data dir/>: the game must start and render from the files of the data folder alone (v2.0, nothing compiled in);
// a missing file is reported by name. If TANGRAM_ADF is set, the files must be byte-identical to the originals on the disk.
#include <cstdio>
#include <cstdlib>
#include "../src/data_files.hpp"
#include "../src/game.hpp"
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
using namespace tg;
int main(int argc, char** argv) {
    const std::string dir = argc >= 2 ? argv[1] : "data/";
    Adf d; std::string err;
    CHECK(loadDataFolder(d, dir, err));
    for (const char* n : kDataFiles) CHECK(d.file(n) && !d.file(n)->empty());
    if (const char* p = std::getenv("TANGRAM_ADF")) if (*p) {
        Adf adf; CHECK(adf.loadFile(p, err));
        for (const char* n : kDataFiles) CHECK(d.file(n) && adf.file(n) && *d.file(n) == *adf.file(n));
    }
    { Adf none; std::string e2; CHECK(!loadDataFolder(none, "/nonexistent/", e2) && e2.find("TANGRAM.DAT") != std::string::npos); }
    CHECK(findDataFolder("/nonexistent/") == "data/");
    Game g;
    CHECK(g.init(d, err, false));
    CHECK(g.mode() == Game::Mode::Menu);
    Event k; k.type = Event::Key; k.key = kKeyEnter; g.event(k);
    for (int i = 0; i < 30; i++) g.step();   // the menu fades out first
    CHECK(g.mode() == Game::Mode::Play && g.level() == 1);
    for (int i = 0; i < 10; i++) g.step();
    int16_t buf[2 * 1000]; g.audio(buf, 1000); CHECK(g.song() == 1);
    std::printf(failures ? "FAILED\n" : "OK\n");
    return failures ? 1 : 0;
}
