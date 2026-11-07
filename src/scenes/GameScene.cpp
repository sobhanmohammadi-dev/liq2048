// The in-game scene: shared run loop for all modes (mode rules come from Mode).

#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "core/Util.hpp"
#include "game/Mode.hpp"
#include "game/Replay.hpp"
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
struct PopRing {
    float x, y, age, r0;
    Color c;
};

constexpr float kParticleDraw = 0.f;    // unused by the table rasterizer
constexpr int kFieldR = 9, kColorR = 6;  // kernel radii (px) of one liquid particle
constexpr float kParticleIso = 1.1f;    // surface level of the summed particle field

class GameScene final : public Scene {
public:
    static ModeId takeMode(Ctx& c) {  // a pasted run code decides the mode
        if (c.hasPendingReplay) c.mode = ModeId(std::min(c.pendingReplay.mode, kModeCount - 1));
        return c.mode;
    }
    explicit GameScene(Ctx& c) : mid_(takeMode(c)) {
        if (c.hasPendingReplay) {
            rec_ = c.pendingReplay;
            c.hasPendingReplay = false;
            replaying_ = true;
            c.puzzleLevel = rec_.puzzle;
        }
        hot_ = c.hotseat && mid_ == ModeId::Classic && !c.hasPendingReplay;
        c.hotseat = false;
        today_ = platform::dayNumber();
        if (mid_ == ModeId::Daily && c.save.dailyDay != today_) {
            c.save.dailyDay = today_;
            c.save.dailyToday = 0;
        }
        if (mid_ == ModeId::Weekly && c.save.weeklyWeek != weekOf(today_)) {
            c.save.weeklyWeek = weekOf(today_);
            c.save.weeklyBest = 0;
        }
        battle_ = mid_ == ModeId::Battle;
        if (battle_) {  // the Battle menus prepared the match (rules + opponent)
            bcfg_ = c.battle.cfg;
            opp_ = c.battle.link ? battle::makeNetOpponent(c.battle.link) : battle::makeBot(bcfg_);
            c.battle.link.reset();
        }
        st_ = c.save.tutorialDone || replaying_ || battle_ ? St::Playing : St::Tutorial;
        c.in.focusLost = false;
        start(c);
    }
    ~GameScene() override {
        if (opp_) opp_->leave();  // closes a LAN link politely
    }

    void frame(Ctx& c, float dt) override {
        Gfx& g = c.gfx;
        c.ui.setMirror(c.save.leftHand);
        const bool paused = st_ == St::Paused || st_ == St::Settings;
        if (c.in.focusLost) {  // window lost focus / app went to background: never lose a run
            c.in.focusLost = false;
            if (st_ == St::Playing) {
                st_ = St::Paused;
                c.ui.resetFocus();
            }
        }
        if (slowT_ > 0.f) slowT_ = std::max(0.f, slowT_ - dt);
        const float slow = c.save.effSlowmo() && slowT_ > 0.f ? 1.f - 0.6f * std::min(1.f, slowT_ * 4.f) : 1.f;
        if (battle_) battleFrame(c, dt, slow);
        else if (st_ == St::Playing) simulate(c, std::min(dt, 0.05f) * slow);
        updateAudio(c, dt);
        if (!paused) {
            updateFx(dt);
            holdX_ += (aimX_ - holdX_) * std::min(1.f, dt * 24.f);
            holdPop_.step(dt, vis::kPopSpring, vis::kPopDamp);
            nextPop_.step(dt, vis::kPopSpring, vis::kPopDamp);
            dispScore_ += (float(score_) - dispScore_) * std::min(1.f, dt * 9.f);
            if (float(score_) - dispScore_ < 1.f) dispScore_ = float(score_);
            shake_ = std::max(0.f, shake_ - dt * 9.f);
            if (st_ == St::Over) overT_ += dt;
        }

        g.drawBackdrop(*c.theme, c.save.reduceMotion ? 0.f : c.time);
        const bool spec = viewing();
        if (!spec) drawNextPanel(c);
        int sx = 0, sy = 0;
        if (shake_ > 0.05f && !paused && c.save.effShake()) {
            sx = int(std::lround(rngFx_.uni(-shake_, shake_)));
            sy = int(std::lround(rngFx_.uni(-shake_, shake_)));
        }
        g.setOffset(sx, sy);
        g.drawTankBg();
        drawBubbles(c);
        drawDeadLine(c, !spec && world_.overflow() > 0.2f, spec ? cfg::kDeadY + 22.f : deadY_);
        if (!spec) drawGuide(c);
        drawBlobs(c, sx, sy);
        drawTankFrame(c);
        if (!spec) drawFx(c);
        g.setOffset(0, 0);
        drawHud(c);
        if (battle_) drawViewButton(c);
        else drawHistory(c);
        drawToast(c);
        if (!replaying_) drawHint(c, paused);
        if (replaying_ && st_ == St::Playing) replayBar(c);
        if (replaying_ && st_ != St::Over) c.gfx.text(c.loc.tr(replayPaused_ ? "replay.paused" : "replay.banner"), cfg::kW / 2.f, 78, 1, c.theme->accent, Align::Center, TextFx::Outline, c.theme->ink);

        switch (st_) {
            case St::Tutorial: tutorial(c); break;
            case St::Paused: pauseMenu(c); break;
            case St::Settings:
                g.dim(*c.theme, 1);
                if (drawSettings(c, 72, false) || c.in.esc) st_ = St::Paused;
                break;
            case St::Over:
                if (shotPending_) {
                    shotPending_ = false;
                    captureCard(c);
                } else {
                    overlay(c);
                }
                break;
            default: break;
        }
        if (battle_ && countdown_ > 0.f) drawCountdown(c);
    }

private:
    enum class St { Tutorial, Playing, Paused, Settings, Over };

    // ----- run state -----
    ModeId mid_;
    std::unique_ptr<Mode> mode_;
    World world_;
    Rng rng_, rngFx_;
    St st_ = St::Playing;
    Outcome outcome_ = Outcome::Running;
    int today_ = 0;
    int score_ = 0, drops_ = 0, cur_ = 0, next_ = 0, maxLevel_ = 0, tutPage_ = 0;
    float dispScore_ = 0, aimX_ = (cfg::kFieldL + cfg::kFieldR) / 2, holdX_ = aimX_;
    float tension_ = 0.f;
    bool shotPending_ = false;
    bool hot_ = false;  // pass-and-play: two players alternate drops in one pot
    int hs_[2] = {0, 0}, turn_ = 0, lastDropper_ = 0;
    std::vector<int> trail_;  // score before every drop (ghost pace)
    float queuedDrop_ = 0, perfEma_ = 1.f / 60.f;
    bool perfLow_ = false;
    int wantSub_ = 2;               // liquid substeps the governor asks for (applied on a step boundary)
    size_t subIdx_ = 0;             // replay: next recorded substep change
    bool olderShown_ = false;
    bool replayPaused_ = false;     // watching a run: paused with the bar button
    int replaySpeed_ = 1;           // 1, 2 or 4
    float slowT_ = 0, cooldown_ = 0, quiet_ = 0, elapsed_ = 0, acc_ = 0, shake_ = 0, overT_ = 0, toastT_ = 0, chainT_ = 0;
    int chainShown_ = 0;
    // Undo (Classic and Zen): one step back, charges are earned by chains.
    struct Snap {
        World world;
        Rng rng;
        int score = 0, drops = 0, cur = 0, next = 0, maxLevel = 0;
        float elapsed = 0, quiet = 0;
        uint32_t simStep = 0;
        bool valid = false;
    } snap_;
    int undos_ = 1;
    // Battle (v1.7.0)
    bool battle_ = false;
    battle::MatchConfig bcfg_;
    std::unique_ptr<battle::Opponent> opp_;
    float countdown_ = 0.f;
    bool viewOpp_ = false, myDone_ = false, toppedOut_ = false, resultSet_ = false, oppLeft_ = false;
    enum class Result { None, Win, Loss, Draw } result_ = Result::None;
    int lastTick_ = 0;
    bool viewing() const { return battle_ && viewOpp_ && opp_; }
    // Replay (v1.4.1): a finished run can be watched again; playback never touches saves or achievements.
    Replay rec_;
    uint32_t simStep_ = 0;
    size_t repIdx_ = 0;
    bool replaying_ = false;
    uint32_t replayTail_ = 0;
    // Modes (v1.3.0)
    int mod_ = 0;                       // active modifier (0 none, 1 low g, 2 heavy g, 3 thick, 4 bonus drops)
    // Harder than before (v1.6.1): the overflow line sits lower in the pot, so there is less room to stack.
    // Zen, Puzzle and Rising Tide (whose line moves) keep the original start position.
    float deadBase() const { return (mid_ == ModeId::Zen || mid_ == ModeId::Puzzle || mid_ == ModeId::Tide) ? cfg::kDeadY : cfg::kDeadY + 22.f; }
    float deadY_ = cfg::kDeadY, tideRelief_ = 0.f, autoT_ = 0.f, autoFill_ = 0.f;
    int queueIdx_ = 0, runMerges_ = 0, runChain_ = 0, stars_ = 0;
    std::string hint_;                  // short coaching banner inside the pot
    float hintT_ = 0.f;                 // seconds left, < 0 = until cleared
    int coach_ = -1;                    // first-run coach step (-1 = off)
    bool statsDone_ = false;
    float gravityNow() const { return cfg::kGravity * mode_->gravityScale() * (mod_ == 1 ? 0.55f : mod_ == 2 ? 1.4f : 1.f); }
    void setHint(const std::string& s, float t) {
        hint_ = s;
        hintT_ = t;
    }
    bool undoAllowed() const { return !hot_ && !replaying_ && (mid_ == ModeId::Classic || mid_ == ModeId::Zen); }

