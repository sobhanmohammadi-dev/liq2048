#include "gfx/BlobRaster.hpp"

#include <algorithm>
#include <cmath>

#include "core/Config.hpp"
#include "gfx/PixelFont.hpp"

namespace liq {

namespace {

// 8x8 Bayer matrix, values 0..63.
constexpr uint8_t kBayer[8][8] = {
    {0, 32, 8, 40, 2, 34, 10, 42},   {48, 16, 56, 24, 50, 18, 58, 26}, {12, 44, 4, 36, 14, 46, 6, 38},
    {60, 28, 52, 20, 62, 30, 54, 22}, {3, 35, 11, 43, 1, 33, 9, 41},    {51, 19, 59, 27, 49, 17, 57, 25},
    {15, 47, 7, 39, 13, 45, 5, 37},  {63, 31, 55, 23, 61, 29, 53, 21},
};

inline float bayer(int x, int y) { return (float(kBayer[y & 7][x & 7]) + 0.5f) / 64.f; }

inline uint32_t mixPacked(uint32_t a, uint32_t b, float t) { return pack(mix(unpack(a), unpack(b), t)); }

}  // namespace

BlobRaster::BlobRaster(int w, int h)
    : w_(w), h_(h), px_(size_t(w) * size_t(h), 0), f_(px_.size()), w1_(px_.size()), w2_(px_.size()),
      c1_(px_.size()), c2_(px_.size()) {}

void BlobRaster::clear() { std::fill(px_.begin(), px_.end(), 0u); }

int BlobRaster::coveredPixels() const {
    int n = 0;
    for (uint32_t p : px_) n += (p >> 24) != 0;
    return n;
}

void BlobRaster::drawField(const std::vector<BlobSprite>& sprites, const Theme& theme, float iso) {
    if (sprites.empty()) return;
    const float S = vis::kShapeSupport, S2 = S * S;
    const float C = vis::kColorSupport, C2 = C * C;
    const float T = iso > 0.f ? iso : std::pow(1.f - 1.f / S2, 3.f);  // field value on a lone blob's edge

    std::fill(f_.begin(), f_.end(), 0.f);
    std::fill(w1_.begin(), w1_.end(), 0.f);
    std::fill(w2_.begin(), w2_.end(), 0.f);

    int minY = h_, maxY = -1, minX = w_, maxX = -1;
    for (const BlobSprite& s : sprites) {
        const uint32_t col = pack(theme.blob[std::min(std::max(s.level, 0), 11)]);
        const float ra = std::max(s.ra, 1.f), rb = std::max(s.rb, 1.f);
        const float ia2 = 1.f / (ra * ra), ib2 = 1.f / (rb * rb);
        const float ext = std::max(ra, rb) * std::max(S, C);
        const int x0 = std::max(0, int(std::floor(s.x - ext))), x1 = std::min(w_ - 1, int(std::ceil(s.x + ext)));
        const int y0 = std::max(0, int(std::floor(s.y - ext))), y1 = std::min(h_ - 1, int(std::ceil(s.y + ext)));
        if (x0 > x1 || y0 > y1) continue;
        minX = std::min(minX, x0);
        maxX = std::max(maxX, x1);
        minY = std::min(minY, y0);
        maxY = std::max(maxY, y1);

        for (int y = y0; y <= y1; ++y) {
            const float dy = float(y) + 0.5f - s.y;
            for (int x = x0; x <= x1; ++x) {
                const float dx = float(x) + 0.5f - s.x;
                const float u = dx * s.cs + dy * s.sn;
                const float v = -dx * s.sn + dy * s.cs;
                const float q = u * u * ia2 + v * v * ib2;
                const size_t i = size_t(y) * size_t(w_) + size_t(x);
                if (q < S2) {
                    const float t = 1.f - q / S2;
                    f_[i] += t * t * t;
                }
                if (q < C2) {
                    const float t = 1.f - q / C2;
                    const float wt = t * t;  // colour weight
                    if (wt > w1_[i]) {
                        w2_[i] = w1_[i];
                        c2_[i] = c1_[i];
                        w1_[i] = wt;
                        c1_[i] = col;
                    } else if (wt > w2_[i]) {
                        w2_[i] = wt;
                        c2_[i] = col;
                    }
                }
            }
        }
    }
    if (maxY < 0) return;

    const uint32_t ink = pack(theme.ink);
    auto inside = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < w_ && y < h_ && f_[size_t(y) * size_t(w_) + size_t(x)] >= T;
    };

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const size_t i = size_t(y) * size_t(w_) + size_t(x);
            if (f_[i] < T) {
                // Fill single-pixel holes where two liquids meet.
                if (!(w1_[i] > 0.f && inside(x - 1, y) && inside(x + 1, y) && inside(x, y - 1) && inside(x, y + 1))) continue;
            }
            uint32_t col = c1_[i];
            const float total = w1_[i] + w2_[i];
            if (total > 0.f && w2_[i] > 0.f) {
                const float t = w2_[i] / total;  // 0..0.5 share of the second colour
                if (vis::kDitherBlend) {
                    if (t > bayer(x, y)) col = c2_[i];
                } else {
                    col = mixPacked(c1_[i], c2_[i], std::floor(t * 10.f + 0.5f) / 10.f);
                }
            }
            // Silhouette edge (4-neighbourhood): darker outline of the local colour.
            const bool edge = !inside(x - 1, y) || !inside(x + 1, y) || !inside(x, y - 1) || !inside(x, y + 1);
            px_[i] = edge ? mixPacked(col, ink, vis::kOutlineMix) : col;
        }
    }
}

