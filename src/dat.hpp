// TANGRAM.DAT: an archive of small planar pictures (menu buttons, fonts, panels, cursors, icons, time bar).
// Layout: text line with a count ("84"), then for every picture a text line with its byte length followed by that many
// bytes: u16 depth, u16 width-1, u16 height-1, then `depth` bitplanes, each height rows of ((width+15)/16)*2 bytes.
// The count line is one less than the number of pictures (85 are stored).
#pragma once
#include <string>
#include <vector>

#include "adf.hpp"
#include "image.hpp"

namespace tg {

struct DatPicture {
    int depth = 0, w = 0, h = 0;
    std::vector<uint8_t> index;   // w*h palette indices
    Image toImage(const Palette& pal) const;
};

bool parseDat(const Bytes& data, std::vector<DatPicture>& out, std::string& err);

}  // namespace tg
