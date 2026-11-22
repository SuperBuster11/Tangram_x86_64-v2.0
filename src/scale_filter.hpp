// Enlarging the 320x200 frame for the window layers (F8 cycles the modes). The game itself always renders 320x200.
#pragma once
#include <cstdint>

#include "image.hpp"

namespace tg {

enum class ScaleFilter { Off = 0, Smooth = 1, Crt = 2, Monitor1084 = 3 };
// nearest neighbour / bilinear / CRT scanlines / Commodore 1084S monitor (soft beam, aperture grille)

// Writes outW x outH pixels as 0x00RRGGBB into dst. Any size works (fullscreen keeps 16:10 at the monitor's size, so the factor
// is not always whole); the window sizes are whole multiples of 320x200.
void scaleFrame(const Image& src, uint32_t* dst, int outW, int outH, ScaleFilter mode);
// (src.w*scale) x (src.h*scale), scale >= 1.
inline void scaleFrame(const Image& src, uint32_t* dst, int scale, ScaleFilter mode) {
    if (scale < 1) scale = 1;
    scaleFrame(src, dst, src.w * scale, src.h * scale, mode);
}

// F8 cycle: 1084S (the default) -> CRT -> smoothed -> off -> 1084S
inline ScaleFilter nextFilter(ScaleFilter f) {
    switch (f) {
    case ScaleFilter::Monitor1084: return ScaleFilter::Crt;
    case ScaleFilter::Crt: return ScaleFilter::Smooth;
    case ScaleFilter::Smooth: return ScaleFilter::Off;
    default: return ScaleFilter::Monitor1084;
    }
}

}  // namespace tg
