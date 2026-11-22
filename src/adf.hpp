// Amiga disk image (ADF, OFS/FFS) reader.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tg {

using Bytes = std::vector<uint8_t>;

class Adf {
public:
    // Parses an 880 KB image. Returns false and sets err on failure.
    bool load(const Bytes& image, std::string& err);
    bool loadFile(const std::string& path, std::string& err);

    // Case-insensitive lookup ("TANGRAM.DAT"); returns nullptr when missing.
    const Bytes* file(const std::string& name) const;
    std::vector<std::string> names() const;
    // Adds (or replaces) a file in memory; lets the game run from data that is not read from a disk image.
    void addFile(const std::string& name, Bytes data);
    const std::string& label() const { return label_; }

private:
    std::string label_;
    std::map<std::string, Bytes> files_;  // lower-case path -> data
};

}  // namespace tg
