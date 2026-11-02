#pragma once
// CPU rasterizer for the droplets. Pure software (no SDL renderer), so it is unit-testable.
//
// How a droplet is drawn
//   1. Every blob adds a smooth compact "field" f = (1 - q/S^2)^3 around its ellipse
//      (q = 1 on the ellipse edge). Pixels where the summed field reaches the iso level
//      T = f(edge) are liquid. A single blob is therefore exactly its ellipse, while
//      blobs that touch grow a tight neck and read as one connected shape.
//   2. Colour: each blob has a second, wider kernel. The two strongest kernels at a pixel
//      decide the colour. With dithering on, an ordered 8x8 Bayer matrix picks between the
//      two palette colours (a pure pixel-art gradient); otherwise the colours are mixed.
//   3. A 1px outline (the fill darkened toward the theme ink) runs along the silhouette only,
//      so touching blobs share one outline and the blended colours are not cut by lines.
// There is no highlight, glow or shading: droplets are flat colour plus outline.
#include <cstdint>
#include <vector>

#include "core/Themes.hpp"
#include "core/Types.hpp"

namespace liq {

class BlobRaster {
public:
    BlobRaster(int w, int h);

    void clear();  // fully transparent
    // Adds droplets. May be called several times per frame for independent groups
    // (each call has its own field, so groups never fuse with each other).
    // Fast path for particle liquids: every sprite is one fluid particle (x, y, level; radii ignored),
    // snapped to the pixel grid. fieldR / colorR are kernel radii in pixels.
    void drawLiquid(const std::vector<BlobSprite>& particles, const Theme& theme, int fieldR, int colorR, float iso);
    void drawField(const std::vector<BlobSprite>& sprites, const Theme& theme, float iso = -1.f);
    void drawLabels(const std::vector<BlobSprite>& sprites, const Theme& theme);

    int width() const { return w_; }
    int height() const { return h_; }
    const uint32_t* pixels() const { return px_.data(); }  // ARGB8888
    uint32_t at(int x, int y) const { return px_[size_t(y) * size_t(w_) + size_t(x)]; }
    int coveredPixels() const;  // number of non-transparent pixels (tests)

private:
    int w_, h_;
    std::vector<uint32_t> px_;
    std::vector<float> f_, w1_, w2_, tmp_;
    std::vector<uint32_t> c1_, c2_;
};

}  // namespace liq
