#include "gfx/Gfx.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "core/Config.hpp"
#include "core/Platform.hpp"

namespace liq {

namespace {

// 5x7 glyphs, 35 chars each: seven rows of five columns ('1' = lit pixel).
struct GlyphDef {
    char c;
    const char* bits;
};
const GlyphDef kGlyphs[] = {
    {'0', "01110100011001110101110011000101110"}, {'1', "00100011000010000100001000010001110"},
    {'2', "01110100010000100010001000100011111"}, {'3', "11110000010000101110000010000111110"},
    {'4', "00010001100101010010111110001000010"}, {'5', "11111100001111000001000011000101110"},
    {'6', "00110010001000011110100011000101110"}, {'7', "11111000010001000100010000100001000"},
    {'8', "01110100011000101110100011000101110"}, {'9', "01110100011000101111000010001001100"},
    {'A', "01110100011000111111100011000110001"}, {'B', "11110100011000111110100011000111110"},
    {'C', "01110100011000010000100001000101110"}, {'D', "11110100011000110001100011000111110"},
    {'E', "11111100001000011110100001000011111"}, {'F', "11111100001000011110100001000010000"},
    {'G', "01110100011000010111100011000101111"}, {'H', "10001100011000111111100011000110001"},
    {'I', "01110001000010000100001000010001110"}, {'J', "00111000100001000010000101001001100"},
    {'K', "10001100101010011000101001001010001"}, {'L', "10000100001000010000100001000011111"},
    {'M', "10001110111010110101100011000110001"}, {'N', "10001110011010110011100011000110001"},
    {'O', "01110100011000110001100011000101110"}, {'P', "11110100011000111110100001000010000"},
    {'Q', "01110100011000110001101011001001101"}, {'R', "11110100011000111110101001001010001"},
    {'S', "01111100001000001110000010000111110"}, {'T', "11111001000010000100001000010000100"},
    {'U', "10001100011000110001100011000101110"}, {'V', "10001100011000110001100010101000100"},
    {'W', "10001100011000110101101011101110001"}, {'X', "10001100010101000100010101000110001"},
    {'Y', "10001100010101000100001000010000100"}, {'Z', "11111000010001000100010001000011111"},
    {'.', "00000000000000000000000000110001100"}, {',', "00000000000000000000001100011000100"},
    {':', "00000011000110000000001100011000000"}, {'-', "00000000000000011111000000000000000"},
    {'+', "00000001000010011111001000010000000"}, {'/', "00001000100001000100010000100010000"},
    {'!', "00100001000010000100001000000000100"}, {'?', "01110100010000100010001000000000100"},
    {'%', "11001110100001000100010000101110011"}, {'<', "00010001000100010000010000010000010"},
    {'>', "01000001000001000001000100010001000"}, {'x', "00000000001000101010001000101010001"},
};

// SDL_ttf lays a whole string out right-to-left with no bidi pass, which would
// mirror embedded Western digits/Latin words ("90" -> "09"). Pre-reversing each
// ASCII alphanumeric run cancels that, so "90" and "x3" read correctly.
std::string rtlFix(const std::string& s) {
    std::string out = s;
    size_t i = 0;
    while (i < out.size()) {
        if (std::isalnum(static_cast<unsigned char>(out[i])) && static_cast<unsigned char>(out[i]) < 0x80) {
            size_t j = i;
            while (j < out.size() && static_cast<unsigned char>(out[j]) < 0x80 &&
                   std::isalnum(static_cast<unsigned char>(out[j])))
                ++j;
            std::reverse(out.begin() + long(i), out.begin() + long(j));
            i = j;
        } else {
            ++i;
        }
    }
    return out;
}

const char* glyph(unsigned char ch) {
    static std::unordered_map<char, const char*> map;
    if (map.empty())
        for (const auto& g : kGlyphs) map[g.c] = g.bits;
    char u = char(std::toupper(ch));
    auto it = map.find(u);
    if (it != map.end()) return it->second;
    if (ch == ' ') return nullptr;
    return map['?'];
}

}  // namespace

Gfx::~Gfx() {
    for (auto& kv : cache_)
        if (kv.second.t) SDL_DestroyTexture(kv.second.t);
    for (Face* f : {&latin_, &fa_}) {
        for (auto& kv : f->sizes) TTF_CloseFont(kv.second);
        if (f->rw) SDL_RWclose(f->rw);
    }
    if (r_) SDL_DestroyRenderer(r_);
    if (TTF_WasInit()) TTF_Quit();
}

bool Gfx::init(SDL_Window* window) {
    r_ = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!r_) r_ = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!r_) return false;
    SDL_RenderSetLogicalSize(r_, cfg::kW, cfg::kH);
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    TTF_Init();
    return true;
}

