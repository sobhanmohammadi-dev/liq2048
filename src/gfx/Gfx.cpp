#include "gfx/Gfx.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "gfx/BlobRaster.hpp"

namespace liq {

namespace {

// SDL_ttf lays a whole string out right-to-left with no bidi pass, which would
// mirror embedded Western digits/Latin words ("90" -> "09"). Pre-reversing each
// ASCII alphanumeric run cancels that, so "90" and "FPS" read correctly.
std::string rtlFix(const std::string& s) {
    std::string out = s;
    size_t i = 0;
    while (i < out.size()) {
        const auto isAn = [](char ch) { return static_cast<unsigned char>(ch) < 0x80 && std::isalnum(static_cast<unsigned char>(ch)); };
        if (isAn(out[i])) {
            size_t j = i;
            while (j < out.size() && isAn(out[j])) ++j;
            std::reverse(out.begin() + long(i), out.begin() + long(j));
            i = j;
        } else {
            ++i;
        }
    }
    return out;
}

constexpr uint8_t kBayer4[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

}  // namespace

Gfx::~Gfx() {
    for (auto& kv : cache_)
        if (kv.second.t) SDL_DestroyTexture(kv.second.t);
    for (Face* f : {&latin_, &fa_})
        for (auto& kv : f->sizes)
            if (kv.second) TTF_CloseFont(kv.second);
    for (SDL_Texture* t : {bgTex_, tankTex_, rasterTex_, dimTex_[0], dimTex_[1], dimTex_[2]})
        if (t) SDL_DestroyTexture(t);
    if (r_) SDL_DestroyRenderer(r_);
    if (TTF_WasInit()) TTF_Quit();
}

bool Gfx::init(SDL_Window* window, bool integerScale) {
    r_ = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!r_) r_ = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!r_) return false;
    SDL_RenderSetLogicalSize(r_, cfg::kW, cfg::kH);
    SDL_RenderSetIntegerScale(r_, integerScale ? SDL_TRUE : SDL_FALSE);
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    TTF_Init();

    rasterTex_ = makeTexture(cfg::kW, cfg::kH, SDL_TEXTUREACCESS_STREAMING, true);
    // Pre-built dither dimmers (25% / 50% / 75% of pixels covered with the ink colour, white now,
    // tinted with a colour mod at draw time).
    for (int lv = 0; lv < 3; ++lv) {
        dimTex_[lv] = makeTexture(cfg::kW, cfg::kH, SDL_TEXTUREACCESS_STATIC, true);
        std::vector<uint32_t> px(size_t(cfg::kW) * size_t(cfg::kH));
        const int cover = (lv + 1) * 4;  // out of 16
        for (int y = 0; y < cfg::kH; ++y)
            for (int x = 0; x < cfg::kW; ++x) px[size_t(y) * cfg::kW + size_t(x)] = kBayer4[y & 3][x & 3] < cover ? 0xFFFFFFFFu : 0u;
        if (dimTex_[lv]) SDL_UpdateTexture(dimTex_[lv], nullptr, px.data(), cfg::kW * 4);
    }
    return rasterTex_ != nullptr;
}

SDL_Texture* Gfx::makeTexture(int w, int h, SDL_TextureAccess acc, bool blend) {
    SDL_Texture* t = SDL_CreateTexture(r_, SDL_PIXELFORMAT_ARGB8888, acc, w, h);
    if (t) {
        SDL_SetTextureBlendMode(t, blend ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
        SDL_SetTextureScaleMode(t, SDL_ScaleModeNearest);
    }
    return t;
}

void Gfx::setVsync(bool on) {
#if SDL_VERSION_ATLEAST(2, 0, 18)
    SDL_RenderSetVSync(r_, on ? 1 : 0);
#else
    (void)on;
#endif
}

// ------------------------------------------------------------------------------------------
// Fonts (Persian + optional Latin pixel TTF)
// ------------------------------------------------------------------------------------------
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
    f.valid = font(f, 12) != nullptr;
    return f.valid;
}

