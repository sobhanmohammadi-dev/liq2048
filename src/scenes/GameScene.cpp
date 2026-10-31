// The in-game scene: shared run loop for all modes (mode rules come from Mode).
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "core/Util.hpp"
#include "game/Mode.hpp"
#include "game/World.hpp"
#include "scenes/Scenes.hpp"

namespace liq {

namespace {

struct Particle {
    float x, y, vx, vy, life, max;
    Color c;
    int size;
};
struct FloatText {
    float x, y, life;
    std::string s;
    Color c;
    int scale;
};

class GameScene final : public Scene {
public:
    explicit GameScene(Ctx& c) : mid_(c.mode) {
        today_ = platform::dayNumber();
        if (mid_ == ModeId::Daily && c.save.dailyDay != today_) {
            c.save.dailyDay = today_;
            c.save.dailyToday = 0;
        }
        st_ = c.save.tutorialDone ? St::Playing : St::Tutorial;
        start(c);
    }

    void frame(Ctx& c, float dt) override {
        Gfx& g = c.gfx;
        g.clear(c.theme->bg);
        drawStars(c);
        if (st_ == St::Playing) simulate(c, std::min(dt, 0.05f));
        updateFx(dt);
        drawField(c);
        drawFx(c);
        drawHud(c);
        switch (st_) {
            case St::Tutorial: tutorial(c); break;
            case St::Paused: pauseMenu(c); break;
            case St::Settings:
                dim(c, 220);
                if (drawSettings(c, 90) || c.in.esc) st_ = St::Paused;
                break;
            case St::Over: overlay(c); break;
            default: break;
        }
    }

private:
    enum class St { Tutorial, Playing, Paused, Settings, Over };

    // ----- run state -----
    ModeId mid_;
    std::unique_ptr<Mode> mode_;
    World world_;
    Rng rng_;
    St st_ = St::Playing;
    Outcome outcome_ = Outcome::Running;
    int today_ = 0;
    int score_ = 0, drops_ = 0, cur_ = 0, next_ = 0, maxLevel_ = 0, tutPage_ = 0;
    float aimX_ = (cfg::kFieldL + cfg::kFieldR) / 2, cooldown_ = 0, quiet_ = 0, elapsed_ = 0, acc_ = 0;
    bool aiming_ = false, committed_ = false, newBest_ = false;
    std::vector<Particle> parts_;
    std::vector<FloatText> texts_;

    int roll() {
        int r = rng_.range(0, 99);
        return r < 40 ? 0 : r < 70 ? 1 : r < 90 ? 2 : 3;
    }

    void start(Ctx& c) {
        mode_ = makeMode(mid_, c.save, today_);
        world_.clear();
        parts_.clear();
        texts_.clear();
        uint32_t seed = mode_->seed();
        rng_.seed(seed ? seed : uint32_t(SDL_GetTicks()) * 2246822519u + 7u);
        score_ = drops_ = maxLevel_ = 0;
        cooldown_ = quiet_ = elapsed_ = acc_ = 0;
        aiming_ = committed_ = newBest_ = false;
        outcome_ = Outcome::Running;
        cur_ = roll();
        next_ = roll();
        aimX_ = (cfg::kFieldL + cfg::kFieldR) / 2;
    }

    Stats stats() const {
        Stats s;
        s.score = score_;
        s.highest = maxLevel_;
        s.drops = drops_;
        int lim = mode_->dropLimit();
        s.dropsLeft = lim < 0 ? -1 : std::max(0, lim - drops_);
        s.elapsed = elapsed_;
        s.quiet = quiet_;
        s.overflow = world_.overflow() > cfg::kOverflowSeconds;
        return s;
    }

    int bestFor(const Ctx& c) const {
        return mid_ == ModeId::Daily ? c.save.dailyToday : c.save.best[int(mid_)];
    }