bool Gfx::loadFace(Face& f, const std::vector<std::string>& assets, const std::vector<std::string>& absolute) {
    for (const auto& a : assets) {
        f.data = platform::readAsset(a);
        if (!f.data.empty()) break;
    }
    if (f.data.empty())
        for (const auto& p : absolute) {
            f.data = platform::readFile(p);
            if (!f.data.empty()) break;
        }
    if (f.data.empty()) return false;
    f.rw = SDL_RWFromConstMem(f.data.data(), int(f.data.size()));
    // Probe once; real sizes are opened on demand (RWops is re-seeked each time).
    f.valid = font(f, 16) != nullptr;
    return f.valid;
}

void Gfx::loadFonts() {
    // Latin pixel font: optional. Without it the built-in bitmap font is used.
    loadFace(latin_, {"fonts/pixel.ttf", "fonts/main.ttf"}, {});
    // Persian font: assets first, then common system fonts.
    loadFace(fa_, {"fonts/fa.ttf", "fonts/Vazirmatn-Regular.ttf"},
             {"C:/Windows/Fonts/tahoma.ttf", "/usr/share/fonts/truetype/vazirmatn/Vazirmatn-Regular.ttf",
              "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/TTF/DejaVuSans.ttf",
              "/System/Library/Fonts/Supplemental/Arial.ttf", "/system/fonts/NotoNaskhArabic-Regular.ttf",
              "/system/fonts/DroidSansArabic.ttf"});
}

TTF_Font* Gfx::font(Face& f, int pt) {
    auto it = f.sizes.find(pt);
    if (it != f.sizes.end()) return it->second;
    SDL_RWseek(f.rw, 0, RW_SEEK_SET);
    TTF_Font* tf = TTF_OpenFontRW(f.rw, 0, pt);
    // NOTE: one RWops serves several sizes; SDL_ttf re-reads from it lazily, so
    // we reopen a fresh RWops per size to stay safe.
    if (tf) {
        TTF_CloseFont(tf);
        SDL_RWops* rw = SDL_RWFromConstMem(f.data.data(), int(f.data.size()));
        tf = TTF_OpenFontRW(rw, 1, pt);
    }
    if (tf) {
        TTF_SetFontHinting(tf, TTF_HINTING_MONO);
#if SDL_TTF_VERSION_ATLEAST(2, 20, 0)
        if (&f == &fa_) {
            TTF_SetFontDirection(tf, TTF_DIRECTION_RTL);
            TTF_SetFontScriptName(tf, "Arab");
        }
#endif
    }
    f.sizes[pt] = tf;
    return tf;
}

Gfx::Face* Gfx::activeFace() {
    if (persian_) return fa_.valid ? &fa_ : nullptr;
    return latin_.valid ? &latin_ : nullptr;
}

void Gfx::setColor(Color c) { SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, c.a); }

void Gfx::clear(Color c) {
    SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, 255);
    SDL_RenderClear(r_);
}

void Gfx::rect(float x, float y, float w, float h, Color c) {
    setColor(c);
    SDL_Rect rc{int(std::lround(x)), int(std::lround(y)), int(std::lround(w)), int(std::lround(h))};
    SDL_RenderFillRect(r_, &rc);
}

void Gfx::border(float x, float y, float w, float h, int t, Color c) {
    rect(x, y, w, t, c);
    rect(x, y + h - t, w, t, c);
    rect(x, y, t, h, c);
    rect(x + w - t, y, t, h, c);
}

void Gfx::ellipse(float cx, float cy, float rx, float ry, Color c) {
    if (rx < 1 || ry < 1) return;
    setColor(c);
    int icy = int(std::lround(cy));
    int ir = int(ry);
    for (int dy = -ir; dy <= ir; ++dy) {
        float t = float(dy) / ry;
        float w = rx * std::sqrt(std::max(0.f, 1.f - t * t));
        int x0 = int(std::lround(cx - w)), x1 = int(std::lround(cx + w));
        SDL_Rect rc{x0, icy + dy, x1 - x0 + 1, 1};
        SDL_RenderFillRect(r_, &rc);
    }
}

int Gfx::bitmapW(const std::string& s, int scale) const {
    int n = 0;
    for (unsigned char ch : s)
        if ((ch & 0xC0) != 0x80) ++n;
    return n ? n * 6 * scale - scale : 0;
}