void Gfx::loadFonts() {
    // Optional Latin pixel font. Without it the built-in bitmap font family is used.
    loadFace(latin_, {"fonts/pixel.ttf", "fonts/main.ttf"}, {});
    // Persian font: assets first, then common system fonts.
    loadFace(fa_, {"fonts/fa.ttf", "fonts/Vazirmatn-Regular.ttf"},
             {"C:/Windows/Fonts/tahomabd.ttf", "C:/Windows/Fonts/tahoma.ttf",
              "/usr/share/fonts/truetype/vazirmatn/Vazirmatn-Regular.ttf",
              "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
              "/usr/share/fonts/TTF/DejaVuSans.ttf", "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
              "/System/Library/Fonts/Supplemental/Arial.ttf", "/system/fonts/NotoNaskhArabic-Regular.ttf",
              "/system/fonts/DroidSansArabic.ttf", "/system/fonts/NotoNaskhArabicUI-Regular.ttf",
              "/system/fonts/NotoSansArabic-Regular.ttf", "/system/fonts/NotoSansArabicUI-Regular.ttf",
              "/System/Library/Fonts/Core/GeezaPro.ttc", "/System/Library/Fonts/GeezaPro.ttc"});
}

TTF_Font* Gfx::font(Face& f, int pt) {
    auto it = f.sizes.find(pt);
    if (it != f.sizes.end()) return it->second;
    SDL_RWops* rw = SDL_RWFromConstMem(f.data.data(), int(f.data.size()));
    TTF_Font* tf = TTF_OpenFontRW(rw, 1, pt);  // closes rw with the font; the data vector outlives it
    if (tf) {
        TTF_SetFontHinting(tf, TTF_HINTING_NORMAL);
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

// White, hard-edged glyph texture (tinted with a colour mod when drawn).
const Gfx::Tex* Gfx::ttfTex(Face& f, int pt, const std::string& s) {
    const std::string key = std::to_string(&f == &fa_) + "|" + std::to_string(pt) + "|" + s;
    auto it = cache_.find(key);
    if (it != cache_.end()) return &it->second;
    if (cache_.size() > 400) {
        for (auto& kv : cache_)
            if (kv.second.t) SDL_DestroyTexture(kv.second.t);
        cache_.clear();
    }
    Tex tex;
    if (TTF_Font* tf = font(f, pt)) {
        const std::string shaped = (&f == &fa_) ? rtlFix(s) : s;
        if (SDL_Surface* surf = TTF_RenderUTF8_Blended(tf, shaped.c_str(), SDL_Color{255, 255, 255, 255})) {
            SDL_Surface* conv = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_ARGB8888, 0);
            SDL_FreeSurface(surf);
            if (conv) {
                // Hard threshold: no anti-aliasing, so text matches the pixel-art look.
                auto* p = static_cast<uint32_t*>(conv->pixels);
                for (int y = 0; y < conv->h; ++y) {
                    uint32_t* row = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(p) + size_t(y) * size_t(conv->pitch));
                    for (int x = 0; x < conv->w; ++x) row[x] = (row[x] >> 24) >= 110 ? 0xFFFFFFFFu : 0u;
                }
                tex.t = SDL_CreateTextureFromSurface(r_, conv);
                if (tex.t) SDL_SetTextureScaleMode(tex.t, SDL_ScaleModeNearest);
                tex.w = conv->w;
                tex.h = conv->h;
                SDL_FreeSurface(conv);
            }
        }
    }
    return &(cache_[key] = tex);
}

// ------------------------------------------------------------------------------------------
// Primitives
// ------------------------------------------------------------------------------------------
void Gfx::clear(Color c) {
    SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, 255);
    SDL_RenderClear(r_);
}

void Gfx::rect(float x, float y, float w, float h, Color c) {
    SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, c.a);
    SDL_Rect rc{int(std::lround(x)) + ox_, int(std::lround(y)) + oy_, int(std::lround(w)), int(std::lround(h))};
    if (rc.w > 0 && rc.h > 0) SDL_RenderFillRect(r_, &rc);
}

void Gfx::border(float x, float y, float w, float h, int t, Color c) {
    rect(x, y, w, float(t), c);
    rect(x, y + h - float(t), w, float(t), c);
    rect(x, y + float(t), float(t), h - 2.f * float(t), c);
    rect(x + w - float(t), y + float(t), float(t), h - 2.f * float(t), c);
}

void Gfx::disc(float cx, float cy, int r, Color c) {
    const int icx = int(std::lround(cx)), icy = int(std::lround(cy));
    for (int dy = -r; dy <= r; ++dy) {
        const int half = int(std::floor(std::sqrt(float((r + 0.5f) * (r + 0.5f) - float(dy * dy)))));
        rect(float(icx - half), float(icy + dy), float(half * 2 + 1), 1.f, c);
    }
}

