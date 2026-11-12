// Tilt maze (phones): a small droplet rolls through a maze towards another droplet; tilt the phone to steer, shake it
// to see the way. Without a motion sensor (or with the mouse) dragging a finger steers instead.
//
// Everything here is local to this scene: a tiny circle-vs-wall simulation, no liquid physics.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "core/Util.hpp"
#include "scenes/Scenes.hpp"
#include "ui/widgets/Widgets.hpp"

namespace liq {

namespace {

constexpr float kWall = 3.f;       // wall thickness (px)
constexpr float kTopY = 84.f;      // top of the maze area
constexpr float kAreaW = 250.f, kAreaH = 352.f;

struct Rect {
    float x0, y0, x1, y1;
};

class Maze {
public:
    int cols = 5, rows = 7;
    float cell = 40.f, ox = 10.f, oy = kTopY;
    std::vector<Rect> walls;
    int goalC = 0, goalR = 0;

    // Perfect maze by randomised depth-first search; the goal is the cell farthest from the start (0,0).
    void build(int level, uint32_t seed) {
        cols = 5 + std::min(level, 4);
        rows = cols * 3 / 2;
        cell = std::floor(std::min(kAreaW / float(cols), kAreaH / float(rows)));
        ox = std::floor((float(cfg::kW) - cell * float(cols)) / 2.f);
        oy = kTopY + std::floor((kAreaH - cell * float(rows)) / 2.f);
        Rng rng;
        rng.seed(seed * 2654435761u + uint32_t(level) * 40503u + 17u);
        // open_[i] bit 0 = east open, bit 1 = south open
        open_.assign(size_t(cols * rows), 0);
        std::vector<char> seen(size_t(cols * rows), 0);
        std::vector<int> stack{0};
        seen[0] = 1;
        while (!stack.empty()) {
            const int cur = stack.back();
            const int cx = cur % cols, cy = cur / cols;
            int cand[4], n = 0;
            if (cx + 1 < cols && !seen[size_t(cur + 1)]) cand[n++] = 0;
            if (cy + 1 < rows && !seen[size_t(cur + cols)]) cand[n++] = 1;
            if (cx > 0 && !seen[size_t(cur - 1)]) cand[n++] = 2;
            if (cy > 0 && !seen[size_t(cur - cols)]) cand[n++] = 3;
            if (n == 0) {
                stack.pop_back();
                continue;
            }
            const int d = cand[rng.range(0, n - 1)];
            int nxt = cur;
            if (d == 0) open_[size_t(cur)] |= 1, nxt = cur + 1;
            else if (d == 1) open_[size_t(cur)] |= 2, nxt = cur + cols;
            else if (d == 2) open_[size_t(cur - 1)] |= 1, nxt = cur - 1;
            else open_[size_t(cur - cols)] |= 2, nxt = cur - cols;
            seen[size_t(nxt)] = 1;
            stack.push_back(nxt);
        }
        // farthest cell by BFS
        const std::vector<int> dist = bfs(0);
        int best = 0;
        for (int i = 0; i < cols * rows; ++i)
            if (dist[size_t(i)] > dist[size_t(best)]) best = i;
        goalC = best % cols;
        goalR = best / cols;
        // wall rectangles
        walls.clear();
        const float hw = kWall / 2.f;
        walls.push_back({ox - hw, oy - hw, ox + cell * float(cols) + hw, oy + hw});
        walls.push_back({ox - hw, oy + cell * float(rows) - hw, ox + cell * float(cols) + hw, oy + cell * float(rows) + hw});
        walls.push_back({ox - hw, oy - hw, ox + hw, oy + cell * float(rows) + hw});
        walls.push_back({ox + cell * float(cols) - hw, oy - hw, ox + cell * float(cols) + hw, oy + cell * float(rows) + hw});
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c) {
                const float x = ox + cell * float(c), y = oy + cell * float(r);
                if (c + 1 < cols && !(open_[size_t(r * cols + c)] & 1)) walls.push_back({x + cell - hw, y - hw, x + cell + hw, y + cell + hw});
                if (r + 1 < rows && !(open_[size_t(r * cols + c)] & 2)) walls.push_back({x - hw, y + cell - hw, x + cell + hw, y + cell + hw});
            }
    }

