// GameScene effects and the pot itself: sparks, rings, bubbles, guide line and the liquid raster.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "scenes/game/GameScene.hpp"

namespace liq {

namespace {
constexpr float kParticleDraw = 0.f;    // unused by the table rasterizer
constexpr int kFieldR = 9, kColorR = 6;  // kernel radii (px) of one liquid particle
constexpr float kParticleIso = 1.1f;    // surface level of the summed particle field
}  // namespace

void GameScene::updateFx(float dt) {
    for (auto& p : parts_) {
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.vy += 260.f * dt;
        p.life -= dt;
    }
    parts_.erase(std::remove_if(parts_.begin(), parts_.end(), [](const FxSpark& p) { return p.life <= 0; }), parts_.end());
    for (auto& f : texts_) {
        f.y -= 18.f * dt;
        f.life -= dt;
    }
    texts_.erase(std::remove_if(texts_.begin(), texts_.end(), [](const FloatText& f) { return f.life <= 0; }), texts_.end());
    for (auto& r : rings_) r.age += dt;
    rings_.erase(std::remove_if(rings_.begin(), rings_.end(), [](const PopRing& r) { return r.age > 0.38f; }), rings_.end());
    if (chainT_ > 0) chainT_ -= dt;
    if (toastT_ > 0) toastT_ -= dt;
}

void GameScene::drawFx(Ctx& c) {
    Gfx& g = c.gfx;
    const bool blink = !c.save.calm;
    for (const auto& r : rings_) {
        const float p = r.age / 0.38f;
        if (blink && p > 0.7f && (int(r.age * 60.f) & 1)) continue;  // blink out instead of alpha fading
        g.ring(r.x, r.y, int(r.r0 + p * 12.f), mix(r.c, rgb(0xffffff), 0.5f * (1.f - p)));
    }
    for (const auto& p : parts_) {
        if (blink && p.life < 0.15f && (int(p.life * 90.f) & 1)) continue;
        g.rect(std::floor(p.x), std::floor(p.y), float(p.size), float(p.size), p.c);
    }
    for (const auto& f : texts_)
        if (f.life > 0.25f || !blink || (int(f.life * 40.f) & 1))
            g.number(f.s, std::floor(f.x), std::floor(f.y), f.scale, f.c, Align::Center, TextFx::Outline, c.theme->ink);
    if (chainT_ > 0.f && chainShown_ >= 2 && (chainT_ > 0.3f || !blink || (int(chainT_ * 30.f) & 1))) {
        const float bob = std::floor(std::sin((1.1f - chainT_) * 14.f) * 2.f * chainT_);
        g.text(c.loc.tr("hud.chain") + " x" + std::to_string(chainShown_), cfg::kW / 2.f, 170.f + bob, 2,
               c.theme->accent, Align::Center, TextFx::Outline, c.theme->ink);
    }
}

float GameScene::landingY(float x, int level) const {
    const float r = World::radiusFor(level);
    return std::min(float(cfg::kFloor), world_.surfaceY(x, r * 0.7f)) - r * 0.55f;
}

// Slow rising bubbles inside the tank for liquid ambience.
void GameScene::drawBubbles(Ctx& c) {
    if (c.save.reduceMotion) return;
    const Theme& t = *c.theme;
    for (int i = 0; i < 9; ++i) {
        const float sp = 7.f + float((i * 5) % 7), ph = float(i) * 37.1f;
        const float y = cfg::kFloor - std::fmod(c.time * sp + ph * 3.f, cfg::kFloor - float(cfg::kTankTop) - 4.f);
        const float x = cfg::kFieldL + 12.f + std::fmod(float(i) * 61.7f, cfg::kFieldR - cfg::kFieldL - 24.f) +
                        std::sin(c.time * 0.8f + ph) * 3.f;
        const Color col = mix(t.tank, t.dim, 0.35f);
        if (i % 3 == 0) c.gfx.ring(x, y, 2, col);
        else c.gfx.rect(std::floor(x), std::floor(y), 2, 2, col);
    }
}

void GameScene::drawGuide(Ctx& c) {
    if (st_ == St::Over || !c.save.guide || mod_ == kModFog) return;
    const int lim = mode_->dropLimit();
    if (lim >= 0 && drops_ >= lim) return;
    Gfx& g = c.gfx;
    const Theme& t = *c.theme;
    const float r = World::radiusFor(cur_), ly = landingY(holdX_, cur_);
    for (float y = cfg::kSpawnY + r + 3.f; y < ly - r - 2.f; y += 5.f) g.rect(std::floor(holdX_), std::floor(y), 1, 2, mix(t.dim, t.tank, 0.25f));
    g.ring(holdX_, ly, int(r), mix(t.dim, t.tank, 0.2f), 2);
}

void GameScene::drawBlobs(Ctx& c, int sx, int sy) {
    const float alpha = clampv(acc_ / cfg::kStep, 0.f, 1.f);
    const bool spec = viewing();
    const World& W = spec ? opp_->world() : world_;
    std::vector<BlobSprite> pile, labels;
    W.particleSprites(spec ? opp_->viewAlpha() : (st_ == St::Playing ? alpha : 1.f), kParticleDraw, pile);
    W.labelSprites(labels);
    const bool blind = mid_ == ModeId::Blind && !spec && peekT_ <= 0.f;
    if (blind) {
        for (auto& s : pile) s.level = kBlindLevel;
        labels.clear();
    }
    for (auto& s : pile) {
        s.x += float(sx);
        s.y += float(sy);
    }
    for (auto& s : labels) {
        s.x += float(sx);
        s.y += float(sy);
    }
    c.raster.clear();
    c.raster.drawLiquid(pile, *c.theme, kFieldR, kColorR, kParticleIso);
    c.raster.drawLabels(labels, *c.theme);

    // The blob you are about to drop (its own group, so it never fuses with the pile).
    const int lim = mode_->dropLimit();
    if (spec) {  // the opponent's hovering drop
        std::vector<BlobSprite> held{plainSprite(opp_->aimX() + float(sx), cfg::kSpawnY + float(sy), opp_->heldLevel())};
        c.raster.drawField(held, *c.theme);
        c.raster.drawLabels(held, *c.theme);
    } else if (st_ != St::Over && (lim < 0 || drops_ < lim)) {
        const float br = 1.f + vis::kBreath * std::sin(c.time * 6.2831853f * vis::kBreathHz);
        // After a release the next drop swells up from nothing while the cooldown runs out.
        float fill = clampv(1.f - std::max(0.f, cooldown_) / cfg::kDropCooldown, 0.f, 1.f);
        if (!mode_->manualDrop()) fill = std::min(fill, std::max(0.15f, autoFill_));
        const float grow = fill * fill * (3.f - 2.f * fill);
        std::vector<BlobSprite> held{popped(plainSprite(holdX_ + float(sx), cfg::kSpawnY + float(sy), cur_), holdPop_.x + br - 1.f)};
        held[0].ra *= grow;
        held[0].rb *= grow;
        if (held[0].ra < 2.f) held.clear();
        c.raster.drawField(held, *c.theme);
        c.raster.drawLabels(held, *c.theme);
    }
    if (spec) {
        c.gfx.raster(c.raster);
        return;
    }
    // Next-blob preview inside its HUD panel (unshaken).
    if (mod_ != kModFog) {
        std::vector<BlobSprite> nxt{popped(plainSprite(c.ui.mx(8, 44) + 22.f, 36.f, mid_ == ModeId::Blind ? kBlindLevel : next_), nextPop_.x)};
        nxt[0].ra = nxt[0].rb = std::min(nxt[0].ra, 13.f);
        c.raster.drawField(nxt, *c.theme);
        if (mid_ != ModeId::Blind) c.raster.drawLabels(nxt, *c.theme);
    }
    c.gfx.raster(c.raster);
    if (blind) {  // hidden values: a question mark on every drop, bobbing a little
        std::vector<BlobSprite> ql;
        W.labelSprites(ql);
        for (const auto& q : ql) {
            const int sc = q.ra > 15.f ? 2 : 1;
            c.gfx.text("?", q.x + float(sx), q.y + float(sy) - 3.f + std::sin(c.time * 2.f + q.x * 0.1f), sc, withAlpha(c.theme->text, 190), Align::Center);
        }
    }
    if (mid_ == ModeId::Blind && !spec) {
        if (peekT_ > 0.f) peekT_ -= 1.f / 60.f;
        const float by = 36.f;
        if (c.ui.button(c.loc.fmt("blind.peek", std::to_string(peeks_)), c.ui.mx(210, 56), by, 56, 20) && peeks_ > 0 && peekT_ <= 0.f && st_ == St::Playing) {
            --peeks_;
            peekT_ = 2.f;
        }
    }
}

}  // namespace liq