    // ----- simulation -----
    void tryDrop(Ctx& c) {
        int lim = mode_->dropLimit();
        if (cooldown_ > 0 || (lim >= 0 && drops_ >= lim)) return;
        world_.spawn(aimX_, cfg::kSpawnY, cur_);
        ++drops_;
        cur_ = next_;
        next_ = roll();
        cooldown_ = cfg::kDropCooldown;
        quiet_ = 0;
        c.audio.play(Sfx::Drop);
    }

    void grant(Ctx& c, int id) {
        if (c.save.unlock(id)) {
            texts_.push_back({180, 220, 2.2f, c.loc.tr("hud.achievement") + " " + c.loc.tr(achKey(id)), c.theme->accent, 1});
            c.audio.play(Sfx::Success);
        }
    }

    void onMerge(Ctx& c, const MergeEvent& e) {
        const Theme& t = *c.theme;
        int value = 1 << e.level;
        score_ += value * e.chain;
        quiet_ = 0;
        maxLevel_ = std::max(maxLevel_, e.level);
        c.audio.play(e.chain >= 2 ? Sfx::Chain : Sfx::Merge);
        Color col = t.blob[std::min(e.level, 11)];
        int n = 12 + e.level * 2;
        for (int i = 0; i < n; ++i) {
            float a = rng2_.uni(0.f, 6.2832f), sp = rng2_.uni(40.f, 180.f);
            parts_.push_back({e.x, e.y, std::cos(a) * sp, std::sin(a) * sp, 0.5f, 0.5f, col, 2 + int(rng2_.uni(0, 2))});
        }
        texts_.push_back({e.x, e.y - World::radiusFor(e.level), 0.8f, "+" + std::to_string(value * e.chain), t.text, 1});
        if (e.chain >= 2)
            texts_.push_back({180, 118, 1.0f, c.loc.tr("hud.chain") + " x" + std::to_string(e.chain), t.accent, 2});
        if (e.level >= 7 && e.level <= 11) grant(c, kFirst128 + (e.level - 7));
        if (e.chain >= 3) grant(c, kChain3);
        if (e.chain >= 5) grant(c, kChain5);
        if (score_ >= 1000) grant(c, kScore1k);
        if (score_ >= 5000) grant(c, kScore5k);
    }

    void simulate(Ctx& c, float dt) {
        Input& in = c.in;
        if (in.esc) {
            st_ = St::Paused;
            c.ui.resetFocus();
            return;
        }
        // Aiming: drag with mouse/finger, release to drop. Keys: arrows + space/down.
        float rad = World::radiusFor(cur_);
        if (in.pressed && in.my > 100) aiming_ = true;
        if (aiming_ && (in.down || in.released)) aimX_ = in.mx;
        if (in.left) aimX_ -= cfg::kAimSpeed * dt;
        if (in.right) aimX_ += cfg::kAimSpeed * dt;
        aimX_ = clampv(aimX_, cfg::kFieldL + rad, cfg::kFieldR - rad);
        bool want = false;
        if (aiming_ && in.released) {
            want = true;
            aiming_ = false;
        }
        if (in.drop) want = true;
        cooldown_ -= dt;
        elapsed_ += dt;
        quiet_ += dt;
        mode_->update(dt);
        if (want) tryDrop(c);

        acc_ += dt;
        while (acc_ >= cfg::kStep) {
            world_.step(cfg::kStep, cfg::kGravity * mode_->gravityScale());
            acc_ -= cfg::kStep;
            for (const auto& e : world_.events) onMerge(c, e);
            world_.events.clear();
        }
        world_.updateOverflow(cfg::kDeadY, dt);

        if (!mode_->failable() && world_.overflow() > 1.5f) popOverflow(c);

        Outcome o = mode_->evaluate(stats());
        if (o != Outcome::Running) finish(c, o);
    }