    void buzz(Ctx& c, float strength, int ms) const {
        if (c.save.haptics) platform::vibrate(strength, ms);
    }

    void saveSnap() {
        snap_.world = world_;
        snap_.rng = rng_;
        snap_.score = score_;
        snap_.drops = drops_;
        snap_.cur = cur_;
        snap_.next = next_;
        snap_.maxLevel = maxLevel_;
        snap_.elapsed = elapsed_;
        snap_.quiet = quiet_;
        snap_.simStep = simStep_;
        snap_.valid = true;
    }

    void doUndo(Ctx& c) {
        if (!undoAllowed() || replaying_ || !snap_.valid || undos_ <= 0 || st_ != St::Playing) return;
        world_ = snap_.world;
        rng_ = snap_.rng;
        score_ = snap_.score;
        drops_ = snap_.drops;
        cur_ = snap_.cur;
        next_ = snap_.next;
        maxLevel_ = snap_.maxLevel;
        elapsed_ = snap_.elapsed;
        quiet_ = snap_.quiet;
        simStep_ = snap_.simStep;
        rec_.truncateFrom(simStep_);
        snap_.valid = false;
        --undos_;
        parts_.clear();
        texts_.clear();
        rings_.clear();
        chainT_ = 0;
        cooldown_ = 0.2f;
        acc_ = 0;
        world_.resetOverflow();
        holdPop_ = Spring{-0.3f, 6.f};
        c.audio.play(Sfx::Swipe);
        buzz(c, 0.3f, 15);
    }
    bool aiming_ = false, committed_ = false, newBest_ = false;
    Spring holdPop_, nextPop_;
    std::string toast_;
    std::vector<Particle> parts_;
    std::vector<FloatText> texts_;
    std::vector<PopRing> rings_;

    int roll() {
        if (mode_) {
            const int q = mode_->queueLevel(queueIdx_++);
            if (q >= 0) return q;
            if (mode_->puzzle()) return 0;  // queue used up
        }
        if (mid_ == ModeId::Battle) return battle::battleLevel(bcfg_.seed, queueIdx_++, mod_);
        const int r = rng_.range(0, 99);
        if (mod_ == 4) return r < 15 ? 1 : r < 45 ? 2 : r < 80 ? 3 : 4;
        if (mid_ == ModeId::Zen) return r < 40 ? 0 : r < 70 ? 1 : r < 90 ? 2 : 3;  // Zen stays relaxed
        // Everyone else gets bigger drops the longer a run lasts (cumulative weights for levels 0..5).
        static const int w[5][6] = {{38, 70, 90, 100, 100, 100}, {28, 56, 78, 93, 100, 100}, {20, 42, 66, 86, 100, 100},
                                    {14, 32, 54, 78, 100, 100}, {10, 24, 44, 68, 88, 100}};
        const int tier = std::min(4, drops_ / 12);
        int lvl = 0;
        while (lvl < 5 && r >= w[tier][lvl]) ++lvl;
        return lvl;
    }

    void start(Ctx& c) {
        mode_ = mid_ == ModeId::Battle ? makeBattleMode(bcfg_.seed, bcfg_.duration, bcfg_.modifier) : makeMode(mid_, c.save, today_, c.puzzleLevel);
        mod_ = mode_->fixedModifier() >= 0 ? mode_->fixedModifier()
               : (mid_ == ModeId::Daily || mid_ == ModeId::Puzzle) ? 0 : c.save.modifier;
        hs_[0] = hs_[1] = 0;
        turn_ = lastDropper_ = 0;
        world_.clear();
        world_.viscScale = mod_ == 3 ? 3.f : 1.f;
        // Daily and Puzzle must play identically for everyone, so they always use the reference quality.
        world_.substeps = (mid_ == ModeId::Daily || mid_ == ModeId::Puzzle || mid_ == ModeId::Battle) ? 2 : c.save.quality + 1;
        if (replaying_) {
            mod_ = rec_.modifier;
            world_.viscScale = mod_ == 3 ? 3.f : 1.f;
            world_.substeps = rec_.substeps;
        }
        wantSub_ = world_.substeps;
        subIdx_ = 0;
        replayPaused_ = false;
        olderShown_ = false;
        replaySpeed_ = 1;
        replayTail_ = 0;
        countdown_ = battle_ ? 3.4f : 0.f;
        lastTick_ = 0;
        viewOpp_ = myDone_ = toppedOut_ = resultSet_ = oppLeft_ = false;
        result_ = Result::None;
        deadY_ = deadBase();
        tideRelief_ = autoT_ = autoFill_ = 0.f;
        queueIdx_ = runMerges_ = runChain_ = stars_ = 0;
        statsDone_ = false;
        hint_.clear();
        hintT_ = 0.f;
        parts_.clear();
        texts_.clear();
        rings_.clear();
        uint32_t seed = mode_->seed();
        if (replaying_) seed = rec_.seed;
        else if (!seed) seed = uint32_t(SDL_GetTicks()) * 2246822519u + 7u;
        rng_.seed(seed);
        simStep_ = 0;
        repIdx_ = 0;
        if (!replaying_) {
            rec_.clear();
            rec_.mode = int(mid_);
            rec_.modifier = mod_;
            rec_.substeps = world_.substeps;
            rec_.gravity = gravityNow();
            rec_.seed = seed;
            rec_.puzzle = c.puzzleLevel;
            rec_.physics = cfg::kPhysicsVersion;
        }
        rngFx_.seed(99u);
        score_ = drops_ = maxLevel_ = 0;
        dispScore_ = 0;
        trail_.clear(); queuedDrop_ = 0; cooldown_ = quiet_ = elapsed_ = acc_ = shake_ = overT_ = toastT_ = chainT_ = slowT_ = 0;
        snap_.valid = false;
        undos_ = 1;
        aiming_ = committed_ = newBest_ = false;
        outcome_ = Outcome::Running;
        if (const PuzzleDef* pz = mode_->puzzle()) {  // pre-filled pot, let it settle before play
            for (const auto& p : pz->pot) world_.spawn(p.x, p.y, p.level);
            for (int i = 0; i < 150; ++i) world_.step(cfg::kStep, cfg::kGravity);
            world_.events.clear();
            world_.impacts.clear();
        }
        cur_ = roll();
        next_ = roll();
        introHint(c);
        aimX_ = holdX_ = (cfg::kFieldL + cfg::kFieldR) / 2;
        holdPop_ = Spring{-0.4f, 6.f};
        nextPop_ = Spring{};
    }

    // The first time a mode is played (and for rules that change), show a short hint inside the pot.
    void introHint(Ctx& c) {
        if (mid_ == ModeId::Battle) return;
        static const char* desc[kModeCount] = {"mode.classic.desc", "mode.timed.desc", "mode.target.desc", "mode.daily.desc",
                                               "mode.zen.desc", "mode.rain.desc", "mode.tide.desc", "mode.weekly.desc", "mode.puzzle.desc", "mode.battle.desc"};
        static const char* modKey[5] = {"mod.none", "mod.lowg", "mod.heavy", "mod.thick", "mod.bonus"};
        const uint32_t bit = 1u << int(mid_);
        std::string s;
        if (!(c.save.seenModes & bit) && st_ != St::Tutorial) {
            c.save.seenModes |= bit;
            s = c.loc.tr(desc[int(mid_)]);
        }
        if (mid_ == ModeId::Weekly) s = (s.empty() ? "" : s + " ") + c.loc.fmt("mod.rule", c.loc.tr(modKey[mod_]));
        else if (mid_ == ModeId::Puzzle) {
            const PuzzleDef* pz = mode_->puzzle();
            s = c.loc.fmt("puzzle.goal", std::to_string(1 << pz->goal)) + " " + c.loc.fmt("puzzle.par", std::to_string(pz->par));
        }
        if (!s.empty()) setHint(s, 5.f);
    }