    float cx(int c) const { return ox + cell * (float(c) + 0.5f); }
    float cy(int r) const { return oy + cell * (float(r) + 0.5f); }
    int cellOf(float x, float y) const {
        const int c = std::max(0, std::min(cols - 1, int((x - ox) / cell))), r = std::max(0, std::min(rows - 1, int((y - oy) / cell)));
        return r * cols + c;
    }

    // Cells from `from` to the goal (inclusive), following the maze passages.
    std::vector<int> path(int from) const {
        const int goal = goalR * cols + goalC;
        std::vector<int> prev(size_t(cols * rows), -2), q{from};
        prev[size_t(from)] = -1;
        for (size_t h = 0; h < q.size(); ++h) {
            const int cur = q[h];
            if (cur == goal) break;
            for (int nb : neighbours(cur))
                if (prev[size_t(nb)] == -2) {
                    prev[size_t(nb)] = cur;
                    q.push_back(nb);
                }
        }
        std::vector<int> out;
        for (int v = goal; v >= 0; v = prev[size_t(v)]) out.push_back(v);
        std::reverse(out.begin(), out.end());
        return out;
    }

private:
    std::vector<unsigned char> open_;

    std::vector<int> neighbours(int cur) const {
        std::vector<int> n;
        const int c = cur % cols, r = cur / cols;
        if (c + 1 < cols && (open_[size_t(cur)] & 1)) n.push_back(cur + 1);
        if (r + 1 < rows && (open_[size_t(cur)] & 2)) n.push_back(cur + cols);
        if (c > 0 && (open_[size_t(cur - 1)] & 1)) n.push_back(cur - 1);
        if (r > 0 && (open_[size_t(cur - cols)] & 2)) n.push_back(cur - cols);
        return n;
    }
    std::vector<int> bfs(int from) const {
        std::vector<int> d(size_t(cols * rows), -1), q{from};
        d[size_t(from)] = 0;
        for (size_t h = 0; h < q.size(); ++h)
            for (int nb : neighbours(q[h]))
                if (d[size_t(nb)] < 0) {
                    d[size_t(nb)] = d[size_t(q[h])] + 1;
                    q.push_back(nb);
                }
        return d;
    }
};

class TiltScene final : public Scene {
public:
    void frame(Ctx& c, float dt) override {
        if (!inited_) init(c);
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        Input& in = c.in;
        dt = std::min(dt, 0.05f);
        if (in.focusLost) {
            in.focusLost = false;
            if (ph_ == Ph::Play) ph_ = Ph::Pause;
        }
        age_ += dt;

        if (ph_ == Ph::Play) {
            if (in.esc) ph_ = Ph::Pause;
            readInput(c, dt);
            step(c, dt);
        } else if (ph_ == Ph::Merge) {
            mergeT_ += dt;
            px_ += (gx_ - px_) * std::min(1.f, dt * 9.f);
            py_ += (gy_ - py_) * std::min(1.f, dt * 9.f);
            if (mergeT_ > 0.7f) {
                ++level_;
                startLevel(c);
            }
        }
        if (hintT_ > 0.f) hintT_ -= dt;

        // ---- draw ----
        g.drawBackdrop(t, c.save.reduceMotion ? 0.f : c.time);
        drawBoard(c);
        drawHud(c);
        if (ph_ == Ph::Pause) drawPause(c);
        if (ph_ == Ph::Over) drawOver(c);
        (void)L;
        (void)ui;
    }

private:
    enum class Ph { Play, Pause, Merge, Over };
    bool inited_ = false;
    Ph ph_ = Ph::Play;
    Maze maze_;
    int level_ = 0, score_ = 0, hints_ = 2;
    float px_ = 0, py_ = 0, vx_ = 0, vy_ = 0, rp_ = 6.f, gx_ = 0, gy_ = 0, rg_ = 8.f;
    float timeLeft_ = 30.f, timeMax_ = 30.f, age_ = 0.f, mergeT_ = 0.f, hintT_ = 0.f, buzzT_ = 0.f, overT_ = 0.f;
    float baseX_ = 0.f, baseY_ = 0.f, calT_ = 0.f, tiltX_ = 0.f, tiltY_ = 0.f;
    int calN_ = 0;
    bool hasSensor_ = false, best_ = false, dragging_ = false;
    float dragX_ = 0.f, dragY_ = 0.f;
    uint32_t seed_ = 1;
    std::vector<int> hintPath_;