    // Zen has no fail state: blobs stuck above the line pop harmlessly.
    void popOverflow(Ctx& c) {
        const Theme& t = *c.theme;
        for (auto& b : world_.blobs)
            if (b.y - b.r < cfg::kDeadY && b.age > 1.2f) {
                b.dead = true;
                for (int i = 0; i < 10; ++i)
                    parts_.push_back({b.x, b.y, rng2_.uni(-120.f, 120.f), rng2_.uni(-120.f, 120.f), 0.5f, 0.5f,
                                      t.blob[std::min(b.level, 11)], 3});
            }
        world_.blobs.erase(std::remove_if(world_.blobs.begin(), world_.blobs.end(), [](const Blob& b) { return b.dead; }),
                           world_.blobs.end());
        world_.resetOverflow();
    }

    void commit(Ctx& c) {
        if (committed_) return;
        committed_ = true;
        int& best = c.save.best[int(mid_)];
        newBest_ = score_ > best;
        if (newBest_) best = score_;
        if (mid_ == ModeId::Daily && score_ > c.save.dailyToday) c.save.dailyToday = score_;
        c.save.store();
    }

    void finish(Ctx& c, Outcome o) {
        outcome_ = o;
        st_ = St::Over;
        if (o == Outcome::Won && mid_ == ModeId::Target) c.save.targetStage = std::min(c.save.targetStage + 1, 2);
        commit(c);
        c.audio.play(o == Outcome::Lost ? Sfx::Fail : Sfx::Success);
    }

    // ----- effects -----
    Rng rng2_;
    void updateFx(float dt) {
        for (auto& p : parts_) {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.vy += 300.f * dt;
            p.life -= dt;
        }
        parts_.erase(std::remove_if(parts_.begin(), parts_.end(), [](const Particle& p) { return p.life <= 0; }), parts_.end());
        for (auto& f : texts_) {
            f.y -= 25.f * dt;
            f.life -= dt;
        }
        texts_.erase(std::remove_if(texts_.begin(), texts_.end(), [](const FloatText& f) { return f.life <= 0; }), texts_.end());
    }

    void drawFx(Ctx& c) {
        for (const auto& p : parts_)
            c.gfx.rect(p.x, p.y, float(p.size), float(p.size), withAlpha(p.c, Uint8(255 * std::max(0.f, p.life / p.max))));
        for (const auto& f : texts_)
            c.gfx.text(f.s, f.x, f.y, f.scale, withAlpha(f.c, Uint8(255 * std::min(1.f, f.life * 2.f))), Align::Center);
    }

    // ----- drawing -----
    static void drawBlob(Ctx& c, float x, float y, float r, int level, float squash, float pop) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        r *= 1.f + 0.15f * pop;
        float rx = r * (1.f + 0.12f * squash), ry = r * (1.f - 0.12f * squash);
        y += r - ry;  // keep the bottom planted while squashing
        Color col = t.blob[std::min(level, 11)];
        g.ellipse(x, y, rx + 2, ry + 2, mix(col, rgb(0x000000), 0.6f));
        g.ellipse(x, y, rx, ry, col);
        g.rect(x - rx * 0.55f, y - ry * 0.55f, std::max(2.f, r * 0.22f), 2.f, mix(col, rgb(0xffffff), 0.55f));
        std::string v = std::to_string(1 << level);
        int sc = clampv(int(r * 1.5f / (6.f * float(v.size()))), 1, 5);
        Color tc = luminance(col) > 140 ? rgb(0x1a1a2e) : rgb(0xffffff);
        g.bitmap(v, x, y, sc, tc, Align::Center);
    }

    float landingY(float x, int level) const {
        float r = World::radiusFor(level), best = cfg::kFloor - r;
        for (const auto& b : world_.blobs) {
            float dx = std::fabs(b.x - x), m = r + b.r;
            if (dx < m) best = std::min(best, b.y - std::sqrt(m * m - dx * dx));
        }
        return best;
    }

