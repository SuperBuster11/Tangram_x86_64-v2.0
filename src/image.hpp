// Planar Amiga pictures ("lo5": 320x200, 5 bitplanes stored plane after plane) and PNG output.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "adf.hpp"

namespace tg {

struct Rgb {
    uint8_t r, g, b;
    bool operator==(const Rgb& o) const { return r == o.r && g == o.g && b == o.b; }
};

struct Palette {
    Rgb c[32];
};

struct Image {
    int w = 0, h = 0;
    std::vector<uint8_t> index;  // palette index per pixel
    std::vector<Rgb> rgb;        // resolved colours
};

// 12-bit Amiga colour word (0x0RGB) -> 8-bit RGB.
Rgb amigaColor(uint16_t word);

// Reads 32 big-endian colour words.
Palette readPalette(const uint8_t* p);

// Decodes a 320x200, 5-plane picture (plane-sequential, 8000 bytes per plane).
bool decodeLo5(const Bytes& data, const Palette& pal, Image& out, std::string& err);

// Writes an uncompressed (stored-deflate) PNG.
bool writePng(const std::string& path, const Image& img, int scale = 1);

}  // namespace tg