    void init(Ctx& c) {
        inited_ = true;
        hasSensor_ = [] {
            float x, y, z;
            return platform::accel(x, y, z);
        }();
        seed_ = uint32_t(SDL_GetTicks()) * 2246822519u + 5u;
        level_ = score_ = 0;
        hints_ = 2;
        best_ = false;
        startLevel(c);
    }

    void startLevel(Ctx& c) {
        (void)c;
        ph_ = Ph::Play;
        maze_.build(level_, seed_);
        rp_ = std::max(4.f, maze_.cell * 0.26f);
        rg_ = std::max(5.f, maze_.cell * 0.32f);
        px_ = maze_.cx(0);
        py_ = maze_.cy(0);
        vx_ = vy_ = 0.f;
        gx_ = maze_.cx(maze_.goalC);
        gy_ = maze_.cy(maze_.goalR);
        timeMax_ = timeLeft_ = 16.f + float(maze_.cols * maze_.rows) * 0.5f;
        hints_ = std::min(3, hints_ + (level_ > 0 ? 1 : 0));
        hintT_ = 0.f;
        mergeT_ = 0.f;
        calN_ = 0;  // re-centre the neutral tilt at the start of every level
        calT_ = 0.f;
    }

    // Reads the phone's tilt (relative to the neutral pose captured at the start), or the finger / keys as a fallback.
    void readInput(Ctx& c, float dt) {
        Input& in = c.in;
        float tx = 0.f, ty = 0.f;
        if (hasSensor_) {
            float ax, ay, az;
            if (platform::accel(ax, ay, az)) {
                const float rx = -ax / 9.81f, ry = ay / 9.81f;  // tilting the right edge down makes ax negative
                if (calT_ < 0.35f) {  // neutral pose: average of the first moments
                    calT_ += dt;
                    baseX_ = (baseX_ * float(calN_) + rx) / float(calN_ + 1);
                    baseY_ = (baseY_ * float(calN_) + ry) / float(calN_ + 1);
                    ++calN_;
                }
                tx = rx - baseX_;
                ty = ry - baseY_;
            }
            if (platform::shake()) hint(c);
        } else {
            if (in.pressed && in.my > 60.f) {
                dragging_ = true;
                dragX_ = in.mx;
                dragY_ = in.my;
            }
            if (in.released) dragging_ = false;
            if (dragging_ && in.down) {
                tx = (in.mx - dragX_) / 50.f;
                ty = (in.my - dragY_) / 50.f;
            }
        }
        if (in.left) tx -= 0.6f;
        if (in.right) tx += 0.6f;
        if (in.up) ty -= 0.6f;
        if (in.dn) ty += 0.6f;
        if (SDL_getenv("LIQ_TILT_DEMO")) {  // dev: scripted steering for screenshots
            tx = std::sin(age_ * 1.3f) * 0.8f;
            ty = std::cos(age_ * 0.9f) * 0.8f;
        }
        tiltX_ = clampv(tx, -1.f, 1.f);
        tiltY_ = clampv(ty, -1.f, 1.f);
    }

    void hint(Ctx& c) {
        if (hints_ <= 0 || ph_ != Ph::Play) return;
        --hints_;
        hintPath_ = maze_.path(maze_.cellOf(px_, py_));
        hintT_ = 3.f;
        c.audio.play(Sfx::Swipe);
    }

    // Circle against the wall rectangles: push out along the contact normal and bounce a little.
    void collide(Ctx& c, float& x, float& y, float& vx, float& vy, float r, float& impact) {
        for (const Rect& w : maze_.walls) {
            const float nx = std::max(w.x0, std::min(x, w.x1)), ny = std::max(w.y0, std::min(y, w.y1));
            float dx = x - nx, dy = y - ny;
            float d2 = dx * dx + dy * dy;
            if (d2 >= r * r) continue;
            float d = std::sqrt(d2), ux, uy;
            if (d > 1e-4f) {
                ux = dx / d;
                uy = dy / d;
            } else {  // centre inside the wall: leave through the nearest face
                const float l = x - w.x0, rr = w.x1 - x, tp = y - w.y0, bt = w.y1 - y;
                const float m = std::min(std::min(l, rr), std::min(tp, bt));
                ux = m == l ? -1.f : m == rr ? 1.f : 0.f;
                uy = m == tp ? -1.f : m == bt ? 1.f : 0.f;
                if (ux == 0.f && uy == 0.f) uy = -1.f;
                d = -m;
            }
            x += ux * (r - d);
            y += uy * (r - d);
            const float vn = vx * ux + vy * uy;
            if (vn < 0.f) {
                vx -= 1.35f * vn * ux;
                vy -= 1.35f * vn * uy;
                impact = std::max(impact, -vn);
            }
        }
        (void)c;
    }