    void drawField(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        // Frame: flat 4px walls.
        g.rect(cfg::kFieldL - 4, 124, 4, cfg::kFloor - 120, t.frame);
        g.rect(cfg::kFieldR, 124, 4, cfg::kFloor - 120, t.frame);
        g.rect(cfg::kFieldL - 4, cfg::kFloor, cfg::kFieldR - cfg::kFieldL + 8, 4, t.frame);
        // Dead line (blinks while overflowing).
        bool warn = world_.overflow() > 0.2f && (SDL_GetTicks() / 150) % 2 == 0;
        for (float x = cfg::kFieldL; x < cfg::kFieldR; x += 12)
            g.rect(x, cfg::kDeadY, 6, 2, warn ? t.danger : withAlpha(t.dim, 200));

        for (const auto& b : world_.blobs) drawBlob(c, b.x, b.y, b.r, b.level, b.squash, b.pop);

        if (st_ == St::Playing || st_ == St::Paused || st_ == St::Tutorial) {
            int lim = mode_->dropLimit();
            if (lim < 0 || drops_ < lim) {
                float ly = landingY(aimX_, cur_);
                for (float y = cfg::kSpawnY + 24; y < ly; y += 10) g.rect(aimX_, y, 2, 5, withAlpha(t.dim, 220));
                float r = World::radiusFor(cur_);
                g.ellipse(aimX_, ly, r, r, withAlpha(t.text, 40));
                if (cooldown_ <= 0) drawBlob(c, aimX_, cfg::kSpawnY, r, cur_, 0, 0);
                else g.ellipse(aimX_, cfg::kSpawnY, r, r, withAlpha(t.dim, 90));
            }
        }
        // Value history strip along the bottom: dots reached are lit.
        for (int i = 0; i < 12; ++i) {
            float x = 180.f + (i - 5.5f) * 26.f;
            bool got = i <= maxLevel_ && maxLevel_ > 0 ? true : i <= 3;
            g.ellipse(x, 620, 7, 7, got ? t.blob[i] : mix(t.panel, t.bg, 0.4f));
        }
        if (maxLevel_ > 0) {
            float x = 180.f + (maxLevel_ - 5.5f) * 26.f;
            g.rect(x - 1, 631, 3, 3, t.text);
        }
    }

    void drawHud(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Ui& ui = c.ui;
        Loc& L = c.loc;
        const bool play = st_ == St::Playing;

        // Next preview.
        float nx = ui.mx(10, 56);
        ui.panel(nx, 10, 56, 56);
        g.text(L.tr("hud.next"), nx + 28, 22, 1, t.dim, Align::Center);
        float pr = 11.f;
        Color col = t.blob[next_];
        g.ellipse(nx + 28, 46, pr + 2, pr + 2, mix(col, rgb(0), 0.6f));
        g.ellipse(nx + 28, 46, pr, pr, col);
        std::string nv = std::to_string(1 << next_);
        g.bitmap(nv, nx + 28, 46, 1, luminance(col) > 140 ? rgb(0x1a1a2e) : rgb(0xffffff), Align::Center);

        // Score + best.
        g.bitmap(std::to_string(score_), 180, 30, 4, t.text, Align::Center);
        g.text(L.tr("hud.best") + " " + std::to_string(std::max(bestFor(c), score_)), 180, 62, 1, t.dim, Align::Center);

        // Pause button (two bars) + language switcher.
        float px = ui.mx(306, 44);
        if (ui.button("", px, 10, 44, 28, play, false)) {
            st_ = St::Paused;
            ui.resetFocus();
        }
        g.rect(px + 15, 17, 5, 14, play ? t.text : t.dim);
        g.rect(px + 25, 17, 5, 14, play ? t.text : t.dim);
        if (play) langToggle(c, 310, 44);

        // Mode indicator + status + progress bar.
        Stats s = stats();
        g.text(L.tr(mode_->nameKey()), 180, 82, 2, t.accent, Align::Center);
        std::string status = mode_->status(L, s);
        if (!status.empty()) g.text(status, 180, 102, 1, t.text, Align::Center);
        float pg = mode_->progress(s);
        if (pg >= 0) {
            g.rect(80, 112, 200, 6, t.panel);
            g.rect(80, 112, 200.f * clampv(pg, 0.f, 1.f), 6, t.accent);
        }
    }