void Gfx::ring(float cx, float cy, int r, Color c, int dashMask) {
    const int icx = int(std::lround(cx)), icy = int(std::lround(cy));
    int x = r, y = 0, err = 1 - r, n = 0;
    auto plot = [&](int px, int py) {
        if (dashMask > 0 && ((n / dashMask) & 1)) return;
        rect(float(icx + px), float(icy + py), 1, 1, c);
    };
    while (x >= y) {
        plot(x, y); plot(y, x); plot(-y, x); plot(-x, y);
        plot(-x, -y); plot(-y, -x); plot(y, -x); plot(x, -y);
        ++y;
        ++n;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            --x;
            err += 2 * (y - x) + 1;
        }
    }
}

void Gfx::panel(float fx, float fy, float fw, float fh, Color fill, Color hi, Color lo, Color edge) {
    const int x = int(std::lround(fx)), y = int(std::lround(fy)), w = int(std::lround(fw)), h = int(std::lround(fh));
    if (w < 4 || h < 4) return;
    rect(float(x + 1), float(y), float(w - 2), 1, edge);
    rect(float(x + 1), float(y + h - 1), float(w - 2), 1, edge);
    rect(float(x), float(y + 1), 1, float(h - 2), edge);
    rect(float(x + w - 1), float(y + 1), 1, float(h - 2), edge);
    rect(float(x + 1), float(y + 1), float(w - 2), float(h - 2), fill);
    rect(float(x + 2), float(y + 1), float(w - 4), 1, hi);
    rect(float(x + 1), float(y + 2), 1, float(h - 4), hi);
    rect(float(x + 2), float(y + h - 2), float(w - 4), 1, lo);
    rect(float(x + w - 2), float(y + 2), 1, float(h - 4), lo);
}

void Gfx::icon(IconId id, float fx, float fy, Color c, int scale) {
    const IconDef& d = iconDef(id);
    for (int row = 0; row < d.h; ++row) {
        int col = 0;
        while (col < d.w) {
            if (d.rows[row][col] == '#') {
                const int st = col;
                while (col < d.w && d.rows[row][col] == '#') ++col;
                rect(fx + float(st * scale), fy + float(row * scale), float((col - st) * scale), float(scale), c);
            } else {
                ++col;
            }
        }
    }
}

void Gfx::dim(const Theme& t, int level) {
    SDL_Texture* tex = dimTex_[std::min(std::max(level, 0), 2)];
    if (!tex) return;
    SDL_SetTextureColorMod(tex, t.ink.r, t.ink.g, t.ink.b);
    SDL_RenderCopy(r_, tex, nullptr, nullptr);
}

void Gfx::blinds(float amount, Color c) {
    if (amount <= 0.f) return;
    constexpr int kBands = 15;
    const float bh = float(cfg::kH) / kBands;
    for (int i = 0; i < kBands; ++i) {
        // Each band starts slightly later than the previous one for a pixel "venetian" sweep.
        float local = std::min(1.f, std::max(0.f, amount * 1.6f - float(i) / kBands * 0.6f));
        // Snap to 6px steps so the edge moves in chunky pixel increments.
        local = std::floor(local * float(cfg::kW) / 6.f + 0.5f) * 6.f / float(cfg::kW);
        const float w = std::min(float(cfg::kW), local * float(cfg::kW));
        if (i & 1) rect(float(cfg::kW) - w, bh * float(i), w, bh + 1, c);
        else rect(0, bh * float(i), w, bh + 1, c);
    }
}

// ------------------------------------------------------------------------------------------
// Text
// ------------------------------------------------------------------------------------------
int Gfx::numberW(const std::string& s, int scale) const { return PixelFont::width(FontFace::Text5x7, s) * scale; }

int Gfx::lineH(int scale) const {
    if (persian_ && fa_.valid) return scale <= 1 ? 15 : 26;
    return 7 * scale + 5;
}

void Gfx::bitmapAt(const std::string& s, FontFace face, int x0, int y0, int scale, Color c) {
    SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, c.a);
    PixelFont::forEachRun(face, s, [&](int x, int y, int len) {
        SDL_Rect rc{x0 + x * scale + ox_, y0 + y * scale + oy_, len * scale, scale};
        SDL_RenderFillRect(r_, &rc);
    });
}