    Stats stats() const {
        Stats s;
        s.score = score_;
        s.highest = maxLevel_;
        s.drops = drops_;
        const int lim = mode_->dropLimit();
        s.dropsLeft = lim < 0 ? -1 : std::max(0, lim - drops_);
        s.elapsed = elapsed_;
        s.quiet = quiet_;
        s.overflow = world_.overflow() > cfg::kOverflowSeconds;
        return s;
    }

    int bestFor(const Ctx& c) const {
        return mid_ == ModeId::Daily ? c.save.dailyToday : mid_ == ModeId::Weekly ? c.save.weeklyBest : c.save.best[int(mid_)];
    }

    // ----- simulation -----
    void tryDrop(Ctx& c, bool force = false, int levelOverride = -1) {
        const int lim = mode_->dropLimit();
        if ((cooldown_ > 0 && !force) || (lim >= 0 && drops_ >= lim)) return;
        if (undoAllowed()) saveSnap();
        const int lvl = levelOverride >= 0 ? levelOverride : cur_;
        aimX_ = quantX(aimX_);  // run codes store positions in 1/128 px
        if (!replaying_) rec_.drops.push_back({simStep_, aimX_, lvl});
        if (!replaying_ && trail_.size() < size_t(Save::kGhostMax)) trail_.push_back(score_);
        world_.spawn(aimX_, cfg::kSpawnY, lvl);
        ++drops_;
        cur_ = next_;
        next_ = roll();
        cooldown_ = cfg::kDropCooldown;
        quiet_ = 0;
        if (hot_) {
            lastDropper_ = turn_;
            turn_ ^= 1;
            setHint(c.loc.fmt("hot.turn", c.loc.tr(turn_ ? "hot.p2" : "hot.p1")), 1.6f);
        }
        holdPop_ = Spring{-0.5f, 8.f};
        nextPop_ = Spring{-0.45f, 7.f};
        c.audio.play(Sfx::Drop);
        buzz(c, 0.25f, 12);
        if (coach_ == 0) {
            coach_ = 1;
            setHint(c.loc.tr("coach.2"), -1.f);
        }
    }

    void grant(Ctx& c, int id) {
        if (replaying_) return;
        if (c.save.unlock(id)) {
            toast_ = c.loc.tr(achKey(id));
            toastT_ = 3.0f;
            c.audio.play(Sfx::Unlock);
        }
    }

    void onMerge(Ctx& c, const MergeEvent& e) {
        const Theme& t = *c.theme;
        const int value = 1 << e.level;
        ++runMerges_;
        runChain_ = std::max(runChain_, e.chain);
        tideRelief_ += 3.f + 1.5f * float(e.level);
        if (coach_ == 1) {
            coach_ = 2;
            setHint(c.loc.tr("coach.3"), 3.5f);
        }
        if (e.chain >= 7) grant(c, kChain7);
        score_ += value * e.chain;
        if (hot_) hs_[lastDropper_] += value * e.chain;
        quiet_ = 0;
        maxLevel_ = std::max(maxLevel_, e.level);
        c.audio.play(Sfx::Merge, 0.5f, e.level, e.chain);
        const Color col = t.blob[std::min(e.level, 11)];
        const int n = c.save.reduceMotion ? 0 : (8 + e.level * 2) / (c.save.calm ? 2 : 1);
        for (int i = 0; i < n; ++i) {
            const float a = rngFx_.uni(0.f, 6.2832f), sp = rngFx_.uni(30.f, 120.f);
            parts_.push_back({e.x, e.y, std::cos(a) * sp, std::sin(a) * sp - 30.f, 0.55f, 0.55f,
                              rngFx_.range(0, 2) ? col : mix(col, rgb(0xffffff), 0.6f), 1 + rngFx_.range(0, 1)});
        }
        if (!c.save.reduceMotion) rings_.push_back({e.x, e.y, 0.f, World::radiusFor(e.level) * 0.8f, col});
        texts_.push_back({e.x, e.y - World::radiusFor(e.level) - 4.f, e.chain >= 3 ? 1.3f : 0.9f,
                          "+" + std::to_string(value * e.chain), e.chain >= 3 ? t.accent : t.text, e.chain >= 3 ? 2 : 1});
        buzz(c, 0.35f + 0.05f * float(std::min(e.level, 8)) + 0.1f * float(std::min(e.chain, 4)), 18 + 6 * std::min(e.level, 8) + 15 * std::min(e.chain - 1, 3));
        if (e.level >= 8 || e.chain >= 4) slowT_ = 0.45f;
        if (e.chain >= 3 && undoAllowed() && undos_ < 3) {
            ++undos_;
            texts_.push_back({e.x, e.y + 8.f, 1.2f, "+" + c.loc.tr("hud.undo"), t.accent, 1});
        }
        if (e.level >= 5 || e.chain >= 3) shake_ = std::max(shake_, 1.2f + 0.25f * float(e.level) + 0.8f * float(e.chain >= 3));
        if (e.chain >= 2) {
            chainShown_ = e.chain;
            chainT_ = 1.1f;
        }
        if (e.level >= 7 && e.level <= 11) grant(c, kFirst128 + (e.level - 7));
        if (e.chain >= 3) grant(c, kChain3);
        if (e.chain >= 5) grant(c, kChain5);
        if (score_ >= 1000) grant(c, kScore1k);
        if (score_ >= 5000) grant(c, kScore5k);
    }

    void onImpact(Ctx& c, const Impact& im) {
        c.audio.play(Sfx::Splash, im.strength, im.level);
        if (im.strength > 0.5f) buzz(c, 0.2f * im.strength, 10);
        const Color col = c.theme->blob[std::min(im.level, 11)];
        const int n = c.save.reduceMotion ? 0 : 4 + int(im.strength * 10.f);
        for (int i = 0; i < n; ++i) {  // spray: tiny pixels flung up and sideways
            const float a = -1.57f + rngFx_.uni(-1.2f, 1.2f), sp = rngFx_.uni(40.f, 70.f + 90.f * im.strength);
            parts_.push_back({im.x + rngFx_.uni(-8.f, 8.f), im.y, std::cos(a) * sp, std::sin(a) * sp, 0.45f, 0.45f, col, 1 + rngFx_.range(0, 1)});
        }
        shake_ = std::max(shake_, im.strength * 1.6f);
    }

    // Music tension follows the pot: the fuller it gets, the busier the soundtrack.
    void updateAudio(Ctx& c, float dt) {
        float top = float(cfg::kFloor);
        for (const auto& d : world_.drops)
            if (d.age > 1.f) top = std::min(top, d.top);
        float target = clampv((float(cfg::kFloor) - top) / (float(cfg::kFloor) - deadY_), 0.f, 1.f);
        if (chainT_ > 0.f) target = std::min(1.f, target + 0.25f);
        if (st_ != St::Playing) target *= 0.3f;
        tension_ += (target - tension_) * std::min(1.f, dt * 1.5f);
        c.audio.setIntensity(tension_);
    }

