#pragma once
// Rendering layer. Everything is drawn on a fixed 270x480 pixel grid (SDL logical size)
// that is scaled up with nearest-neighbour filtering, so pixel art stays perfectly crisp.
//
// Text: Latin uses the built-in bitmap font family (no files needed) unless
// assets/fonts/pixel.ttf exists. Persian uses SDL_ttf; glyphs are thresholded to hard
// edges so they match the pixel look. All numbers use the bitmap font (Western digits).
#include <SDL.h>
#include <SDL_ttf.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "core/Themes.hpp"
#include "core/Types.hpp"
#include "gfx/Icons.hpp"
#include "gfx/PixelFont.hpp"

namespace liq {

class BlobRaster;

enum class TextFx { None, Shadow, Outline };

class Gfx {
public:
    ~Gfx();
    bool init(SDL_Window* window, bool integerScale);
    void loadFonts();
    bool hasPersianFont() const { return fa_.valid; }
    void setPersian(bool on) { persian_ = on; }
    void setVsync(bool on);

    // Offset applied to every following draw call (used for screen shake).
    void setOffset(int dx, int dy) { ox_ = dx; oy_ = dy; }

    void clear(Color c);
    // Paints the window area outside the logical screen (phones taller than 9:16, wide desktop windows)
    // with the theme gradient, so the game always fills the whole display.
    void fillBars(const Theme& t);
    void present() { SDL_RenderPresent(r_); }

    // ---- primitives (coordinates are pixel-grid units, rounded to whole pixels) ----
    void rect(float x, float y, float w, float h, Color c);
    void border(float x, float y, float w, float h, int t, Color c);
    void disc(float cx, float cy, int r, Color c);
    void ring(float cx, float cy, int r, Color c, int dashMask = 0);  // 1px circle; dashMask>0 skips arcs
    // Pixel-art panel: ink edge with notched corners, fill, light top/left, dark bottom/right.
    void panel(float x, float y, float w, float h, Color fill, Color hi, Color lo, Color edge);
    void icon(IconId id, float x, float y, Color c, int scale = 1);  // (x,y) = top-left
    // Dim the whole screen with an ordered-dither pattern; level 0..2 (25%, 50%, 75%).
    void dim(const Theme& t, int level);
    void blinds(float amount, Color c);  // scene transition: 0 = clear, 1 = covered

    // ---- text ----
    // cy is the vertical centre of the text line. scale 1 = body (7px), 2 = heading (14px).
    void text(const std::string& s, float x, float cy, int scale, Color c, Align a = Align::Left,
              TextFx fx = TextFx::None, Color fxColor = Color{0, 0, 0, 255});
    // Numbers and Western digits: always the bitmap font, any integer scale >= 1.
    void number(const std::string& s, float x, float cy, int scale, Color c, Align a = Align::Left,
                TextFx fx = TextFx::None, Color fxColor = Color{0, 0, 0, 255});
    int textW(const std::string& s, int scale);
    int numberW(const std::string& s, int scale) const;
    int lineH(int scale) const;
    std::vector<std::string> wrap(const std::string& s, float maxW, int scale);

    // ---- backdrop and droplets ----
    void buildBackdrop(const Theme& t);
    void drawBackdrop(const Theme& t, float time);  // gradient + twinkling stars
    void drawTankBg();                              // tank interior (dotted grid)
    void raster(const BlobRaster& r);               // upload + draw the droplet layer

    bool screenshot(const std::string& path);

private:
    struct Face {
        bool valid = false;
        std::vector<unsigned char> data;  // must outlive the fonts
        std::unordered_map<int, TTF_Font*> sizes;
    };
    struct Tex {
        SDL_Texture* t = nullptr;
        int w = 0, h = 0;
    };
    bool loadFace(Face& f, const std::vector<std::string>& assets, const std::vector<std::string>& absolute);
    TTF_Font* font(Face& f, int pt);
    Face* activeFace();
    static int ptFor(int scale, bool persian) { return persian ? (scale <= 1 ? 11 : 19) : 8 * scale; }
    const Tex* ttfTex(Face& f, int pt, const std::string& s);
    void drawTtf(const std::string& s, int x, int cy, int scale, Color c, Align a, TextFx fx, Color fxc);
    void bitmapAt(const std::string& s, FontFace face, int x0, int y0, int scale, Color c);
    SDL_Texture* makeTexture(int w, int h, SDL_TextureAccess acc, bool blend);

    SDL_Renderer* r_ = nullptr;
    Face latin_, fa_;
    bool persian_ = false;
    int ox_ = 0, oy_ = 0;
    std::unordered_map<std::string, Tex> cache_;
    SDL_Texture* bgTex_ = nullptr;
    SDL_Texture* tankTex_ = nullptr;
    SDL_Texture* rasterTex_ = nullptr;
    SDL_Texture* dimTex_[3] = {nullptr, nullptr, nullptr};
    struct Star {
        int x, y, kind;
        float phase;
    };
    std::vector<Star> stars_;
    Color ink_{0, 0, 0, 255};
};

}  // namespace liq