void Gfx::number(const std::string& s, float x, float cy, int scale, Color c, Align a, TextFx fx, Color fxc) {
    if (s.empty()) return;
    const int w = numberW(s, scale);
    const float xl = a == Align::Left ? x : (a == Align::Center ? x - float(w) / 2.f : x - float(w));
    const int x0 = int(std::floor(xl + 0.5f)), y0 = int(std::floor(cy - 3.5f * float(scale) + 0.5f));
    if (fx == TextFx::Outline) {
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if (dx || dy) bitmapAt(s, FontFace::Text5x7, x0 + dx, y0 + dy, scale, fxc);
    } else if (fx == TextFx::Shadow) {
        bitmapAt(s, FontFace::Text5x7, x0, y0 + std::max(1, scale / 2 + (scale > 1 ? 1 : 0)), scale, fxc);
    }
    bitmapAt(s, FontFace::Text5x7, x0, y0, scale, c);
}

int Gfx::textW(const std::string& s, int scale) {
    if (Face* f = activeFace()) {
        if (const Tex* t = ttfTex(*f, ptFor(scale, persian_), s)) return t->w;
        return 0;
    }
    return PixelFont::width(FontFace::Text5x7, s) * scale;
}

void Gfx::drawTtf(const std::string& s, int x, int cy, int scale, Color c, Align a, TextFx fx, Color fxc) {
    Face* f = activeFace();
    if (!f) return;
    const Tex* t = ttfTex(*f, ptFor(scale, persian_), s);
    if (!t || !t->t) return;
    const int xl = a == Align::Left ? x : (a == Align::Center ? x - t->w / 2 : x - t->w);
    const int yt = cy - t->h / 2;
    auto blit = [&](int dx, int dy, Color col) {
        SDL_SetTextureColorMod(t->t, col.r, col.g, col.b);
        SDL_SetTextureAlphaMod(t->t, col.a);
        SDL_Rect dst{xl + dx + ox_, yt + dy + oy_, t->w, t->h};
        SDL_RenderCopy(r_, t->t, nullptr, &dst);
    };
    if (fx == TextFx::Outline) {
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if (dx || dy) blit(dx, dy, fxc);
    } else if (fx == TextFx::Shadow) {
        blit(1, 1, fxc);
    }
    blit(0, 0, c);
}

void Gfx::text(const std::string& s, float x, float cy, int scale, Color c, Align a, TextFx fx, Color fxc) {
    if (s.empty()) return;
    if (activeFace()) {
        drawTtf(s, int(std::floor(x + 0.5f)), int(std::floor(cy + 0.5f)), scale, c, a, fx, fxc);
        return;
    }
    number(s, x, cy, scale, c, a, fx, fxc);
}

std::vector<std::string> Gfx::wrap(const std::string& s, float maxW, int scale) {
    std::vector<std::string> lines;
    std::string line, word;
    auto flush = [&]() {
        if (word.empty()) return;
        const std::string trial = line.empty() ? word : line + " " + word;
        if (!line.empty() && float(textW(trial, scale)) > maxW) {
            lines.push_back(line);
            line = word;
        } else {
            line = trial;
        }
        word.clear();
    };
    for (size_t i = 0; i < s.size(); ++i) {
        const char ch = s[i];
        if (ch == ' ') {
            flush();
        } else if (ch == '\n') {
            flush();
            lines.push_back(line);
            line.clear();
        } else {
            word += ch;
        }
    }
    flush();
    if (!line.empty()) lines.push_back(line);
    return lines;
}

