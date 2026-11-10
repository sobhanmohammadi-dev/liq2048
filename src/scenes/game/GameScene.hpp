// Private header of the in-game scene. The class is implemented across several translation units:
//
//   GameScene.cpp     construction, frame orchestration, run (re)start
//   GameSim.cpp       drops, merges, undo, fixed-step loop, scoring, run completion
//   GameBattle.cpp    opponent link, countdown, result, Battle overlay
//   GameFx.cpp        sparks / rings / bubbles / guide line / liquid raster
//   GameHud.cpp       score header, buttons, history strip, hint banner, toast
//   GameOverlays.cpp  tutorial, pause menu, game-over and puzzle panels
//
// Nothing outside src/scenes/game/ may include this file; the scene is created through makeGameScene().
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "core/Util.hpp"
#include "game/Liquid.hpp"
#include "game/Mode.hpp"
#include "game/Modifier.hpp"
#include "game/Replay.hpp"
#include "game/World.hpp"
#include "scenes/Scenes.hpp"

namespace liq {

// A short-lived spark of the merge / splash effects.
struct FxSpark {
    float x, y, vx, vy, life, max;
    Color c;
    int size;
};
// Floating "+score" text.
struct FloatText {
    float x, y, life;
    std::string s;
    Color c;
    int scale;
};
// Expanding ring drawn where drops merge or pop.
struct PopRing {
    float x, y, age, r0;
    Color c;
};

class GameScene final : public Scene {
public:
    static ModeId takeMode(Ctx& c);  // a pasted run code decides the mode
    explicit GameScene(Ctx& c);
    ~GameScene() override;
    void frame(Ctx& c, float dt) override;

private:
    enum class St { Tutorial, Playing, Paused, Settings, Over };

    // ----- run state -----
    ModeId mid_;
    std::unique_ptr<Mode> mode_;
    World world_;
    Rng rng_, rngFx_;
    int liquid_ = 0;                   // liquid type of this run
    Rng madRng_;                       // Madhouse: its own stream, so the drop queue is untouched
    int peeks_ = 3;                    // I Don't Know: reveals left
    float peekT_ = 0.f, madFlash_ = 0.f;
    static constexpr uint32_t kMadSteps = uint32_t(cfg::kMadnessPeriod * 60.f + 0.5f);
    static constexpr int kBlindLevel = 7;  // the one colour every hidden drop is drawn in
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
    int mod_ = 0;                       // active modifier (see game/Modifier.hpp)
    // Harder than before (v1.6.1): the overflow line sits lower in the pot, so there is less room to stack.
    // Zen, Puzzle and Rising Tide (whose line moves) keep the original start position.
    float deadBase() const { return (mid_ == ModeId::Zen || mid_ == ModeId::Puzzle || tideOn()) ? cfg::kDeadY : cfg::kDeadY + 22.f; }
    float deadY_ = cfg::kDeadY, tideRelief_ = 0.f, autoT_ = 0.f, autoFill_ = 0.f;
    int queueIdx_ = 0, runMerges_ = 0, runChain_ = 0, stars_ = 0;
    std::string hint_;                  // short coaching banner inside the pot
    float hintT_ = 0.f;                 // seconds left, < 0 = until cleared
    int coach_ = -1;                    // first-run coach step (-1 = off)
    bool statsDone_ = false;
    float gravityNow() const { return cfg::kGravity * mode_->gravityScale() * modifierGravity(mod_); }
    bool rainOn() const { return mid_ == ModeId::Rain || mod_ == kModRain; }  // drops fall by themselves
    bool tideOn() const { return mid_ == ModeId::Tide || mod_ == kModTide; }  // the overflow line sinks and swells
    float autoInterval() const { return mod_ == kModRain && mid_ != ModeId::Rain ? rainInterval(elapsed_) : mode_->autoDropInterval(elapsed_); }
    bool manualOk() const { return mode_->manualDrop() && mod_ != kModRain; }
    void setHint(const std::string& s, float t) {
        hint_ = s;
        hintT_ = t;
    }
    bool undoAllowed() const { return !hot_ && !replaying_ && (mid_ == ModeId::Classic || mid_ == ModeId::Zen); }

    void buzz(Ctx& c, float strength, int ms) const {
        if (c.save.haptics) platform::vibrate(strength, ms);
    }

    void saveSnap();

    void doUndo(Ctx& c);
    bool aiming_ = false, committed_ = false, newBest_ = false;
    Spring holdPop_, nextPop_;
    std::string toast_;
    std::vector<FxSpark> parts_;
    std::vector<FloatText> texts_;
    std::vector<PopRing> rings_;

    int roll();

    void start(Ctx& c);

    // The first time a mode is played (and for rules that change), show a short hint inside the pot.
    void introHint(Ctx& c);

    Stats stats() const;

    int bestFor(const Ctx& c) const;

    // ----- simulation -----
    void tryDrop(Ctx& c, bool force = false, int levelOverride = -1);

    void grant(Ctx& c, int id);

    void onMerge(Ctx& c, const MergeEvent& e);

    void onImpact(Ctx& c, const Impact& im);

    // Music tension follows the pot: the fuller it gets, the busier the soundtrack.
    void updateAudio(Ctx& c, float dt);

    void simulate(Ctx& c, float dt, bool locked = false);

    // Controls while watching a run code: pause / play, speed, and how far the replay is.
    void replayBar(Ctx& c);

    // Zen has no fail state: liquid that settles above the line drains away harmlessly.
    void popOverflow(Ctx& c);

    void commit(Ctx& c);

    void finish(Ctx& c, Outcome o);

    // ----- Battle -----
    battle::SelfInfo selfInfo() const;

    void battleFrame(Ctx& c, float dt, float slow);

    void setResult(Ctx& c);

    void rematch(Ctx& c);

    void drawViewButton(Ctx& c);

    void drawCountdown(Ctx& c);

    void battleOverlay(Ctx& c);

    static const char* modName(int m) { return modifierKey(m); }

    // ----- effects -----
    void updateFx(float dt);

    void drawFx(Ctx& c);

    // ----- drawing -----
    float landingY(float x, int level) const;

    // Slow rising bubbles inside the tank for liquid ambience.
    void drawBubbles(Ctx& c);

    void drawGuide(Ctx& c);

    static BlobSprite popped(BlobSprite s, float pop) {
        s.ra *= 1.f + pop;
        s.rb *= 1.f + pop;
        return s;
    }

    // Madhouse: every 10 s about half of the drops (and the next ones) change number, size and colour by one step.
    void madShuffle(Ctx& c);

    void drawBlobs(Ctx& c, int sx, int sy);

    void drawNextPanel(Ctx& c);

    // Share card: the final pot with a caption, saved as a PNG next to the save file (the overlay is hidden for it).
    void captureCard(Ctx& c);

    bool ghostMode() const {
        return !hot_ && mod_ == 0 && (mid_ == ModeId::Classic || mid_ == ModeId::Timed || mid_ == ModeId::Rain || mid_ == ModeId::Tide ||
                             mid_ == ModeId::Zen);
    }

    void drawHud(Ctx& c);

    // Bottom strip: droplet levels reached so far.
    void drawHistory(Ctx& c);

    void drawHint(Ctx& c, bool paused);

    void drawToast(Ctx& c);

    void tutorialDemo(Ctx& c, float cx, float cy);

    void tutorial(Ctx& c);

    void endTutorial(Ctx& c);

    void pauseMenu(Ctx& c);

    void puzzleOverlay(Ctx& c, float oy);

    void overlay(Ctx& c);
};

}  // namespace liq
