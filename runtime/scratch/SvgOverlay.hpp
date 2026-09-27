// SvgOverlay.hpp - draw SVG <text> and embedded <image> that nanosvg ignores.
#pragma once

#include <string>

namespace scratch {

void overlaySvgExtras(const std::string& svg, unsigned char* pixels, int width, int height,
                      double rasterScale, double viewMinX, double viewMinY);

}  // namespace scratch
