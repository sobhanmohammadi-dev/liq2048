#include "gfx/Png.hpp"

#include <algorithm>
#include <cstdio>

namespace liq {

namespace {

uint32_t crcUpdate(uint32_t crc, const uint8_t* p, size_t n) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = true;
    }
    for (size_t i = 0; i < n; ++i) crc = table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
    return crc;
}

void be32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(uint8_t(x >> 24));
    v.push_back(uint8_t(x >> 16));
    v.push_back(uint8_t(x >> 8));
    v.push_back(uint8_t(x));
}

void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    be32(out, uint32_t(data.size()));
    std::vector<uint8_t> body(type, type + 4);
    body.insert(body.end(), data.begin(), data.end());
    out.insert(out.end(), body.begin(), body.end());
    be32(out, crcUpdate(0xFFFFFFFFu, body.data(), body.size()) ^ 0xFFFFFFFFu);
}

}  // namespace

bool writePng(const std::string& path, int w, int h, const std::vector<uint8_t>& rgb) {
    if (w <= 0 || h <= 0 || rgb.size() != size_t(w) * size_t(h) * 3) return false;
    // Raw scanlines: filter byte 0 + RGB.
    std::vector<uint8_t> raw;
    raw.reserve(size_t(h) * (size_t(w) * 3 + 1));
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), rgb.begin() + long(y) * w * 3, rgb.begin() + long(y + 1) * w * 3);
    }
    // zlib stream made of stored blocks (max 65535 bytes each) + Adler-32.
    std::vector<uint8_t> z = {0x78, 0x01};
    uint32_t a = 1, b = 0;
    for (uint8_t x : raw) {
        a = (a + x) % 65521u;
        b = (b + a) % 65521u;
    }
    for (size_t pos = 0; pos < raw.size();) {
        const size_t n = std::min<size_t>(65535, raw.size() - pos);
        z.push_back(pos + n >= raw.size() ? 1 : 0);
        z.push_back(uint8_t(n & 0xFF));
        z.push_back(uint8_t(n >> 8));
        z.push_back(uint8_t(~n & 0xFF));
        z.push_back(uint8_t((~n >> 8) & 0xFF));
        z.insert(z.end(), raw.begin() + long(pos), raw.begin() + long(pos + n));
        pos += n;
    }
    be32(z, (b << 16) | a);

    std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> ihdr;
    be32(ihdr, uint32_t(w));
    be32(ihdr, uint32_t(h));
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});  // 8-bit RGB
    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", z);
    chunk(out, "IEND", {});

    FILE* f = nullptr;

    f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) {
        return false;
    }
    
    if (!f) return false;
    const bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
    return std::fclose(f) == 0 && ok;
}

}  // namespace liq