    void simulate(Ctx& c, float dt, bool locked = false) {
        Input in = c.in;  // a copy: the pause menu (drawn later) still sees the real clicks
        if (locked) in.pressed = in.down = in.released = in.drop = in.left = in.right = in.undo = in.esc = false;
        if (viewing()) {  // looking at the other pot: no aiming or dropping, the clock keeps running
            in.pressed = in.down = in.released = in.drop = false;
            aiming_ = false;
        }
        if (in.esc) {
            st_ = St::Paused;
            c.ui.resetFocus();
            return;
        }
        if (in.undo) doUndo(c);
        if (replaying_) in.pressed = in.down = in.released = in.drop = in.left = in.right = false;
        // Aiming: drag with mouse/finger, release to drop. Keys: arrows + space/down.
        const float rad = World::radiusFor(cur_);
        if (in.pressed && in.my > 64 && !(battle_ && in.my > 446)) aiming_ = true;
        if (aiming_ && (in.down || in.released)) aimX_ = in.mx;
        if (in.left) aimX_ -= cfg::kAimSpeed * dt;
        if (in.right) aimX_ += cfg::kAimSpeed * dt;
        if (in.wheel != 0.f && !aiming_) aimX_ += in.wheel * 8.f;  // mouse wheel nudges the aim
        aimX_ = clampv(aimX_, cfg::kFieldL + rad, cfg::kFieldR - rad);
        bool want = false;
        if (aiming_ && in.released) {
            want = true;
            aiming_ = false;
        }
        if (in.drop) want = true;
        cooldown_ -= dt;
        // Input buffering: a tap that lands while the drop cooldown is running is kept for a moment and fires
        // as soon as the cooldown ends, instead of being lost (felt like "I have to tap twice").
        if (want && cooldown_ > 0.f && !replaying_) {
            queuedDrop_ = 0.45f;
            want = false;
        } else if (queuedDrop_ > 0.f) {
            queuedDrop_ -= dt;
            if (cooldown_ <= 0.f) {
                want = true;
                queuedDrop_ = 0.f;
            }
        }
        if (!(replaying_ && replayPaused_)) elapsed_ += dt;
        quiet_ += dt;
        mode_->update(dt);
        if (want && mode_->manualDrop()) tryDrop(c);
        if (const float iv = mode_->autoDropInterval(elapsed_); iv > 0.f && !replaying_) {
            autoT_ += dt;
            autoFill_ = std::min(1.f, autoT_ / iv);
            if (autoT_ >= iv) {
                autoT_ = 0.f;
                tryDrop(c, true);
            }
        }
        if (mid_ == ModeId::Rain && elapsed_ >= 60.f) grant(c, kRain60);
        if (mid_ == ModeId::Tide && elapsed_ >= 90.f) grant(c, kTide90);

        const bool frozen = replaying_ && replayPaused_;
        if (!frozen) acc_ = std::min(acc_ + dt * float(replaying_ ? replaySpeed_ : 1), cfg::kStep * 3.f * float(replaying_ ? replaySpeed_ : 1));  // never catch up more than 3 steps (slow phones)
        // Performance governor: on a device that cannot hold ~50 fps, halve the liquid substeps in modes where
        // the physics need not be identical for everyone.
        if (!replaying_ && mid_ != ModeId::Daily && mid_ != ModeId::Puzzle && mid_ != ModeId::Battle) {
            perfEma_ += (dt - perfEma_) * 0.04f;
            if (!perfLow_ && perfEma_ > 0.024f) perfLow_ = true;
            if (perfLow_ && perfEma_ < 0.017f) perfLow_ = false;
            wantSub_ = perfLow_ ? 1 : c.save.quality + 1;
        }
        while (!frozen && acc_ >= cfg::kStep) {
            // Everything that touches the physics happens on a step boundary and is a function of the step count,
            // never of frame time, so a recorded run replays exactly (also on a different frame rate).
            if (replaying_) {
                while (subIdx_ < rec_.subs.size() && rec_.subs[subIdx_].first <= simStep_) world_.substeps = rec_.subs[subIdx_++].second;
            } else if (wantSub_ != world_.substeps) {
                world_.substeps = wantSub_;
                rec_.subs.push_back({simStep_, uint8_t(wantSub_)});
            }
            while (replaying_ && repIdx_ < rec_.drops.size() && rec_.drops[repIdx_].step <= simStep_) {
                aimX_ = rec_.drops[repIdx_].x;
                tryDrop(c, true, rec_.drops[repIdx_].level);
                ++repIdx_;
            }
            world_.step(cfg::kStep, gravityNow());
            ++simStep_;
            acc_ -= cfg::kStep;
            for (const auto& e : world_.events) onMerge(c, e);
            world_.events.clear();
            for (const auto& im : world_.impacts) onImpact(c, im);
            world_.impacts.clear();
            if (mid_ == ModeId::Tide) {
                const float shift = mode_->deadLineShift(float(simStep_) * cfg::kStep);
                tideRelief_ = std::min(tideRelief_, shift);
                // The line keeps moving all game (see Tide::deadLineShift); merges lift it, but never above its start.
                const float target = cfg::kDeadY + shift - tideRelief_;
                deadY_ += clampv(target - deadY_, -50.f * cfg::kStep, 50.f * cfg::kStep);
            }
            world_.updateOverflow(deadY_, cfg::kStep);
            if (!mode_->failable() && world_.overflow() > 1.5f) popOverflow(c);
            if (replaying_ && rec_.endStep && simStep_ >= rec_.endStep) break;
        }

        if (replaying_ && rec_.endStep) {  // a code that knows where the run ended plays exactly to that point
            if (simStep_ >= rec_.endStep && repIdx_ >= rec_.drops.size()) {
                finish(c, rec_.outcome == 1 ? Outcome::Won : rec_.outcome == 2 ? Outcome::Lost : Outcome::Finished);
                return;
            }
            return;
        }
        const Outcome o = mode_->evaluate(stats());
        if (o != Outcome::Running) finish(c, o);
        else if (replaying_ && repIdx_ >= rec_.drops.size() && ++replayTail_ > 60u * 4u) finish(c, Outcome::Finished);
    }

    // Controls while watching a run code: pause / play, speed, and how far the replay is.
    void replayBar(Ctx& c) {
        Gfx& g = c.gfx;
        Ui& ui = c.ui;
        const Theme& t = *c.theme;
        const float y = 452.f;
        if (ui.iconButton(replayPaused_ ? IconId::Play : IconId::Pause, 22, y, 30, 24, true, true)) replayPaused_ = !replayPaused_;
        static const int speeds[3] = {1, 2, 4};
        if (ui.button(std::to_string(replaySpeed_) + "x", 58, y, 36, 24)) {
            int k = 0;
            while (k < 2 && speeds[k] != replaySpeed_) ++k;
            replaySpeed_ = speeds[(k + 1) % 3];
        }
        const uint32_t total = rec_.endStep ? rec_.endStep : (rec_.drops.empty() ? 1u : rec_.drops.back().step + 240u);
        const float f = clampv(float(simStep_) / float(std::max(1u, total)), 0.f, 1.f);
        const float bx = 102.f, bw = 146.f;
        g.rect(bx - 1, y + 8, bw + 2, 10, t.ink);
        g.rect(bx, y + 9, bw, 8, t.panelLo);
        g.rect(bx, y + 9, bw * f, 8, t.accent);
        if (c.in.pressed && c.in.mx >= 22 && c.in.mx <= 94 && c.in.my >= y && c.in.my <= y + 24) c.audio.play(Sfx::Click);
        if (rec_.physics != cfg::kPhysicsVersion && !olderShown_) {
            olderShown_ = true;
            toast_ = c.loc.tr("replay.older");
            toastT_ = 4.f;
        }
    }

    // Zen has no fail state: liquid that settles above the line drains away harmlessly.
    void popOverflow(Ctx& c) {
        const Theme& t = *c.theme;
        for (const Drop& d : world_.popAbove(deadY_)) {
            const Color col = t.blob[std::min(d.level, 11)];
            for (int i = 0; i < 14; ++i)
                parts_.push_back({d.x, d.y, rngFx_.uni(-90.f, 90.f), rngFx_.uni(-90.f, 90.f), 0.5f, 0.5f, col, 2});
            rings_.push_back({d.x, d.y, 0.f, World::radiusFor(d.level), col});
        }
        world_.resetOverflow();
    }

    void commit(Ctx& c) {
        if (committed_) return;
        committed_ = true;
        if (!statsDone_) {  // lifetime statistics
            statsDone_ = true;
            Save& sv = c.save;
            ++sv.gamesPlayed;
            sv.totalDrops += drops_;
            sv.totalMerges += runMerges_;
            sv.longestChain = std::max(sv.longestChain, runChain_);
            sv.highestLevel = std::max(sv.highestLevel, maxLevel_);
            sv.playSeconds += int(elapsed_);
            if (sv.totalDrops >= 1000) grant(c, kDrops1000);
            if (sv.streak >= 3) grant(c, kStreak3);
            if (mod_ != 0 && mid_ != ModeId::Weekly && score_ >= 1000) grant(c, kModded);
        }
        if (mid_ != ModeId::Puzzle && mid_ != ModeId::Battle && !hot_) {
            const bool counts = mod_ == 0 || mid_ == ModeId::Weekly;  // modifiers keep their own, unranked, runs
            int& best = mid_ == ModeId::Weekly ? c.save.weeklyBest : c.save.best[int(mid_)];
            newBest_ = counts && score_ > best;
            if (newBest_) best = score_;
            if (counts && !replaying_) {
                std::vector<int>& h = c.save.hist[int(mid_)];
                h.push_back(score_);
                if (h.size() > size_t(Save::kHistMax)) h.erase(h.begin(), h.end() - Save::kHistMax);
                if (newBest_ && ghostMode()) {
                    trail_.push_back(score_);
                    c.save.ghost[int(mid_)] = trail_;
                }
            }
            if (mid_ == ModeId::Daily && score_ > c.save.dailyToday) c.save.dailyToday = score_;
        }
        c.save.store();
    }

