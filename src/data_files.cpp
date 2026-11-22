#include "data_files.hpp"

#include <fstream>
#include <iterator>

namespace tg {

const char* const kDataFiles[8] = {"TANGRAM.DAT", "TANGRAM.MLU", "TITLEPIC.LO5", "THALION.LO5", "MUSIC1.IMG", "MUSIC2.IMG", "HIGHSCOREPIC.LO5", "SCORE.DTA"};

bool loadDataFolder(Adf& adf, const std::string& dir, std::string& err) {
    std::string missing;
    for (const char* name : kDataFiles) {
        std::ifstream in(dir + name, std::ios::binary);
        Bytes b;
        if (in) b.assign(std::istreambuf_iterator<char>(in), {});
        if (b.empty()) { missing += missing.empty() ? "" : ", "; missing += name; continue; }
        adf.addFile(name, std::move(b));
    }
    if (missing.empty()) return true;
    err = "Missing game data in " + (dir.empty() ? std::string("data/") : dir) + ": " + missing +
          ".\nKeep the \"data\" folder next to the program (or start it with a Tangram disk image: tangram disk.adf).";
    return false;
}

std::string findDataFolder(const std::string& exeDir) {
    const std::string next = exeDir + "data/";
    if (std::ifstream(next + kDataFiles[0], std::ios::binary)) return next;
    return "data/";
}

}  // namespace tg
