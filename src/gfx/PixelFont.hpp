#pragma once
// Built-in bitmap fonts. No font files are needed: the glyphs are drawn from string art
// in PixelFont.cpp. Two faces:
//   Text5x7   proportional caps/digits/punctuation with 1px strokes (UI, titles, labels)
//   Digits3x5 tiny numerals for small blobs
// Lower-case input is shown as upper-case. Non-ASCII input shows '?'.
#include <cstdint>
#include <string>

namespace liq {

enum class FontFace { Text5x7, Digits3x5 };

struct Glyph {
    uint8_t w = 0;
    uint8_t rows[7] = {};  // bit c set = column c (from the left) is lit
};

class PixelFont {
public:
    static const Glyph& glyph(FontFace f, char c);
    static int height(FontFace f) { return f == FontFace::Text5x7 ? 7 : 5; }
    static std::string ascii(const std::string& utf8);  // fold to printable ASCII
    static int width(FontFace f, const std::string& s);  // in pixels at scale 1

    // Calls fn(x, y, runLength) for every horizontal run of lit pixels (origin 0,0).
    template <class Fn>
    static void forEachRun(FontFace f, const std::string& s, Fn&& fn) {
        const std::string a = ascii(s);
        const int h = height(f);
        int x = 0;
        for (char ch : a) {
            const Glyph& g = glyph(f, ch);
            for (int r = 0; r < h; ++r) {
                int c = 0;
                while (c < g.w) {
                    if ((g.rows[r] >> c) & 1) {
                        const int st = c;
                        while (c < g.w && ((g.rows[r] >> c) & 1)) ++c;
                        fn(x + st, r, c - st);
                    } else {
                        ++c;
                    }
                }
            }
            x += g.w + 1;
        }
    }
};

}  // namespace liq