// ------------------------------------------------------------------------------------------
// Backdrop, tank background, droplet layer
// ------------------------------------------------------------------------------------------
void Gfx::buildBackdrop(const Theme& t) {
    ink_ = t.ink;
    if (bgTex_) SDL_DestroyTexture(bgTex_);
    if (tankTex_) SDL_DestroyTexture(tankTex_);
    bgTex_ = makeTexture(cfg::kW, cfg::kH, SDL_TEXTUREACCESS_STATIC, false);
    tankTex_ = makeTexture(cfg::kW, cfg::kH, SDL_TEXTUREACCESS_STATIC, true);

    // Vertical gradient between the 5 sky colours, blended with an 8x8-ish ordered dither
    // (4x4 Bayer here) so the image only ever contains palette colours.
    std::vector<uint32_t> px(size_t(cfg::kW) * size_t(cfg::kH));
    for (int y = 0; y < cfg::kH; ++y) {
        const float p = float(y) / float(cfg::kH - 1) * 4.f;
        const int band = std::min(3, int(p));
        const float frac = p - float(band);
        for (int x = 0; x < cfg::kW; ++x) {
            const bool next = frac * 16.f > float(kBayer4[y & 3][x & 3]) + 0.5f;
            px[size_t(y) * cfg::kW + size_t(x)] = pack(t.sky[next ? band + 1 : band]);
        }
    }
    if (bgTex_) SDL_UpdateTexture(bgTex_, nullptr, px.data(), cfg::kW * 4);

    // Tank interior with a faint dot grid for a sense of scale.
    std::fill(px.begin(), px.end(), 0u);
    const int L = int(cfg::kFieldL), R = int(cfg::kFieldR), T = cfg::kTankTop, B = int(cfg::kFloor);
    for (int y = T; y < B; ++y) {
        for (int x = L; x < R; ++x) {
            const bool dot = ((x - L) % 14 == 7) && ((y - T) % 14 == 7);
            px[size_t(y) * cfg::kW + size_t(x)] = pack(dot ? t.tankDot : t.tank);
        }
    }
    if (tankTex_) SDL_UpdateTexture(tankTex_, nullptr, px.data(), cfg::kW * 4);

    // Stars: fixed pseudo-random placement so the sky is stable between runs.
    stars_.clear();
    uint32_t s = 12345u;
    auto rnd = [&]() { s = s * 1664525u + 1013904223u; return (s >> 8) & 0xFFFFu; };
    for (int i = 0; i < 70; ++i) {
        Star st;
        st.x = int(rnd() % unsigned(cfg::kW));
        st.y = int(rnd() % unsigned(cfg::kH));
        st.kind = i < 56 ? 0 : (i < 66 ? 1 : 2);
        st.phase = float(rnd() % 628) / 100.f;
        stars_.push_back(st);
    }
}

void Gfx::drawBackdrop(const Theme& t, float time) {
    if (!bgTex_) buildBackdrop(t);
    SDL_RenderCopy(r_, bgTex_, nullptr, nullptr);
    for (const Star& st : stars_) {
        const float tw = std::sin(time * (st.kind == 0 ? 0.9f : 1.7f) + st.phase);
        if (st.kind == 0) {
            rect(float(st.x), float(st.y), 1, 1, tw > 0.2f ? t.star : mix(t.star, t.sky[2], 0.65f));
        } else if (st.kind == 1) {
            const Color c = tw > 0.f ? t.star : mix(t.star, t.sky[2], 0.5f);
            rect(float(st.x), float(st.y), 2, 2, c);
        } else if (tw > -0.2f) {  // plus-shaped sparkle that blinks
            const Color c = tw > 0.6f ? t.star2 : t.star;
            rect(float(st.x), float(st.y), 1, 1, c);
            if (tw > 0.3f) {
                rect(float(st.x - 1), float(st.y), 1, 1, c);
                rect(float(st.x + 1), float(st.y), 1, 1, c);
                rect(float(st.x), float(st.y - 1), 1, 1, c);
                rect(float(st.x), float(st.y + 1), 1, 1, c);
            }
        }
    }
}

void Gfx::drawTankBg() {
    if (!tankTex_) return;
    SDL_Rect dst{ox_, oy_, cfg::kW, cfg::kH};
    SDL_RenderCopy(r_, tankTex_, nullptr, &dst);
}

void Gfx::raster(const BlobRaster& r) {
    if (!rasterTex_) return;
    SDL_UpdateTexture(rasterTex_, nullptr, r.pixels(), r.width() * 4);
    SDL_Rect dst{ox_, oy_, cfg::kW, cfg::kH};
    SDL_RenderCopy(r_, rasterTex_, nullptr, &dst);
}

bool Gfx::screenshot(const std::string& path) {
    int w = 0, h = 0;
    SDL_GetRendererOutputSize(r_, &w, &h);
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return false;
    const bool ok = SDL_RenderReadPixels(r_, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch) == 0 &&
                    SDL_SaveBMP(s, path.c_str()) == 0;
    SDL_FreeSurface(s);
    return ok;
}

}  // namespace liq