    void step(Ctx& c, float dt) {
        timeLeft_ -= dt;
        buzzT_ -= dt;
        float impact = 0.f;
        const int sub = 4;
        const float h = dt / float(sub);
        for (int i = 0; i < sub; ++i) {
            vx_ += tiltX_ * 620.f * h;
            vy_ += tiltY_ * 620.f * h;
            const float damp = std::max(0.f, 1.f - 2.2f * h);
            vx_ *= damp;
            vy_ *= damp;
            const float sp = std::sqrt(vx_ * vx_ + vy_ * vy_);
            if (sp > 230.f) {
                vx_ *= 230.f / sp;
                vy_ *= 230.f / sp;
            }
            px_ += vx_ * h;
            py_ += vy_ * h;
            collide(c, px_, py_, vx_, vy_, rp_, impact);
        }
        if (impact > 35.f && buzzT_ <= 0.f) {  // a knock on the wall: a tick you can feel and hear
            buzzT_ = 0.09f;
            if (c.save.haptics) platform::vibrate(std::min(0.8f, impact / 240.f + 0.15f), 14);
            c.audio.play(Sfx::Drop, std::min(0.5f, impact / 400.f));
        }
        const float dx = px_ - gx_, dy = py_ - gy_;
        if (std::sqrt(dx * dx + dy * dy) < rp_ + rg_ - 3.f) {  // the drops touch: merge!
            const int bonus = int(timeLeft_ * 4.f);
            score_ += 100 * (level_ + 1) + bonus;
            ph_ = Ph::Merge;
            mergeT_ = 0.f;
            c.audio.play(Sfx::Merge, 0.7f, std::min(level_ + 3, 10), 1);
            if (c.save.haptics) platform::vibrate(0.9f, 40);
            return;
        }
        if (timeLeft_ <= 0.f) finish(c);
    }

    void finish(Ctx& c) {
        ph_ = Ph::Over;
        overT_ = 0.f;
        if (score_ > c.save.tiltBest) {
            c.save.tiltBest = score_;
            best_ = true;
        }
        c.save.store();
        c.audio.play(Sfx::Fail);
    }

    // ---- drawing ----
    void drawBoard(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        const float w = maze_.cell * float(maze_.cols), h = maze_.cell * float(maze_.rows);
        g.rect(maze_.ox, maze_.oy, w, h, withAlpha(t.ink, 120));
        // faint checkerboard so the grid is easy to read
        for (int r = 0; r < maze_.rows; ++r)
            for (int q = 0; q < maze_.cols; ++q)
                if ((r + q) & 1) g.rect(maze_.ox + maze_.cell * float(q), maze_.oy + maze_.cell * float(r), maze_.cell, maze_.cell, withAlpha(t.panelHi, 22));
        // hint path (dotted)
        if (hintT_ > 0.f && hintPath_.size() > 1) {
            const int a = int(255.f * std::min(1.f, hintT_));
            for (size_t i = 0; i + 1 < hintPath_.size(); ++i) {
                const float x0 = maze_.cx(hintPath_[i] % maze_.cols), y0 = maze_.cy(hintPath_[i] / maze_.cols);
                const float x1 = maze_.cx(hintPath_[i + 1] % maze_.cols), y1 = maze_.cy(hintPath_[i + 1] / maze_.cols);
                for (int k = 0; k < 4; ++k) {
                    const float u = float(k) / 4.f;
                    g.rect(std::floor(x0 + (x1 - x0) * u), std::floor(y0 + (y1 - y0) * u), 2, 2, withAlpha(t.accent, a));
                }
            }
        }
        for (const Rect& r : maze_.walls) {
            g.rect(r.x0, r.y0, r.x1 - r.x0, r.y1 - r.y0, t.frame);
            g.rect(r.x0, r.y0, r.x1 - r.x0, 1, t.frameHi);
        }
        // the two droplets fuse when they touch, like everywhere else in the game
        const float pulse = ph_ == Ph::Merge ? 1.f + 0.25f * std::sin(mergeT_ * 14.f) : 1.f;
        std::vector<BlobSprite> sp;
        BlobSprite me = plainSprite(px_, py_, std::min(level_ + 1, 10), 0.08f, c.time);
        me.ra = me.rb = rp_ * pulse;
        BlobSprite goal = plainSprite(gx_, gy_, std::min(level_ + 3, 10), 0.08f, c.time + 1.7f);
        goal.ra = goal.rb = rg_ * pulse;
        sp.push_back(goal);
        sp.push_back(me);
        c.raster.clear();
        c.raster.drawField(sp, t);
        g.raster(c.raster);
        // start marker
        g.rect(maze_.cx(0) - 1.f, maze_.cy(0) + rp_ + 3.f, 3, 1, t.dim);
    }

