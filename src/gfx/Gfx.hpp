#pragma once
// Rendering layer: SDL renderer at a fixed logical resolution, flat pixel
// primitives, a built-in 5x7 bitmap font (so the game needs no font files) and
// optional SDL_ttf fonts loaded from assets/fonts/.
#include <SDL.h>
#include <SDL_ttf.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "core/Types.hpp"

namespace liq {

class Gfx {
public:
    ~Gfx();
    bool init(SDL_Window* window);
    void loadFonts();
    bool hasPersianFont() const { return fa_.valid; }
    void setPersian(bool on) { persian_ = on; }

    void clear(Color c);
    void present() { SDL_RenderPresent(r_); }
    void rect(float x, float y, float w, float h, Color c);
    void border(float x, float y, float w, float h, int t, Color c);
    void ellipse(float cx, float cy, float rx, float ry, Color c);

    // Language-aware text (TTF if available, else bitmap). y is the vertical centre.
    void text(const std::string& s, float x, float cy, int scale, Color c, Align a = Align::Left);
    int textW(const std::string& s, int scale);
    // Always the built-in bitmap font: used for numbers/Western digits.
    void bitmap(const std::string& s, float x, float cy, int scale, Color c, Align a = Align::Left);
    int bitmapW(const std::string& s, int scale) const;
    std::vector<std::string> wrap(const std::string& s, float maxW, int scale);
    // Saves the current frame buffer as a BMP (call before present()).
    bool screenshot(const std::string& path);

private:
    struct Face {
        bool valid = false;
        std::vector<unsigned char> data;       // must outlive the fonts
        std::unordered_map<int, TTF_Font*> sizes;
        SDL_RWops* rw = nullptr;
    };
    struct Tex {
        SDL_Texture* t = nullptr;
        int w = 0, h = 0;
    };
    bool loadFace(Face& f, const std::vector<std::string>& assets, const std::vector<std::string>& absolute);
    TTF_Font* font(Face& f, int pt);
    Face* activeFace();
    int ptFor(int scale) const { return persian_ ? 8 * scale + 2 : 8 * scale; }
    const Tex* ttfTex(Face& f, int pt, const std::string& s, Color c);
    void setColor(Color c);

    SDL_Renderer* r_ = nullptr;
    Face latin_, fa_;
    bool persian_ = false;
    std::unordered_map<std::string, Tex> cache_;
};

}  // namespace liq