void BlobRaster::drawLiquid(const std::vector<BlobSprite>& parts, const Theme& theme, int fieldR, int colorR, float iso) {
    if (parts.empty()) return;
    // Kernel tables (cached): field (1-q)^3 and colour weight (1-q)^2, q = d^2 / R^2.
    static int cachedF = -1, cachedC = -1;
    static std::vector<float> kf, kc;
    if (cachedF != fieldR || cachedC != colorR) {
        cachedF = fieldR;
        cachedC = colorR;
        const int nf = 2 * fieldR + 1, nc = 2 * colorR + 1;
        kf.assign(size_t(nf) * size_t(nf), 0.f);
        kc.assign(size_t(nc) * size_t(nc), 0.f);
        for (int y = -fieldR; y <= fieldR; ++y)
            for (int x = -fieldR; x <= fieldR; ++x) {
                const float q = float(x * x + y * y) / float((fieldR + 0.5f) * (fieldR + 0.5f));
                if (q < 1.f) kf[size_t(y + fieldR) * size_t(nf) + size_t(x + fieldR)] = (1.f - q) * (1.f - q) * (1.f - q);
            }
        for (int y = -colorR; y <= colorR; ++y)
            for (int x = -colorR; x <= colorR; ++x) {
                const float q = float(x * x + y * y) / float((colorR + 0.5f) * (colorR + 0.5f));
                if (q < 1.f) kc[size_t(y + colorR) * size_t(nc) + size_t(x + colorR)] = (1.f - q) * (1.f - q);
            }
    }
    std::fill(f_.begin(), f_.end(), 0.f);
    std::fill(w1_.begin(), w1_.end(), 0.f);
    std::fill(w2_.begin(), w2_.end(), 0.f);
    const int nf = 2 * fieldR + 1, nc = 2 * colorR + 1;
    int minY = h_, maxY = -1, minX = w_, maxX = -1;
    for (const BlobSprite& s : parts) {
        const int cx = int(std::floor(s.x)), cy = int(std::floor(s.y));
        const uint32_t col = pack(theme.blob[std::min(std::max(s.level, 0), 11)]);
        const int x0 = std::max(0, cx - fieldR), x1 = std::min(w_ - 1, cx + fieldR);
        const int y0 = std::max(0, cy - fieldR), y1 = std::min(h_ - 1, cy + fieldR);
        if (x0 > x1 || y0 > y1) continue;
        minX = std::min(minX, x0);
        maxX = std::max(maxX, x1);
        minY = std::min(minY, y0);
        maxY = std::max(maxY, y1);
        for (int y = y0; y <= y1; ++y) {
            const float* row = &kf[size_t(y - cy + fieldR) * size_t(nf) + size_t(x0 - cx + fieldR)];
            float* dst = &f_[size_t(y) * size_t(w_) + size_t(x0)];
            for (int x = 0; x <= x1 - x0; ++x) dst[x] += row[x];
        }
        const int cx0 = std::max(0, cx - colorR), cx1 = std::min(w_ - 1, cx + colorR);
        const int cy0 = std::max(0, cy - colorR), cy1 = std::min(h_ - 1, cy + colorR);
        for (int y = cy0; y <= cy1; ++y) {
            const float* row = &kc[size_t(y - cy + colorR) * size_t(nc) + size_t(cx0 - cx + colorR)];
            const size_t base = size_t(y) * size_t(w_) + size_t(cx0);
            for (int x = 0; x <= cx1 - cx0; ++x) {
                const float wt = row[x];
                if (wt <= 0.f) continue;
                const size_t i = base + size_t(x);
                if (wt > w1_[i]) {
                    if (c1_[i] != col) {
                        w2_[i] = w1_[i];
                        c2_[i] = c1_[i];
                    }
                    w1_[i] = wt;
                    c1_[i] = col;
                } else if (c1_[i] != col && wt > w2_[i]) {
                    w2_[i] = wt;
                    c2_[i] = col;
                }
            }
        }
    }
    if (maxY < 0) return;
    const uint32_t ink = pack(theme.ink);
    const float T = iso;
    auto inside = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < w_ && y < h_ && f_[size_t(y) * size_t(w_) + size_t(x)] >= T;
    };
    for (int y = minY; y <= maxY; ++y)
        for (int x = minX; x <= maxX; ++x) {
            const size_t i = size_t(y) * size_t(w_) + size_t(x);
            if (f_[i] < T) {
                if (!(w1_[i] > 0.f && inside(x - 1, y) && inside(x + 1, y) && inside(x, y - 1) && inside(x, y + 1))) continue;
            }
            if (w1_[i] <= 0.f) continue;
            uint32_t col = c1_[i];
            if (w2_[i] > 0.f) {
                const float t = w2_[i] / (w1_[i] + w2_[i]);
                if (t > bayer(x, y)) col = c2_[i];
            }
            const bool edge = !inside(x - 1, y) || !inside(x + 1, y) || !inside(x, y - 1) || !inside(x, y + 1);
            px_[i] = edge ? mixPacked(col, ink, vis::kOutlineMix) : col;
        }
}