    void finish(Ctx& c, Outcome o) {
        outcome_ = o;
        if (!replaying_) {
            rec_.endStep = simStep_;
            rec_.outcome = o == Outcome::Won ? 1 : o == Outcome::Lost ? 2 : 3;
        }
        st_ = St::Over;
        overT_ = 0;
        if (replaying_) {  // watching only: no stats, bests, stars or achievements
            c.audio.play(Sfx::Success);
            return;
        }
        if (battle_) {  // my run is over; the opponent decides the result
            myDone_ = true;
            toppedOut_ = o == Outcome::Lost;
            opp_->sendFinal(selfInfo());
            commit(c);
            c.audio.play(toppedOut_ ? Sfx::Fail : Sfx::Success);
            buzz(c, 0.5f, 60);
            return;
        }
        if (o == Outcome::Won && mid_ == ModeId::Target) {
            c.save.targetStage = std::min(c.save.targetStage + 1, 2);
            grant(c, kTargetWin);
        }
        if (o == Outcome::Finished && mid_ == ModeId::Weekly) grant(c, kWeekly);
        if (o == Outcome::Won && mid_ == ModeId::Puzzle) {
            const PuzzleDef* pz = mode_->puzzle();
            stars_ = puzzleStarsFor(*pz, drops_);
            unsigned char& st = c.save.puzzleStars[std::min(c.puzzleLevel, Save::kPuzzleMax - 1)];
            st = (unsigned char)std::max<int>(st, stars_);
            if (c.save.puzzlesSolved() >= 5) grant(c, kPuzzle5);
            if (stars_ == 3) grant(c, kPuzzle3Star);
        }
        commit(c);
        c.audio.play(o == Outcome::Lost ? Sfx::Fail : Sfx::Success);
        buzz(c, o == Outcome::Lost ? 0.7f : 0.5f, o == Outcome::Lost ? 140 : 60);
    }

    // ----- Battle -----
    battle::SelfInfo selfInfo() const {
        battle::SelfInfo me;
        me.score = score_;
        me.maxLevel = maxLevel_;
        me.drops = drops_;
        me.finished = myDone_;
        me.toppedOut = toppedOut_;
        me.elapsed = elapsed_;
        me.world = &world_;
        me.aimX = aimX_;
        me.heldLevel = cur_;
        return me;
    }

    void battleFrame(Ctx& c, float dt, float slow) {
        const bool lan = opp_->isLan();
        const bool menuUp = st_ == St::Paused || st_ == St::Settings;
        if (countdown_ > 0.f) {  // 3, 2, 1, GO
            if (!menuUp || lan) countdown_ -= dt;
            const int n = int(std::ceil(countdown_));
            if (n != lastTick_ && n >= 0) {
                lastTick_ = n;
                c.audio.play(n > 0 ? Sfx::Click : Sfx::Swipe);
            }
            if (lan) opp_->update(dt, selfInfo());  // keep the link alive
            return;
        }
        if (!menuUp || lan) {  // a bot match really pauses; a LAN match cannot
            opp_->fastForward(myDone_);
            opp_->update(dt, selfInfo());
        }
        if (st_ == St::Playing) simulate(c, std::min(dt, 0.05f) * slow);
        else if (menuUp && lan) simulate(c, std::min(dt, 0.05f) * slow, true);
        if (!resultSet_) {
            if (opp_->disconnected()) {
                oppLeft_ = true;
                if (!myDone_) {
                    myDone_ = true;
                    outcome_ = Outcome::Finished;
                    st_ = St::Over;
                    overT_ = 0;
                    commit(c);
                }
                setResult(c);
            } else if (myDone_ && opp_->finished()) {
                setResult(c);
            }
        }
    }

    void setResult(Ctx& c) {
        resultSet_ = true;
        const int mine = score_, theirs = opp_->score();
        if (oppLeft_ || mine > theirs) result_ = Result::Win;
        else if (mine < theirs) result_ = Result::Loss;
        else if (maxLevel_ != opp_->maxLevel()) result_ = maxLevel_ > opp_->maxLevel() ? Result::Win : Result::Loss;
        else result_ = Result::Draw;
        if (result_ == Result::Win) ++c.save.battleWins;
        else if (result_ == Result::Loss) ++c.save.battleLosses;
        else ++c.save.battleDraws;
        c.save.store();
        c.audio.play(result_ == Result::Loss ? Sfx::Fail : Sfx::Unlock);
        overT_ = 0;
    }

    void rematch(Ctx& c) {  // bot matches only: new seed, fresh bot
        bcfg_.seed = bcfg_.seed * 1664525u + 1013904223u;
        opp_ = battle::makeBot(bcfg_);
        start(c);
        st_ = St::Playing;
    }

    void drawViewButton(Ctx& c) {
        if (resultSet_ && viewOpp_ == false && st_ == St::Over) {
            // still offered after the result, so the opponent's final pot can be inspected
        }
        const bool can = st_ == St::Playing || st_ == St::Over || countdown_ > 0.f;
        const std::string label = c.loc.tr(viewOpp_ ? "battle.back" : "battle.view");
        if (c.ui.button(label, 22, 450, 226, 22, can, can, viewOpp_ ? BtnStyle::Primary : BtnStyle::Normal)) viewOpp_ = !viewOpp_;
    }

    void drawCountdown(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        g.dim(t, 0);
        const float frac = countdown_ - std::floor(countdown_);
        const int n = int(std::ceil(countdown_));
        const int scale = 8 + (c.save.reduceMotion ? 0 : int(frac * 4.f));
        if (n >= 1 && n <= 3) g.number(std::to_string(n), cfg::kW / 2.f, 230, scale, t.accent, Align::Center, TextFx::Outline, t.ink);
        else g.text(c.loc.tr("battle.go"), cfg::kW / 2.f, 230, 5, t.accent, Align::Center, TextFx::Outline, t.ink);
        g.text(opp_->name(), cfg::kW / 2.f, 290, 1, t.text, Align::Center, TextFx::Outline, t.ink);
        g.text("VS", cfg::kW / 2.f, 276, 1, t.dim, Align::Center);
    }

