#pragma once
// Shared application context handed to every scene (no global state).
#include <memory>
#include <string>

#include "audio/Audio.hpp"
#include "battle/Battle.hpp"
#include "core/Config.hpp"
#include "core/Themes.hpp"
#include "game/Replay.hpp"
#include "gfx/BlobRaster.hpp"
#include "gfx/Gfx.hpp"
#include "i18n/Loc.hpp"
#include "save/Save.hpp"
#include "ui/Input.hpp"
#include "ui/Ui.hpp"

namespace liq {

enum class SceneId { Menu, ModeSelect, Progress, Settings, Game, Brand, Puzzle, Credits, Battle };

struct Ctx {
    Ctx() : raster(cfg::kW, cfg::kH) { ui.bind(this); }
    Ctx(const Ctx&) = delete;
    Ctx& operator=(const Ctx&) = delete;

    SDL_Window* window = nullptr;
    Gfx gfx;
    BlobRaster raster;  // shared droplet layer
    Loc loc;
    Save save;
    Audio audio;
    Input in;
    Ui ui;
    Theme themeStore = themeAt(0);  // the active theme with accessibility options applied
    const Theme* theme = &themeStore;
    bool quit = false;
    bool hasNext = false;
    SceneId next = SceneId::Menu;
    ModeId mode = ModeId::Classic;
    Replay pendingReplay;       // run code pasted in the menu, picked up by the next game scene
    bool hasPendingReplay = false;
    // Battle: filled in by the Battle menus, picked up by the next game scene.
    struct BattleSetup {
        battle::MatchConfig cfg;
        std::shared_ptr<battle::Link> link;  // set for LAN matches
    } battle;
    int puzzleLevel = 0;        // selected puzzle (0-based) when mode == Puzzle
    float time = 0.f;           // seconds since start (animation clock)
    std::string brandVariant;   // dev: which branding image the Brand scene draws

    void go(SceneId s) { next = s; hasNext = true; }
    void applySettings();                       // fullscreen + volume
    bool setLanguage(const std::string& code);  // false if Persian font missing
    void setTheme(int i);
    void refreshTheme() { setTheme(save.theme); }  // after changing colour vision / contrast
    void cycleTheme(int dir = 1);               // next/previous unlocked theme
    bool themeUnlocked(int i) const;
};

class Scene {
public:
    virtual ~Scene() = default;
    virtual void frame(Ctx& c, float dt) = 0;  // update + draw
};

}  // namespace liq