void BlobRaster::drawLabels(const std::vector<BlobSprite>& sprites, const Theme& theme) {
    auto block = [&](int x, int y, int n, uint32_t c) {
        for (int yy = y; yy < y + n; ++yy)
            for (int xx = x; xx < x + n; ++xx)
                if (xx >= 0 && yy >= 0 && xx < w_ && yy < h_) px_[size_t(yy) * size_t(w_) + size_t(xx)] = c;
    };
    for (const BlobSprite& s : sprites) {
        const std::string text = std::to_string(1 << std::min(std::max(s.level, 0), 11));
        const float avail = std::min(s.ra, s.rb) * 1.55f;
        FontFace face = FontFace::Digits3x5;
        int scale = 1;
        if (PixelFont::width(FontFace::Text5x7, text) <= avail && std::min(s.ra, s.rb) >= 10.5f) {
            face = FontFace::Text5x7;
            if (PixelFont::width(face, text) * 2 <= avail) scale = 2;
        } else if (PixelFont::width(FontFace::Digits3x5, text) > avail + 1.f) {
            continue;  // too small to label legibly
        }
        const int tw = PixelFont::width(face, text) * scale, th = PixelFont::height(face) * scale;
        const int ox = int(std::floor(s.x - float(tw) * 0.5f + 0.5f)), oy = int(std::floor(s.y - float(th) * 0.5f + 0.5f));
        // Flat purple numerals straight on the fill (no outline, no shadow).
        const uint32_t fg = pack(theme.label);
        PixelFont::forEachRun(face, text, [&](int x, int y, int len) {
            for (int dy = 0; dy < scale; ++dy)
                for (int dx = 0; dx < len * scale; ++dx) block(ox + x * scale + dx, oy + y * scale + dy, 1, fg);
        });
    }
}

}  // namespace liq