    void dim(Ctx& c, Uint8 a) { c.gfx.rect(0, 0, cfg::kW, cfg::kH, withAlpha(c.theme->bg, a)); }

    void tutorial(Ctx& c) {
        Gfx& g = c.gfx;
        Ui& ui = c.ui;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        dim(c, 225);
        ui.panel(30, 150, 300, 300);
        g.text(L.tr("tut.title"), 180, 180, 2, t.accent, Align::Center);
        static const char* keys[3] = {"tut.1", "tut.2", "tut.3"};
        ui.paragraph(L.tr(keys[tutPage_]), 48, 225, 264, 2, t.text, ui.start() == Align::Right ? Align::Right : Align::Left);
        for (int i = 0; i < 3; ++i) g.rect(165.f + i * 12, 392, 6, 6, i == tutPage_ ? t.accent : t.dim);
        bool last = tutPage_ == 2;
        if (ui.button(L.tr(last ? "tut.start" : "tut.next"), ui.mx(190, 120), 408, 120, 30)) {
            if (last) endTutorial(c);
            else ++tutPage_;
        }
        if (ui.button(L.tr("tut.skip"), ui.mx(50, 120), 408, 120, 30) || c.in.esc) endTutorial(c);
    }

    void endTutorial(Ctx& c) {
        c.save.tutorialDone = true;
        c.save.store();
        st_ = St::Playing;
        c.in.esc = false;
    }

    void pauseMenu(Ctx& c) {
        Gfx& g = c.gfx;
        Ui& ui = c.ui;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        dim(c, 220);
        ui.panel(60, 190, 240, 270);
        g.text(L.tr("pause.title"), 180, 220, 3, t.accent, Align::Center);
        if (ui.button(L.tr("pause.resume"), 80, 250, 200, 40) || c.in.esc) st_ = St::Playing;
        if (ui.button(L.tr("pause.restart"), 80, 298, 200, 40)) {
            commit(c);
            start(c);
            st_ = St::Playing;
        }
        if (ui.button(L.tr("pause.settings"), 80, 346, 200, 40)) st_ = St::Settings;
        if (ui.button(L.tr("pause.quit"), 80, 394, 200, 40)) {
            commit(c);
            c.go(SceneId::Menu);
        }
    }

    void overlay(Ctx& c) {
        Gfx& g = c.gfx;
        Ui& ui = c.ui;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        dim(c, 200);
        ui.panel(40, 170, 280, 300);
        const char* key = outcome_ == Outcome::Won ? "over.won" : outcome_ == Outcome::Lost ? "over.lost" : "over.finished";
        g.text(L.tr(key), 180, 205, 3, outcome_ == Outcome::Lost ? t.danger : t.accent, Align::Center);
        g.text(L.tr("over.score"), 180, 250, 2, t.dim, Align::Center);
        g.bitmap(std::to_string(score_), 180, 282, 5, t.text, Align::Center);
        g.text(L.tr("hud.best") + " " + std::to_string(bestFor(c)), 180, 322, 2, t.dim, Align::Center);
        if (newBest_) g.text(L.tr("over.newbest"), 180, 350, 2, t.accent, Align::Center);
        if (ui.button(L.tr("over.restart"), 70, 380, 220, 36)) {
            start(c);
            st_ = St::Playing;
        }
        if (ui.button(L.tr("over.menu"), 70, 424, 220, 36) || c.in.esc) c.go(SceneId::Menu);
    }
};

}  // namespace

std::unique_ptr<Scene> makeGameScene(Ctx& c) { return std::make_unique<GameScene>(c); }

}  // namespace liq
