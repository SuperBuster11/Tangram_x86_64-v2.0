#include "adf.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>

namespace tg {
namespace {

constexpr size_t kBlock = 512;

uint32_t be32(const uint8_t* p) {
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

struct Reader {
    const Bytes& d;
    bool ffs;
    std::map<std::string, Bytes>& out;
    std::vector<bool> seen;  // blocks already visited (guards against cyclic chains in damaged images)

    bool visit(uint32_t n) {
        if (seen.empty()) seen.assign(d.size() / kBlock, false);
        if (n >= seen.size() || seen[n]) return false;
        seen[n] = true;
        return true;
    }

    const uint8_t* blk(uint32_t n) const {
        return (size_t(n) + 1) * kBlock <= d.size() ? d.data() + size_t(n) * kBlock : nullptr;
    }
    static std::string name(const uint8_t* b) {
        size_t l = std::min<size_t>(b[432], 30);
        return std::string(reinterpret_cast<const char*>(b + 433), l);
    }

    void readFile(const uint8_t* header, const std::string& path) {
        size_t size = be32(header + 324);
        Bytes data;
        data.reserve(size);
        const uint8_t* cur = header;
        size_t extBlocks = 0;
        while (cur) {
            if (++extBlocks > 4096) break;
            uint32_t cnt = be32(cur + 8);
            for (uint32_t k = 0; k < cnt && k < 72; k++) {
                const uint8_t* pb = blk(be32(cur + 308 - 4 * k));
                if (!pb) continue;
                if (ffs) {
                    data.insert(data.end(), pb, pb + kBlock);
                } else {
                    size_t ds = std::min<size_t>(be32(pb + 12), kBlock - 24);
                    data.insert(data.end(), pb + 24, pb + 24 + ds);
                }
            }
            uint32_t ext = be32(cur + kBlock - 8);
            cur = ext ? blk(ext) : nullptr;
        }
        if (data.size() > size) data.resize(size);
        out[lower(path)] = std::move(data);
    }

    void walk(uint32_t n, const std::string& prefix, int depth) {
        if (depth > 16) return;
        const uint8_t* b = blk(n);
        if (!b) return;
        for (int i = 0; i < 72; i++) {
            uint32_t c = be32(b + 24 + 4 * i);
            while (c) {
                const uint8_t* e = blk(c);
                if (!e || !visit(c)) break;
                std::string path = prefix.empty() ? name(e) : prefix + "/" + name(e);
                int32_t type = int32_t(be32(e + 508));
                if (type == 2) walk(c, path, depth + 1);
                else if (type == -3) readFile(e, path);
                c = be32(e + 496);
            }
        }
    }
};

}  // namespace

bool Adf::load(const Bytes& d, std::string& err) {
    if (d.size() != 901120 && d.size() != 1802240) {
        err = "unexpected ADF size " + std::to_string(d.size());
        return false;
    }
    if (d[0] != 'D' || d[1] != 'O' || d[2] != 'S') {
        err = "not an AmigaDOS disk (no DOS boot block)";
        return false;
    }
    Reader r{d, (d[3] & 1) != 0, files_, {}};
    uint32_t root = uint32_t(d.size() / kBlock / 2);
    const uint8_t* rb = r.blk(root);
    if (!rb || be32(rb) != 2) {
        err = "root block not found";
        return false;
    }
    label_ = Reader::name(rb);
    r.walk(root, "", 0);
    return true;
}

bool Adf::loadFile(const std::string& path, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        err = "cannot open " + path;
        return false;
    }
    Bytes d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return load(d, err);
}

const Bytes* Adf::file(const std::string& name) const {
    auto it = files_.find(lower(name));
    return it == files_.end() ? nullptr : &it->second;
}

std::vector<std::string> Adf::names() const {
    std::vector<std::string> v;
    for (auto& kv : files_) v.push_back(kv.first);
    return v;
}

void Adf::addFile(const std::string& name, Bytes data) {
    std::string k = name;
    for (char& ch : k) ch = char(std::tolower(static_cast<unsigned char>(ch)));
    files_[k] = std::move(data);
}

}  // namespace tg
