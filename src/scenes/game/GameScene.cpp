// GameScene core: construction, per-frame orchestration and run (re)start.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "scenes/game/GameScene.hpp"

namespace liq {

ModeId GameScene::takeMode(Ctx& c) {  // a pasted run code decides the mode
    if (c.hasPendingReplay) c.mode = ModeId(std::min(c.pendingReplay.mode, kModeCount - 1));
    return c.mode;
}

GameScene::GameScene(Ctx& c) : mid_(takeMode(c)) {
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

GameScene::~GameScene() {
    if (opp_) opp_->leave();  // closes a LAN link politely
}

void GameScene::frame(Ctx& c, float dt) {
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


void GameScene::start(Ctx& c) {
    mode_ = mid_ == ModeId::Battle ? makeBattleMode(bcfg_.seed, bcfg_.duration, bcfg_.modifier) : makeMode(mid_, c.save, today_, c.puzzleLevel);
    mod_ = mode_->fixedModifier() >= 0 ? mode_->fixedModifier()
           : (mid_ == ModeId::Daily || mid_ == ModeId::Puzzle) ? 0 : c.save.modifier;
    hs_[0] = hs_[1] = 0;
    turn_ = lastDropper_ = 0;
    world_.clear();
    // Daily, Puzzle and Weekly are always Water so everyone plays the same physics; Battle uses the host's liquid.
    liquid_ = battle_ ? bcfg_.liquid : (mid_ == ModeId::Daily || mid_ == ModeId::Puzzle || mid_ == ModeId::Weekly) ? 0 : c.save.liquid;
    applyLiquid(world_, liquid_, mod_ == kModThick);
    // Daily and Puzzle must play identically for everyone, so they always use the reference quality.
    world_.substeps = (mid_ == ModeId::Daily || mid_ == ModeId::Puzzle || mid_ == ModeId::Battle) ? 2 : c.save.quality + 1;
    if (replaying_) {
        mod_ = rec_.modifier;
        liquid_ = rec_.liquid;
        applyLiquid(world_, liquid_, mod_ == kModThick);
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
    madRng_.seed(seed ^ 0x9e3779b9u);
    peeks_ = 3;
    peekT_ = madFlash_ = 0.f;
    simStep_ = 0;
    repIdx_ = 0;
    if (!replaying_) {
        rec_.clear();
        rec_.mode = int(mid_);
        rec_.modifier = mod_;
        rec_.liquid = liquid_;
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
void GameScene::introHint(Ctx& c) {
    if (mid_ == ModeId::Battle) return;
    static const char* desc[kModeCount] = {"mode.classic.desc", "mode.timed.desc", "mode.target.desc", "mode.daily.desc",
                                           "mode.zen.desc", "mode.rain.desc", "mode.tide.desc", "mode.weekly.desc", "mode.puzzle.desc", "mode.battle.desc",
                                           "mode.mirror.desc", "mode.bigbox.desc", "mode.madhouse.desc", "mode.blind.desc"};
    const uint32_t bit = 1u << int(mid_);
    std::string s;
    if (!(c.save.seenModes & bit) && st_ != St::Tutorial) {
        c.save.seenModes |= bit;
        s = c.loc.tr(desc[int(mid_)]);
    }
    if (mid_ == ModeId::Weekly) s = (s.empty() ? "" : s + " ") + c.loc.fmt("mod.rule", c.loc.tr(modifierKey(mod_)));
    else if (mid_ == ModeId::Puzzle) {
        const PuzzleDef* pz = mode_->puzzle();
        s = c.loc.fmt("puzzle.goal", std::to_string(1 << pz->goal)) + " " + c.loc.fmt("puzzle.par", std::to_string(pz->par));
    }
    if (!s.empty()) setHint(s, 5.f);
}

Stats GameScene::stats() const {
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

int GameScene::bestFor(const Ctx& c) const {
    return mid_ == ModeId::Daily ? c.save.dailyToday : mid_ == ModeId::Weekly ? c.save.weeklyBest : c.save.best[int(mid_)];
}

std::unique_ptr<Scene> makeGameScene(Ctx& c) { return std::make_unique<GameScene>(c); }

}  // namespace liq