void Gfx::bitmap(const std::string& s, float x, float cy, int sc, Color c, Align a) {
    int w = bitmapW(s, sc);
    float x0 = a == Align::Left ? x : (a == Align::Center ? x - w / 2.f : x - w);
    int cx = int(std::lround(x0)), y0 = int(std::lround(cy - 3.5f * sc));
    setColor(c);
    for (unsigned char ch : s) {
        if ((ch & 0xC0) == 0x80) continue;  // UTF-8 continuation byte
        if (ch < 0x80) {
            if (const char* g = glyph(ch)) {
                for (int row = 0; row < 7; ++row) {
                    int col = 0;
                    while (col < 5) {
                        if (g[row * 5 + col] == '1') {
                            int start = col;
                            while (col < 5 && g[row * 5 + col] == '1') ++col;
                            SDL_Rect rc{cx + start * sc, y0 + row * sc, (col - start) * sc, sc};
                            SDL_RenderFillRect(r_, &rc);
                        } else {
                            ++col;
                        }
                    }
                }
            }
        } else if (const char* g = glyph('?')) {
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col)
                    if (g[row * 5 + col] == '1') {
                        SDL_Rect rc{cx + col * sc, y0 + row * sc, sc, sc};
                        SDL_RenderFillRect(r_, &rc);
                    }
        }
        cx += 6 * sc;
    }
}

const Gfx::Tex* Gfx::ttfTex(Face& f, int pt, const std::string& s, Color c) {
    std::string key = std::to_string(&f == &fa_) + "|" + std::to_string(pt) + "|" + std::to_string(c.r) + "," +
                      std::to_string(c.g) + "," + std::to_string(c.b) + "|" + s;
    auto it = cache_.find(key);
    if (it != cache_.end()) return &it->second;
    if (cache_.size() > 300) {
        for (auto& kv : cache_)
            if (kv.second.t) SDL_DestroyTexture(kv.second.t);
        cache_.clear();
    }
    Tex tex;
    if (TTF_Font* tf = font(f, pt)) {
        const std::string shaped = (&f == &fa_) ? rtlFix(s) : s;
        SDL_Surface* surf = TTF_RenderUTF8_Blended(tf, shaped.c_str(), SDL_Color{c.r, c.g, c.b, 255});
        if (surf) {
            tex.t = SDL_CreateTextureFromSurface(r_, surf);
            tex.w = surf->w;
            tex.h = surf->h;
            SDL_FreeSurface(surf);
        }
    }
    return &(cache_[key] = tex);
}

int Gfx::textW(const std::string& s, int scale) {
    if (Face* f = activeFace()) {
        int w = 0, h = 0;
        if (TTF_Font* tf = font(*f, ptFor(scale)))
            TTF_SizeUTF8(tf, (f == &fa_ ? rtlFix(s) : s).c_str(), &w, &h);
        return w;
    }
    return bitmapW(s, scale);
}

void Gfx::text(const std::string& s, float x, float cy, int scale, Color c, Align a) {
    if (s.empty()) return;
    Face* f = activeFace();
    if (!f) {
        bitmap(s, x, cy, scale, c, a);
        return;
    }
    const Tex* t = ttfTex(*f, ptFor(scale), s, c);
    if (!t || !t->t) return;
    float x0 = a == Align::Left ? x : (a == Align::Center ? x - t->w / 2.f : x - t->w);
    SDL_SetTextureAlphaMod(t->t, c.a);
    SDL_Rect dst{int(std::lround(x0)), int(std::lround(cy - t->h / 2.f)), t->w, t->h};
    SDL_RenderCopy(r_, t->t, nullptr, &dst);
}

bool Gfx::screenshot(const std::string& path) {
    int w = 0, h = 0;
    SDL_GetRendererOutputSize(r_, &w, &h);
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return false;
    bool ok = SDL_RenderReadPixels(r_, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch) == 0 &&
              SDL_SaveBMP(s, path.c_str()) == 0;
    SDL_FreeSurface(s);
    return ok;
}

std::vector<std::string> Gfx::wrap(const std::string& s, float maxW, int scale) {
    std::vector<std::string> lines;
    std::string line, word;
    auto flush = [&]() {
        if (word.empty()) return;
        std::string trial = line.empty() ? word : line + " " + word;
        if (!line.empty() && textW(trial, scale) > maxW) {
            lines.push_back(line);
            line = word;
        } else {
            line = trial;
        }
        word.clear();
    };
    for (char ch : s) {
        if (ch == ' ') flush();
        else if (ch == '\n') { flush(); lines.push_back(line); line.clear(); }
        else word += ch;
    }
    flush();
    if (!line.empty()) lines.push_back(line);
    return lines;
}

}  // namespace liq
