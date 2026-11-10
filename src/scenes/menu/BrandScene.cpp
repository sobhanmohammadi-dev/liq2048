// Dev-only scene used by `liq2048 --screenshot out.bmp --scene logo|banner|social` to render the
// repository's logo and banner with the real game renderer (see tools/branding/make_branding.py).

#define _CRT_SECURE_NO_WARNINGS

#include <cmath>

#include "game/World.hpp"
#include "scenes/Scenes.hpp"

namespace liq {

namespace {

BlobSprite sized(float x, float y, int level, float r, float wob = 0.f, float ang = 0.f) {
    BlobSprite s = plainSprite(x, y, level, wob, ang);
    const float k = r / World::radiusFor(level);
    s.ra *= k;
    s.rb *= k;
    return s;
}

class BrandScene final : public Scene {
public:
    void frame(Ctx& c, float) override {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        const std::string& v = c.brandVariant;
        g.drawBackdrop(t, 6.f);
        std::vector<BlobSprite> sp;
        if (v == "logo") {
            // 128x128 tile: a big gold 2048 droplet fused with two small ones.
            g.rect(0, 0, 128, 128, t.sky[1]);
            g.drawBackdrop(t, 6.f);
            g.rect(0, 0, 128, 128, withAlpha(t.sky[1], 120));
            sp = {sized(64, 70, 11, 40, 0.05f, 0.5f), sized(30, 30, 0, 16, 0.08f, 0.8f), sized(102, 36, 4, 15, 0.06f, 2.2f)};
            c.raster.clear();
            c.raster.drawField(sp, t);
            c.raster.drawLabels(sp, t);
            g.raster(c.raster);
            g.border(0, 0, 128, 128, 3, t.frame);
            g.border(3, 3, 122, 122, 1, t.ink);
            g.border(0, 0, 128, 128, 1, t.ink);
            return;
        }
        const bool social = v == "social";
        const float oy = social ? 18.f : 0.f;
        sp = {sized(15, 64 + oy, 2, 13, 0.06f, 0.4f),   sized(20, 40 + oy, 1, 10, 0.05f, 1.4f),  sized(10, 18 + oy, 0, 8, 0.1f, 2.0f),
              sized(253, 66 + oy, 8, 17, 0.05f, 0.3f), sized(250, 44 + oy, 5, 10, 0.07f, 1.0f), sized(260, 24 + oy, 3, 8, 0.06f, 2.2f)};
        c.raster.clear();
        c.raster.drawField(sp, t);
        c.raster.drawLabels(sp, t);
        g.raster(c.raster);
        drawWordmark(c, cfg::kW / 2.f, 34.f + oy, 5);
        g.text(c.loc.tr("menu.tagline"), cfg::kW / 2.f, 70.f + oy, 1, t.text, Align::Center);
        if (social) g.number("FREE - OPEN SOURCE - NO ADS", cfg::kW / 2.f, 88.f + oy, 1, t.dim, Align::Center);
    }
};

}  // namespace

std::unique_ptr<Scene> makeBrandScene() { return std::make_unique<BrandScene>(); }

}  // namespace liq
