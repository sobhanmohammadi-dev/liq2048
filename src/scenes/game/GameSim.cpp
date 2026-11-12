// GameScene simulation: drops, merges, undo, the fixed-step loop, scoring and run completion.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "scenes/game/GameScene.hpp"

namespace liq {

void GameScene::saveSnap() {
    if (snaps_.size() >= kUndoDepth) snaps_.erase(snaps_.begin());
    snaps_.emplace_back();
    Snap& s = snaps_.back();
    s.world = world_;
    s.rng = rng_;
    s.score = score_;
    s.drops = drops_;
    s.cur = cur_;
    s.next = next_;
    s.maxLevel = maxLevel_;
    s.elapsed = elapsed_;
    s.quiet = quiet_;
    s.simStep = simStep_;
    s.relief = tideRelief_;
    s.combo = combo_;
    s.comboT = comboT_;
    s.deadY = deadY_;
}

void GameScene::doUndo(Ctx& c) {
    if (!undoAllowed() || replaying_ || snaps_.empty() || undos_ <= 0 || st_ != St::Playing) return;
    // Back to before the last combination; with no merge in the history, just the last drop.
    size_t i = snaps_.size() - 1;
    for (size_t k = snaps_.size(); k-- > 0;)
        if (snaps_[k].merged) {
            i = k;
            break;
        }
    const Snap& snap_ = snaps_[i];
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
    tideRelief_ = snap_.relief;
    combo_ = snap_.combo;
    comboT_ = snap_.comboT;
    deadY_ = snap_.deadY;
    rec_.truncateFrom(simStep_);
    snaps_.resize(i);
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

int GameScene::roll() {
    if (mode_) {
        const int q = mode_->queueLevel(queueIdx_++);
        if (q >= 0) return q;
        if (mode_->puzzle()) return 0;  // queue used up
    }
    if (mid_ == ModeId::Battle) return battle::battleLevel(bcfg_.seed, queueIdx_++, mod_);
    const int r = rng_.range(0, 99);
    if (mod_ == kModBonus) return r < 15 ? 1 : r < 45 ? 2 : r < 80 ? 3 : 4;
    if (mid_ == ModeId::Zen) return r < 30 ? 0 : r < 60 ? 1 : r < 85 ? 2 : 3;  // Zen stays relaxed, but not trivial
    // Everyone else gets bigger drops the longer a run lasts (cumulative weights for levels 0..5).
    static const int w[6][7] = {{26, 52, 76, 92, 100, 100, 100}, {20, 42, 66, 86, 100, 100, 100}, {14, 32, 54, 78, 94, 100, 100},
                                {10, 24, 44, 68, 86, 98, 100}, {7, 18, 36, 58, 78, 94, 100}, {5, 13, 28, 48, 70, 90, 100}};
    const int tier = std::min(5, drops_ / rampDiv());
    int lvl = 0;
    while (lvl < 6 && r >= w[tier][lvl]) ++lvl;
    return lvl;
}

void GameScene::tryDrop(Ctx& c, bool force, int levelOverride) {
    const int lim = mode_->dropLimit();
    if ((cooldown_ > 0 && !force) || (lim >= 0 && drops_ >= lim)) return;
    if (undoAllowed()) saveSnap();  // history for undo
    const int lvl = levelOverride >= 0 ? levelOverride : cur_;
    aimX_ = quantX(aimX_);  // run codes store positions in 1/128 px
    if (!replaying_) rec_.drops.push_back({simStep_, aimX_, lvl});
    if (!replaying_ && trail_.size() < size_t(Save::kGhostMax)) trail_.push_back(score_);
    world_.spawn(aimX_, cfg::kSpawnY, lvl, mod_ == kModMix ? mixLiquidOf(rec_.seed, drops_) : 0);
    ++drops_;
    cur_ = next_;
    next_ = roll();
    cooldown_ = cfg::kDropCooldown;
    quiet_ = 0;
    stashDue_ = !replaying_;
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

void GameScene::grant(Ctx& c, int id) {
    if (replaying_) return;
    if (c.save.unlock(id)) {
        toast_ = c.loc.tr(achKey(id));
        toastT_ = 3.0f;
        c.audio.play(Sfx::Unlock);
    }
}

void GameScene::onMerge(Ctx& c, const MergeEvent& e) {
    const Theme& t = *c.theme;
    const int value = 1 << e.level;
    ++runMerges_;
    runChain_ = std::max(runChain_, e.chain);
    tideRelief_ = std::min(cfg::kTideReliefCap, tideRelief_ + 2.f + 1.f * float(e.level));
    if (coach_ == 1) {
        coach_ = 2;
        setHint(c.loc.tr("coach.3"), 3.5f);
    }
    if (e.chain >= 7) grant(c, kChain7);
    ++combo_;
    comboT_ = kComboWindow;
    const int gain = value * e.chain * comboMult();
    score_ += gain;
    if (hot_) hs_[lastDropper_] += gain;
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
                      "+" + std::to_string(gain), e.chain >= 3 ? t.accent : t.text, e.chain >= 3 ? 2 : 1});
    buzz(c, 0.35f + 0.05f * float(std::min(e.level, 8)) + 0.1f * float(std::min(e.chain, 4)), 18 + 6 * std::min(e.level, 8) + 15 * std::min(e.chain - 1, 3));
    if (e.level >= 8 || e.chain >= 4) slowT_ = 0.45f;
    if (!snaps_.empty()) snaps_.back().merged = true;
    if (e.chain >= 4 && undoAllowed() && undos_ < 2) {
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

void GameScene::onImpact(Ctx& c, const Impact& im) {
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
void GameScene::updateAudio(Ctx& c, float dt) {
    float top = float(cfg::kFloor);
    for (const auto& d : world_.drops)
        if (d.age > 1.f) top = std::min(top, d.top);
    float target = clampv((float(cfg::kFloor) - top) / (float(cfg::kFloor) - deadY_), 0.f, 1.f);
    if (chainT_ > 0.f) target = std::min(1.f, target + 0.25f);
    if (st_ != St::Playing) target *= 0.3f;
    tension_ += (target - tension_) * std::min(1.f, dt * 1.5f);
    c.audio.setIntensity(tension_);
}

void GameScene::simulate(Ctx& c, float dt, bool locked) {
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
    if (static const bool dev = std::getenv("LIQ_AUTODROP") != nullptr; dev && !replaying_ && manualOk() && simStep_ % 50 == 0) {  // dev: scripted play for screenshots
            aimX_ = holdX_ = 40.f + 170.f * (0.5f + 0.5f * std::sin(float(simStep_) * 0.021f));
            tryDrop(c);
        }
        if (want && manualOk()) tryDrop(c);
    if (const float iv = autoInterval(); iv > 0.f && !replaying_) {
        autoT_ += dt;
        autoFill_ = std::min(1.f, autoT_ / iv);
        if (autoT_ >= iv) {
            autoT_ = 0.f;
            tryDrop(c, true);
        }
    }
    if (rainOn() && elapsed_ >= 60.f) grant(c, kRain60);
    if (tideOn() && elapsed_ >= 90.f) grant(c, kTide90);

    const bool frozen = replaying_ && replayPaused_;
    if (!frozen) acc_ = std::min(acc_ + dt * float(replaying_ ? replaySpeed_ : 1), cfg::kStep * 3.f * float(replaying_ ? replaySpeed_ : 1));  // never catch up more than 3 steps (slow phones)
    // Performance governor: on a device that cannot hold ~50 fps, halve the liquid substeps in modes where
    // the physics need not be identical for everyone.
    if (!replaying_ && mid_ != ModeId::Daily && mid_ != ModeId::Puzzle && mid_ != ModeId::Battle) {
        perfEma_ += (dt - perfEma_) * 0.04f;
        const bool ph = platform::isMobile();  // phones react earlier: a steady 50 fps beats a stuttering 60
        if (!perfLow_ && perfEma_ > (ph ? 0.0205f : 0.024f)) perfLow_ = true;
        if (perfLow_ && perfEma_ < (ph ? 0.0172f : 0.017f)) perfLow_ = false;
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
        if (mid_ == ModeId::Madhouse && simStep_ > 0 && simStep_ % kMadSteps == 0) madShuffle(c);
        world_.step(cfg::kStep, gravityNow(), modifierSideways(mod_, simStep_));
        ++simStep_;
        if (comboT_ > 0.f && (comboT_ -= cfg::kStep) <= 0.f) combo_ = 0, comboT_ = 0.f;
        acc_ -= cfg::kStep;
        for (const auto& e : world_.events) onMerge(c, e);
        world_.events.clear();
        for (const auto& im : world_.impacts) onImpact(c, im);
        world_.impacts.clear();
        if (tideOn()) {
            const float shift = mid_ == ModeId::Tide ? mode_->deadLineShift(float(simStep_) * cfg::kStep) : tideShift(float(simStep_) * cfg::kStep);
            tideRelief_ = std::min(tideRelief_, shift);
            // The line keeps moving all game (see Tide::deadLineShift); merges lift it, but never above its start.
            const float target = std::min(cfg::kDeadY + shift - tideRelief_, cfg::kFloor - 60.f);
            deadY_ += clampv(target - deadY_, -50.f * cfg::kStep, 50.f * cfg::kStep);
        }
        world_.updateOverflow(deadY_, cfg::kStep);
        if (!mode_->failable() && world_.overflow() > 1.5f) popOverflow(c);
        if (replaying_ && rec_.endStep && simStep_ >= rec_.endStep) break;
    }
    if (stashDue_ && !replaying_ && !rec_.empty() && simStep_ > rec_.drops.back().step) {
        stashDue_ = false;
        stashResume(c);
    }

    if (replaying_ && rec_.endStep) {  // a code that knows where the run ended plays exactly to that point
        if (simStep_ >= rec_.endStep && repIdx_ >= rec_.drops.size()) {
            if (resuming_) {
                finishResume(c);
                return;
            }
            finish(c, rec_.outcome == 1 ? Outcome::Won : rec_.outcome == 2 ? Outcome::Lost : Outcome::Finished);
            return;
        }
        return;
    }
    const Outcome o = mode_->evaluate(stats());
    if (o != Outcome::Running) finish(c, o);
    else if (replaying_ && repIdx_ >= rec_.drops.size() && ++replayTail_ > 60u * 4u) finish(c, Outcome::Finished);
}

// Zen has no fail state: liquid that settles above the line drains away harmlessly.
void GameScene::popOverflow(Ctx& c) {
    const Theme& t = *c.theme;
    for (const Drop& d : world_.popAbove(deadY_)) {
        const Color col = t.blob[std::min(d.level, 11)];
        for (int i = 0; i < 14; ++i)
            parts_.push_back({d.x, d.y, rngFx_.uni(-90.f, 90.f), rngFx_.uni(-90.f, 90.f), 0.5f, 0.5f, col, 2});
        rings_.push_back({d.x, d.y, 0.f, World::radiusFor(d.level), col});
    }
    world_.resetOverflow();
}

void GameScene::commit(Ctx& c) {
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
        const bool counts = (mod_ == 0 && liquid_ == 0) || mid_ == ModeId::Weekly;  // modifiers keep their own, unranked, runs
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

void GameScene::finish(Ctx& c, Outcome o) {
    outcome_ = o;
    if (!replaying_) c.save.resumeCode.clear();
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

// Madhouse: every 10 s about half of the drops (and the next ones) change number, size and colour by one step.
void GameScene::madShuffle(Ctx& c) {
    for (size_t i = 0; i < world_.drops.size(); ++i) {
        if (!world_.drops[i].n || madRng_.range(0, 99) >= 55) continue;
        const int lv = clampv(world_.drops[i].level + (madRng_.range(0, 1) ? 1 : -1), 0, 9);
        if (lv != world_.drops[i].level) world_.relevel(int(i), lv);
    }
    cur_ = clampv(cur_ + (madRng_.range(0, 1) ? 1 : -1), 0, 5);
    next_ = clampv(next_ + (madRng_.range(0, 1) ? 1 : -1), 0, 5);
    madFlash_ = 0.6f;
    if (!replaying_) {
        c.audio.play(Sfx::Swipe, 0.6f);
        shake_ = std::max(shake_, 3.f);
    }
}

}  // namespace liq