    void battleOverlay(Ctx& c) {
        if (viewing()) return;  // the opponent's pot is on screen; the button below leads back
        Gfx& g = c.gfx;
        Ui& ui = c.ui;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        g.dim(t, 0);
        const float slide = std::min(1.f, overT_ * 4.f);
        const float oy = std::floor((1.f - slide) * -60.f);
        const float px = 25, pw = 220, py = 96 + oy;
        ui.panel(px, py, pw, 330);
        const float cx = cfg::kW / 2.f;
        if (!resultSet_) {
            g.text(L.tr("battle.waitopp"), cx, py + 26, 1, t.accent, Align::Center);
            const int dots = int(c.time * 3.f) % 4;  // small spinner
            for (int i = 0; i < 4; ++i) g.rect(cx - 15.f + float(i) * 10.f, py + 44.f, 6, 6, i == dots ? t.accent : t.panelLo);
        } else {
            const bool win = result_ == Result::Win, loss = result_ == Result::Loss;
            g.text(L.tr(win ? "battle.victory" : loss ? "battle.defeat" : "battle.draw"), cx, py + 28, 2, win ? t.accent : loss ? t.danger : t.text,
                   Align::Center, TextFx::Outline, t.ink);
            if (oppLeft_) g.text(L.tr("battle.oppleft"), cx, py + 50, 1, t.dim, Align::Center);
            else if (toppedOut_) g.text(L.tr("battle.toppedout"), cx, py + 50, 1, t.dim, Align::Center);
            else if (opp_->toppedOut()) g.text(L.tr("battle.oppout"), cx, py + 50, 1, t.dim, Align::Center);
        }
        // Two columns: you and the opponent, with the biggest droplet each reached.
        const float lx = px + 56, rx = px + pw - 56;
        g.rect(cx, py + 74, 1, 96, t.panelLo);
        g.text("VS", cx, py + 122, 1, t.dim, Align::Center);
        auto column = [&](float x, const std::string& name, int score, int level, bool winner) {
            g.text(name, x, py + 80, 1, t.dim, Align::Center);
            g.number(std::to_string(score), x, py + 108, 2, winner ? t.accent : t.text, Align::Center, TextFx::Outline, t.ink);
            g.disc(x, py + 142, 11, t.ink);
            g.disc(x, py + 142, 10, t.blob[std::min(std::max(level, 0), 11)]);
            g.number(std::to_string(1 << std::min(std::max(level, 0), 11)), x, py + 142, 1, t.label, Align::Center);
            if (winner) g.icon(IconId::Crown, x - 5.f, py + 160.f, t.accent);
        };
        const bool mineWins = resultSet_ && result_ == Result::Win, theirsWin = resultSet_ && result_ == Result::Loss;
        column(lx, L.tr("battle.you"), score_, maxLevel_, mineWins);
        column(rx, opp_->name(), opp_->score(), opp_->maxLevel(), theirsWin);
        g.rect(px + 14, py + 186, pw - 28, 1, t.panelLo);
        g.text(L.tr(modName(mod_)), cx, py + 200, 1, t.dim, Align::Center);
        float y = py + 222;
        if (resultSet_) {
            if (!opp_->isLan()) {
                if (ui.button(L.tr("battle.rematch"), px + 20, y, pw - 40, 26, true, true, BtnStyle::Primary)) rematch(c);
                y += 34;
            }
            if (ui.button(L.tr("battle.menu"), px + 20, y, pw - 40, 26, true, true, opp_->isLan() ? BtnStyle::Primary : BtnStyle::Normal)) c.go(SceneId::Battle);
            y += 34;
            if (ui.button(L.tr("over.menu"), px + 20, y, pw - 40, 26) || c.in.esc) c.go(SceneId::Menu);
        } else {
            if (ui.button(L.tr("pause.quit"), px + 20, y + 20, pw - 40, 26)) c.go(SceneId::Battle);
        }
    }

    static const char* modName(int m) {
        static const char* k[5] = {"mod.none", "mod.lowg", "mod.heavy", "mod.thick", "mod.bonus"};
        return k[std::min(std::max(m, 0), 4)];
    }

