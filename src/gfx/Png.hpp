#pragma once
// Minimal PNG writer (RGB, stored deflate blocks: no compression library needed).
#include <cstdint>
#include <string>
#include <vector>

namespace liq {

// `rgb` holds w*h*3 bytes, row by row. Returns false if the file cannot be written.
bool writePng(const std::string& path, int w, int h, const std::vector<uint8_t>& rgb);

}  // namespace liq
