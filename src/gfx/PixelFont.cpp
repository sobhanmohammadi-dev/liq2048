#include "gfx/PixelFont.hpp"

#include <array>
#include <cctype>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "core/Platform.hpp"

namespace liq {

namespace {

using Table = std::array<Glyph, 128>;

// Reads a BDF bitmap font (https://en.wikipedia.org/wiki/Glyph_Bitmap_Distribution_Format) from the assets.
// Only the parts the game needs are interpreted: ENCODING, DWIDTH, BBX height and the BITMAP rows (up to 8 columns,
// most significant bit = leftmost column). Anything unknown or malformed is skipped, never fatal.
Table loadBdf(const std::string& rel, int height, char fallback) {
    Table t{};
    const auto data = platform::readAsset(rel);
    if (data.empty()) {
        std::fprintf(stderr, "Pixel font %s is missing\n", rel.c_str());
    }
    std::istringstream in(std::string(data.begin(), data.end()));
    std::string line;
    int enc = -1, w = 0, row = -1;
    Glyph g;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("STARTCHAR", 0) == 0) {
            enc = -1; w = 0; row = -1; g = Glyph();
        } else if (line.rfind("ENCODING ", 0) == 0) {
            enc = std::atoi(line.c_str() + 9);
        } else if (line.rfind("DWIDTH ", 0) == 0) {
            w = std::min(8, std::max(0, std::atoi(line.c_str() + 7)));
            g.w = uint8_t(w);
        } else if (line == "BITMAP") {
            row = 0;
        } else if (line == "ENDCHAR") {
            if (enc >= 0 && enc < 128 && w > 0) t[size_t(enc)] = g;
            row = -1;
        } else if (row >= 0 && row < height && row < 7) {
            const unsigned v = unsigned(std::strtoul(line.c_str(), nullptr, 16)) & 0xFFu;
            uint8_t bits = 0;
            for (int c = 0; c < w; ++c)
                if (v & (0x80u >> c)) bits |= uint8_t(1u << c);
            g.rows[row++] = bits;
        }
    }
    for (Glyph& x : t)
        if (x.w == 0) x = t[size_t(fallback)].w ? t[size_t(fallback)] : Glyph{3, {}};
    return t;
}

const Table& table(FontFace f) {
    static const Table a = loadBdf("fonts/pixel-5x7.bdf", 7, '?');
    static const Table b = loadBdf("fonts/pixel-3x5.bdf", 5, '?');
    return f == FontFace::Text5x7 ? a : b;
}

}  // namespace

const Glyph& PixelFont::glyph(FontFace f, char c) {
    unsigned char u = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(c)));
    if (u >= 128) u = '?';
    return table(f)[u];
}

namespace {
bool g_germanFold = false;

// Folds a Unicode code point to printable ASCII for the pixel font ("" = drop it).
const char* foldCp(unsigned cp) {
    switch (cp) {
        case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC5: return "A";
        case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE5: return "A";
        case 0xC4: case 0xE4: return g_germanFold ? "AE" : "A";
        case 0xC6: case 0xE6: return "AE";
        case 0xC7: case 0xE7: return "C";
        case 0xC8: case 0xC9: case 0xCA: case 0xCB: case 0xE8: case 0xE9: case 0xEA: case 0xEB: return "E";
        case 0xCC: case 0xCD: case 0xCE: case 0xCF: case 0xEC: case 0xED: case 0xEE: case 0xEF: return "I";
        case 0x130: case 0x131: return "I";
        case 0xD1: case 0xF1: return "N";
        case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD8: case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF8: return "O";
        case 0xD6: case 0xF6: return g_germanFold ? "OE" : "O";
        case 0x152: case 0x153: return "OE";
        case 0xD9: case 0xDA: case 0xDB: case 0xF9: case 0xFA: case 0xFB: return "U";
        case 0xDC: case 0xFC: return g_germanFold ? "UE" : "U";
        case 0xDD: case 0xFD: case 0xFF: return "Y";
        case 0xDF: return "SS";
        case 0x11E: case 0x11F: return "G";
        case 0x15E: case 0x15F: return "S";
        case 0xA1: case 0xBF: return "";
        case 0xAB: case 0xBB: case 0x201C: case 0x201D: return "\"";
        case 0x2018: case 0x2019: return "'";
        case 0x2013: case 0x2014: return "-";
        case 0x2026: return "...";
        case 0xA0: return " ";
        default: return "?";
    }
}
}  // namespace

void PixelFont::setGermanFold(bool on) { g_germanFold = on; }

std::string PixelFont::ascii(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        const unsigned char ch = static_cast<unsigned char>(s[i]);
        if (ch < 0x80) {
            out += (ch == '\n' || ch == '\t') ? ' ' : ((ch >= 0x20 && ch < 0x7F) ? char(ch) : '?');
            ++i;
            continue;
        }
        unsigned cp = 0;
        int n = 1;
        if ((ch & 0xE0) == 0xC0) { cp = ch & 0x1F; n = 2; }
        else if ((ch & 0xF0) == 0xE0) { cp = ch & 0x0F; n = 3; }
        else if ((ch & 0xF8) == 0xF0) { cp = ch & 0x07; n = 4; }
        int k = 1;
        for (; k < n && i + size_t(k) < s.size() && (static_cast<unsigned char>(s[i + size_t(k)]) & 0xC0) == 0x80; ++k)
            cp = (cp << 6) | (static_cast<unsigned char>(s[i + size_t(k)]) & 0x3F);
        out += foldCp(cp);
        i += size_t(k);
    }
    return out;
}

int PixelFont::width(FontFace f, const std::string& s) {
    const std::string a = ascii(s);
    int w = 0;
    for (char ch : a) w += glyph(f, ch).w + 1;
    return w > 0 ? w - 1 : 0;
}

}  // namespace liq