    // ----- effects -----
    void updateFx(float dt) {
        for (auto& p : parts_) {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.vy += 260.f * dt;
            p.life -= dt;
        }
        parts_.erase(std::remove_if(parts_.begin(), parts_.end(), [](const Particle& p) { return p.life <= 0; }), parts_.end());
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

    void drawFx(Ctx& c) {
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

    // ----- drawing -----
    float landingY(float x, int level) const {
        const float r = World::radiusFor(level);
        return std::min(float(cfg::kFloor), world_.surfaceY(x, r * 0.7f)) - r * 0.55f;
    }

    // Slow rising bubbles inside the tank for liquid ambience.
    void drawBubbles(Ctx& c) {
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

    void drawGuide(Ctx& c) {
        if (st_ == St::Over || !c.save.guide) return;
        const int lim = mode_->dropLimit();
        if (lim >= 0 && drops_ >= lim) return;
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        const float r = World::radiusFor(cur_), ly = landingY(holdX_, cur_);
        for (float y = cfg::kSpawnY + r + 3.f; y < ly - r - 2.f; y += 5.f) g.rect(std::floor(holdX_), std::floor(y), 1, 2, mix(t.dim, t.tank, 0.25f));
        g.ring(holdX_, ly, int(r), mix(t.dim, t.tank, 0.2f), 2);
    }

    static BlobSprite popped(BlobSprite s, float pop) {
        s.ra *= 1.f + pop;
        s.rb *= 1.f + pop;
        return s;
    }

    void drawBlobs(Ctx& c, int sx, int sy) {
        const float alpha = clampv(acc_ / cfg::kStep, 0.f, 1.f);
        const bool spec = viewing();
        const World& W = spec ? opp_->world() : world_;
        std::vector<BlobSprite> pile, labels;
        W.particleSprites(spec ? opp_->viewAlpha() : (st_ == St::Playing ? alpha : 1.f), kParticleDraw, pile);
        W.labelSprites(labels);
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
        std::vector<BlobSprite> nxt{popped(plainSprite(c.ui.mx(8, 44) + 22.f, 36.f, next_), nextPop_.x)};
        nxt[0].ra = nxt[0].rb = std::min(nxt[0].ra, 13.f);
        c.raster.drawField(nxt, *c.theme);
        c.raster.drawLabels(nxt, *c.theme);
        c.gfx.raster(c.raster);
    }

    void drawNextPanel(Ctx& c) {
        const Theme& t = *c.theme;
        const float nx = c.ui.mx(8, 44);
        c.ui.panel(nx, 8, 44, 44);
        c.gfx.text(c.loc.tr("hud.next"), nx + 22, 15, 1, t.dim, Align::Center);
    }

    // Share card: the final pot with a caption, saved as a PNG next to the save file (the overlay is hidden for it).
    void captureCard(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        c.ui.panel(35, 190, 200, 70);
        g.text(L.tr(mode_->nameKey()), cfg::kW / 2.f, 206, 1, t.accent, Align::Center);
        g.number(std::to_string(score_), cfg::kW / 2.f, 230, 2, t.text, Align::Center, TextFx::Outline, t.ink);
        g.text("LIQ2048 V" LIQ_VERSION, cfg::kW / 2.f, 250, 1, t.dim, Align::Center);
        const bool ok = g.capturePng(platform::prefPath() + "liq2048-result.png");
        toast_ = L.tr(ok ? "card.saved" : "extras.fail");
        toastT_ = 2.5f;
    }

    bool ghostMode() const {
        return !hot_ && mod_ == 0 && (mid_ == ModeId::Classic || mid_ == ModeId::Timed || mid_ == ModeId::Rain || mid_ == ModeId::Tide ||
                             mid_ == ModeId::Zen);
    }

    void drawHud(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Ui& ui = c.ui;
        Loc& L = c.loc;
        const bool play = st_ == St::Playing;

        // Score (counts up) + best. In Battle: the opponent's score under yours (or yours under theirs while watching).
        const bool vw = viewing();
        g.number(std::to_string(vw ? opp_->score() : int(std::lround(dispScore_))), cfg::kW / 2.f, 24, 3, vw ? t.accent : t.text, Align::Center,
                 TextFx::Outline, t.ink);
        if (battle_) {
            const std::string line = vw ? L.tr("battle.you") + " " + std::to_string(score_) : "VS " + opp_->name() + " " + std::to_string(opp_->score());
            const bool behind = !vw && opp_->score() > score_;
            g.text(line, cfg::kW / 2.f, 48, 1, behind ? t.danger : t.dim, Align::Center);
        } else if (hot_) {  // two scores, the player on turn highlighted
            for (int p = 0; p < 2; ++p) {
                const bool on = turn_ == p && st_ == St::Playing;
                const float x = p ? 190.f : 80.f;
                g.text((on ? "> " : "") + L.tr(p ? "hot.p2" : "hot.p1"), x, 48, 1, on ? t.accent : t.dim, Align::Center);
                g.number(std::to_string(hs_[p]), x, 60, 1, on ? t.text : t.dim, Align::Center);
            }
        } else if (mid_ != ModeId::Puzzle) {
            std::string best = L.tr("hud.best") + " " + std::to_string(std::max(bestFor(c), score_));
            const std::vector<int>& gh = c.save.ghost[int(mid_)];
            if (ghostMode() && !replaying_ && !gh.empty() && st_ != St::Over) {  // pace against your best run
                const int d = score_ - gh[std::min(size_t(drops_), gh.size() - 1)];
                best += "  " + L.tr("hud.ghost") + " " + (d >= 0 ? "+" : "-") + std::to_string(std::abs(d));
            }
            const float bw = float(g.textW(best, 1));
            g.icon(IconId::Crown, cfg::kW / 2.f - bw / 2.f - 13.f, 45.f, t.accent);
            g.text(best, cfg::kW / 2.f + 5.f, 48, 1, t.dim, Align::Center);
        }

        // Pause.
        const float px = ui.mx(238, 24);
        if (ui.iconButton(IconId::Pause, px, 8, 24, 24, play, false)) {
            st_ = St::Paused;
            ui.resetFocus();
        }

        // Undo (Classic and Zen): icon plus remaining charges.
        if (undoAllowed()) {
            const float ux = ui.mx(8, 44);
            const bool can = play && snap_.valid && undos_ > 0;
            if (ui.button("", ux, 56, 44, 22, can, can)) doUndo(c);
            g.icon(IconId::Undo, ux + 8, 64.f, can ? t.text : t.dim);
            g.number(std::to_string(undos_), ux + 32.f, 67, 1, can ? t.text : t.dim, Align::Center);
        }

        // Mode, status, segmented progress bar.
        const Stats s = stats();
        if (vw) g.text(opp_->name() + " - " + L.tr("battle.viewing"), cfg::kW / 2.f, 64, 1, t.danger, Align::Center);
        else g.text(L.tr(mode_->nameKey()), cfg::kW / 2.f, 64, 1, t.accent, Align::Center);
        const std::string status = mode_->status(L, s);
        if (!status.empty()) g.text(status, cfg::kW / 2.f, 77, 1, t.text, Align::Center);
        const float pg = mode_->progress(s);
        if (pg >= 0.f) {
            const int segs = 20, on = int(std::lround(clampv(pg, 0.f, 1.f) * float(segs)));
            const float x0 = (cfg::kW - segs * 6.f + 1.f) / 2.f;
            for (int i = 0; i < segs; ++i) g.rect(x0 + float(i) * 6.f, 86, 5, 4, i < on ? t.accent : t.panelLo);
        }
    }

    // Bottom strip: droplet levels reached so far.
    void drawHistory(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        for (int i = 0; i < 12; ++i) {
            const float x = cfg::kW / 2.f + (float(i) - 5.5f) * 19.f;
            const bool got = i <= 3 || i <= maxLevel_;
            if (got) {
                g.disc(x, 468, 4, t.ink);
                g.disc(x, 468, 3, t.blob[i]);
            } else {
                g.ring(x, 468, 3, t.panelLo);
            }
        }
        if (maxLevel_ > 3) g.icon(IconId::Marker, cfg::kW / 2.f + (float(maxLevel_) - 5.5f) * 19.f - 2.f, 474.f, t.accent);
    }

    void drawHint(Ctx& c, bool paused) {
        if (hint_.empty() || st_ == St::Tutorial || st_ == St::Over) return;
        if (hintT_ > 0.f && !paused) {
            hintT_ -= 1.f / 60.f;
            if (hintT_ <= 0.f) {
                hint_.clear();
                return;
            }
        }
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        const float w = 196.f;
        const auto lines = g.wrap(hint_, w - 16.f, 1);
        const float h = 12.f + float(lines.size()) * float(g.lineH(1));
        const float x = (cfg::kW - w) / 2.f, y = 190.f;
        g.panel(x, y, w, h, t.panel, t.panelHi, t.panelLo, t.accent);
        for (size_t i = 0; i < lines.size(); ++i) g.text(lines[i], cfg::kW / 2.f, y + 7.f + float(i) * float(g.lineH(1)) + 3.f, 1, t.text, Align::Center);
    }

    void drawToast(Ctx& c) {
        if (toastT_ <= 0.f) return;
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        const float slide = std::min(1.f, std::min(toastT_, 3.f - toastT_) * 5.f);
        const float y = std::floor(-26.f + (104.f + 26.f) * slide);
        const float w = std::max(120.f, float(g.textW(toast_, 1)) + 44.f);
        const float x = (cfg::kW - w) / 2.f;
        g.panel(x, y, w, 22, t.panel, t.panelHi, t.panelLo, t.accent);
        g.icon(IconId::Check, x + 8, y + 8, t.accent);
        g.text(toast_, x + 22, y + 11, 1, t.text, Align::Left);
    }

    void tutorialDemo(Ctx& c, float cx, float cy) {
        const Theme& t = *c.theme;
        Gfx& g = c.gfx;
        const float tm = c.time;
        std::vector<BlobSprite> sp;
        if (tutPage_ == 0) {  // drag and drop
            const float x = cx + std::sin(tm * 1.6f) * 50.f;
            sp.push_back(plainSprite(x, cy - 12.f, 1, 0.05f, 1.57f));
            for (float y = cy + 2.f; y < cy + 26.f; y += 5.f) g.rect(std::floor(x), std::floor(y), 1, 2, t.dim);
            g.ring(x, cy + 34.f, 10, t.dim, 2);
        } else if (tutPage_ == 1) {  // merge
            const float p = std::fmod(tm, 2.4f) / 2.4f;
            if (p < 0.55f) {
                const float d = 24.f * (1.f - p / 0.55f) + 9.5f;
                sp.push_back(plainSprite(cx - d, cy, 1));
                sp.push_back(plainSprite(cx + d, cy, 1));
            } else {
                const float k = (p - 0.55f) / 0.45f;
                sp.push_back(plainSprite(cx, cy, 2, 0.15f * std::max(0.f, 1.f - k * 2.f), 0.f));
            }
        } else {  // keep below the line
            for (float x = cx - 60.f; x < cx + 60.f; x += 8.f) g.rect(x, cy - 12.f, 5, 1, t.danger);
            sp.push_back(plainSprite(cx - 18.f, cy + 10.f, 3));
            sp.push_back(plainSprite(cx + 14.f, cy + 14.f, 2));
            if (int(tm * 2.f) & 1) g.icon(IconId::Cross, cx - 3.f, cy - 30.f, t.danger);
        }
        c.raster.clear();
        c.raster.drawField(sp, t);
        c.raster.drawLabels(sp, t);
        g.raster(c.raster);
    }

    void tutorial(Ctx& c) {
        Gfx& g = c.gfx;
        Ui& ui = c.ui;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        g.dim(t, 1);
        ui.panel(20, 118, 230, 262);
        g.text(L.tr("tut.title"), cfg::kW / 2.f, 140, 2, t.accent, Align::Center, TextFx::Outline, t.ink);
        // Demo box (drawn first so the droplets sit on top of its backing).
        g.rect(34, 160, 202, 82, t.tank);
        g.border(34, 160, 202, 82, 1, t.ink);
        tutorialDemo(c, cfg::kW / 2.f, 201.f);
        static const char* keys[3] = {"tut.1", "tut.2", "tut.3"};
        ui.paragraph(L.tr(keys[tutPage_]), 34, 262, 202, 1, t.text, Align::Left);
        for (int i = 0; i < 3; ++i) g.rect(123.f + float(i) * 10.f, 336, 6, 6, i == tutPage_ ? t.accent : t.panelLo);
        const bool last = tutPage_ == 2;
        if (ui.button(L.tr(last ? "tut.start" : "tut.next"), ui.mx(138, 92), 348, 92, 22, true, true, BtnStyle::Primary)) {
            if (last) endTutorial(c);
            else ++tutPage_;
        }
        if (ui.button(L.tr("tut.skip"), ui.mx(40, 92), 348, 92, 22) || c.in.esc) endTutorial(c);
    }

    void endTutorial(Ctx& c) {
        c.save.tutorialDone = true;
        c.save.store();
        st_ = St::Playing;
        c.in.esc = false;
        coach_ = 0;
        setHint(c.loc.tr("coach.1"), -1.f);
    }

    void pauseMenu(Ctx& c) {
        Gfx& g = c.gfx;
        Ui& ui = c.ui;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        g.dim(t, 1);
        const bool canCopy = !battle_ && !rec_.empty();
        const float ph = battle_ ? 138.f : (canCopy && !replaying_) ? 198.f : 168.f;
        ui.panel(55, 170, 160, ph);
        g.text(L.tr("pause.title"), cfg::kW / 2.f, 192, 2, t.accent, Align::Center, TextFx::Outline, t.ink);
        if (!platform::isMobile()) g.text("ESC  SPACE  U  F11", cfg::kW / 2.f, 170 + ph + 10, 1, t.dim, Align::Center);
        if (ui.button(L.tr("pause.resume"), 70, 214, 130, 24, true, true, BtnStyle::Primary) || c.in.esc) st_ = St::Playing;
        float by = 244;
        if (!battle_ && !replaying_) {
            if (ui.button(L.tr("pause.restart"), 70, by, 130, 24)) {
                commit(c);
                start(c);
                st_ = St::Playing;
            }
            by += 30;
        }
        if (canCopy) {  // the run so far, from the first drop to this moment
            if (ui.button(L.tr("pause.copycode"), 70, by, 130, 24)) {
                Replay part = rec_;
                if (!replaying_) {
                    part.endStep = simStep_;
                    part.outcome = 0;
                }
                SDL_SetClipboardText(part.toCode().c_str());
                toast_ = L.tr("over.copied");
                toastT_ = 2.f;
            }
            by += 30;
        }
        if (ui.button(L.tr("pause.settings"), 70, by, 130, 24)) st_ = St::Settings;
        by += 30;
        if (ui.button(L.tr("pause.quit"), 70, by, 130, 24)) {
            if (battle_) {  // leaving a LAN match counts as a forfeit
                if (opp_->isLan() && !resultSet_) {
                    ++c.save.battleLosses;
                    resultSet_ = true;
                }
                c.save.store();
                c.go(SceneId::Battle);
            } else {
                commit(c);
                c.go(mid_ == ModeId::Puzzle ? SceneId::Puzzle : SceneId::Menu);
            }
        }
    }

    void puzzleOverlay(Ctx& c, float oy) {
        Gfx& g = c.gfx;
        Ui& ui = c.ui;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        const PuzzleDef* pz = mode_->puzzle();
        const bool won = outcome_ == Outcome::Won;
        g.text(L.tr(won ? "over.solved" : "over.outofdrops"), cfg::kW / 2.f, 164 + oy, 2, won ? t.accent : t.danger, Align::Center,
               TextFx::Outline, t.ink);
        g.text(L.fmt("puzzle.level", std::to_string(c.puzzleLevel + 1)), cfg::kW / 2.f, 190 + oy, 1, t.dim, Align::Center);
        for (int i = 0; i < 3; ++i) {  // stars drawn as crowns, lit one after another
            const bool on = won && i < stars_ && overT_ > 0.25f + 0.25f * float(i);
            g.icon(IconId::Crown, cfg::kW / 2.f - 36.f + float(i) * 26.f, 206.f + oy, on ? t.accent : t.panelLo, 2);
        }
        g.text(L.tr("hud.drops") + " " + std::to_string(drops_) + "  " + L.fmt("puzzle.par", std::to_string(pz->par)), cfg::kW / 2.f,
               236.f + oy, 1, t.text, Align::Center);
        if (replaying_) {  // watching a code: only watch again or leave
            if (ui.button(L.tr("over.replay"), 55, 270 + oy, 160, 24, true, true, BtnStyle::Primary)) {
                start(c);
                st_ = St::Playing;
            }
            if (ui.button(L.tr("over.menu"), 55, 300 + oy, 160, 24) || c.in.esc) c.go(SceneId::Menu);
            return;
        }
        const bool hasNext = won && c.puzzleLevel + 1 < int(puzzlePack().size());
        float y = 258.f + oy;
        if (hasNext) {
            if (ui.button(L.tr("over.next"), 55, y, 160, 24, true, true, BtnStyle::Primary)) {
                ++c.puzzleLevel;
                start(c);
                st_ = St::Playing;
            }
            y += 30.f;
        }
        if (ui.button(L.tr(won ? "over.retry" : "over.restart"), 55, y, 160, 24, true, true, hasNext ? BtnStyle::Normal : BtnStyle::Primary)) {
            start(c);
            st_ = St::Playing;
        }
        y += 30.f;
        if (ui.button(L.tr("over.levels"), 55, y, 160, 24) || c.in.esc) c.go(SceneId::Puzzle);
        y += 30.f;
        if (!rec_.empty()) {  // share or rewatch the solution
            if (ui.button(L.tr("over.copycode"), 55, y, 78, 20)) {
                SDL_SetClipboardText(rec_.toCode().c_str());
                toast_ = L.tr("over.copied");
                toastT_ = 2.f;
            }
            if (ui.button(L.tr("over.replay"), 137, y, 78, 20)) {
                replaying_ = true;
                start(c);
                st_ = St::Playing;
            }
        }
    }

    void overlay(Ctx& c) {
        if (mid_ == ModeId::Battle) {
            battleOverlay(c);
            return;
        }
        Gfx& g = c.gfx;
        Ui& ui = c.ui;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        g.dim(t, 0);
        const float slide = std::min(1.f, overT_ * 4.f);
        const float oy = std::floor((1.f - slide) * -60.f);
        ui.panel(35, 140 + oy, 200, mid_ == ModeId::Puzzle || hot_ ? 248 : 272);
        if (mid_ == ModeId::Puzzle) {
            puzzleOverlay(c, oy);
            return;
        }
        if (hot_) {
            const bool draw = hs_[0] == hs_[1];
            const int w = hs_[0] > hs_[1] ? 0 : 1;
            g.text(draw ? L.tr("hot.draw") : L.fmt("hot.wins", L.tr(w ? "hot.p2" : "hot.p1")), cfg::kW / 2.f, 170 + oy, 1, t.accent, Align::Center,
                   TextFx::Outline, t.ink);
            for (int p = 0; p < 2; ++p) {
                const float y = 206.f + float(p) * 34.f + float(oy);
                g.text(L.tr(p ? "hot.p2" : "hot.p1"), 52, y, 1, t.dim, Align::Left);
                g.number(std::to_string(hs_[p]), 218, y, 2, !draw && w == p ? t.accent : t.text, Align::Right, TextFx::Outline, t.ink);
            }
            if (ui.button(L.tr("battle.rematch"), 55, 290 + oy, 160, 24, true, true, BtnStyle::Primary)) {
                start(c);
                st_ = St::Playing;
            }
            if (ui.button(L.tr("over.menu"), 55, 322 + oy, 160, 22) || c.in.esc) c.go(SceneId::Menu);
            return;
        }
        const char* key = outcome_ == Outcome::Won ? "over.won" : outcome_ == Outcome::Lost ? "over.lost" : "over.finished";
        g.text(L.tr(key), cfg::kW / 2.f, 164 + oy, 2, outcome_ == Outcome::Lost ? t.danger : t.accent, Align::Center,
               TextFx::Outline, t.ink);
        g.text(L.tr("over.score"), cfg::kW / 2.f, 192 + oy, 1, t.dim, Align::Center);
        const float k = clampv(overT_ / 0.8f, 0.f, 1.f);
        g.number(std::to_string(int(std::lround(float(score_) * (1.f - (1.f - k) * (1.f - k))))), cfg::kW / 2.f, 214 + oy, 3, t.text,
                 Align::Center, TextFx::Outline, t.ink);
        g.text(L.tr("hud.best") + " " + std::to_string(bestFor(c)), cfg::kW / 2.f, 240 + oy, 1, t.dim, Align::Center);
        if (newBest_ && (int(overT_ * 3.f) % 2 == 0 || overT_ > 3.f)) {
            const float w = float(g.textW(L.tr("over.newbest"), 1));
            g.text(L.tr("over.newbest"), cfg::kW / 2.f, 260 + oy, 1, t.accent, Align::Center);
            g.icon(IconId::Crown, cfg::kW / 2.f - w / 2.f - 14.f, 257.f + oy, t.accent);
            g.icon(IconId::Crown, cfg::kW / 2.f + w / 2.f + 5.f, 257.f + oy, t.accent);
        }
        const bool canReplay = !rec_.empty();
        if (canReplay) {
            if (ui.button(L.tr("over.copycode"), 55, 360 + oy, 78, 20)) {
                SDL_SetClipboardText(rec_.toCode().c_str());
                toast_ = L.tr("over.copied");
                toastT_ = 2.f;
            }
            if (ui.button(L.tr("over.copyres"), 137, 360 + oy, 78, 20)) {
                static const char* names[kModeCount] = {"mode.classic", "mode.timed", "mode.target", "mode.daily", "mode.zen",
                                                        "mode.rain", "mode.tide", "mode.weekly", "mode.puzzle", "mode.battle"};
                SDL_SetClipboardText(("Liq2048 V" LIQ_VERSION " - " + L.tr(names[int(mid_)]) + ": " + std::to_string(score_) + " pts, " +
                                      std::to_string(drops_) + " drops, chain x" + std::to_string(runChain_)).c_str());
                toast_ = L.tr("over.copied");
                toastT_ = 2.f;
            }
        }
        if (!replaying_ && ui.button(L.tr("card.shareimg"), 55, 384 + oy, 160, 20)) shotPending_ = true;
        if (ui.button(L.tr("over.restart"), 55, 280 + oy, 160, 24, true, true, BtnStyle::Primary)) {
            replaying_ = false;
            start(c);
            st_ = St::Playing;
        }
        if (canReplay && ui.button(L.tr("over.replay"), 55, 308 + oy, 160, 22)) {
            replaying_ = true;
            start(c);
            st_ = St::Playing;
        }
        if (ui.button(L.tr("over.menu"), 55, 334 + oy, 160, 22) || c.in.esc) c.go(SceneId::Menu);
    }
};

}  // namespace

std::unique_ptr<Scene> makeGameScene(Ctx& c) { return std::make_unique<GameScene>(c); }

}  // namespace liq