    void drawHud(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        g.rect(0, 0, float(cfg::kW), 62.f, mix(t.sky[0], t.ink, 0.6f));
        g.rect(0, 61, float(cfg::kW), 1, t.panelLo);
        g.text(L.fmt("tilt.level", std::to_string(level_ + 1)), cfg::kW / 2.f, 14, 1, t.accent, Align::Center);
        g.number(std::to_string(score_), cfg::kW / 2.f, 32, 2, t.text, Align::Center, TextFx::Outline, t.ink);
        widgets::meter(g, t, 66.f, 50.f, 138.f, 7.f, clampv(timeLeft_ / timeMax_, 0.f, 1.f));
        g.text(L.tr("tilt.shake") + " x" + std::to_string(hints_), 135, 72, 1, hints_ > 0 ? t.dim : t.panelLo, Align::Center);
        if (ph_ == Ph::Play && ui.iconButton(IconId::Pause, 232, 8, 30, 24)) ph_ = Ph::Pause;
        if (!hasSensor_ && ph_ == Ph::Play && age_ < 6.f) g.text(L.tr("tilt.nosensor"), cfg::kW / 2.f, 452, 1, t.dim, Align::Center, TextFx::Outline, t.ink);
        if (ph_ == Ph::Play && hasSensor_ && hints_ > 0 && ui.button(L.tr("tilt.hint"), 14, 446, 70, 22)) hint(c);
    }

    void drawPause(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        g.rect(0, 0, float(cfg::kW), float(cfg::kH), withAlpha(t.ink, 170));
        ui.panel(35, 140, 200, 200);
        g.text(L.tr("pause.title"), 135, 158, 2, t.accent, Align::Center, TextFx::Outline, t.ink);
        if (ui.button(L.tr("pause.resume"), 55, 190, 160, 28, true, true, BtnStyle::Primary) || c.in.esc) {
            ph_ = Ph::Play;
            calN_ = 0;
            calT_ = 0.f;  // take the pose the player resumes in as neutral
        }
        if (ui.button(L.tr("tilt.calibrate"), 55, 228, 160, 26)) {
            ph_ = Ph::Play;
            calN_ = 0;
            calT_ = 0.f;
        }
        if (ui.button(L.tr("pause.restart"), 55, 262, 160, 26)) init(c);
        if (ui.button(L.tr("pause.quit"), 55, 296, 160, 26)) c.go(SceneId::Extras);
    }

    void drawOver(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        overT_ += 1.f / 60.f;
        g.rect(0, 0, float(cfg::kW), float(cfg::kH), withAlpha(t.ink, 170));
        ui.panel(35, 150, 200, 180);
        g.text(L.tr("tilt.over"), 135, 168, 2, t.accent, Align::Center, TextFx::Outline, t.ink);
        g.text(L.fmt("tilt.level", std::to_string(level_ + 1)), 135, 196, 1, t.dim, Align::Center);
        g.number(std::to_string(score_), 135, 224, 2, t.text, Align::Center, TextFx::Outline, t.ink);
        if (best_) g.text(L.tr("over.newbest"), 135, 250, 1, t.accent, Align::Center);
        else g.text(L.tr("hud.best") + " " + std::to_string(c.save.tiltBest), 135, 250, 1, t.dim, Align::Center);
        if (ui.button(L.tr("over.restart"), 55, 270, 160, 24, true, true, BtnStyle::Primary)) init(c);
        if (ui.button(L.tr("common.back"), 55, 298, 160, 22) || c.in.esc) c.go(SceneId::Extras);
    }
};

}  // namespace

std::unique_ptr<Scene> makeTiltScene() { return std::make_unique<TiltScene>(); }

}  // namespace liq
