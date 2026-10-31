#pragma once
// Shared application context handed to every scene (no global state).
#include <memory>
#include <string>

#include "audio/Audio.hpp"
#include "core/Themes.hpp"
#include "gfx/Gfx.hpp"
#include "i18n/Loc.hpp"
#include "save/Save.hpp"
#include "ui/Input.hpp"
#include "ui/Ui.hpp"

namespace liq {

enum class SceneId { Menu, ModeSelect, Progress, Settings, Game };

struct Ctx {
    Ctx() { ui.bind(this); }
    Ctx(const Ctx&) = delete;
    Ctx& operator=(const Ctx&) = delete;

    SDL_Window* window = nullptr;
    Gfx gfx;
    Loc loc;
    Save save;
    Audio audio;
    Input in;
    Ui ui;
    const Theme* theme = &themeAt(0);
    bool quit = false;
    bool hasNext = false;
    SceneId next = SceneId::Menu;
    ModeId mode = ModeId::Classic;

    void go(SceneId s) { next = s; hasNext = true; }
    void applySettings();                       // fullscreen + volume
    bool setLanguage(const std::string& code);  // false if Persian font missing
    void setTheme(int i);
    void cycleTheme();                          // next unlocked theme
    bool themeUnlocked(int i) const;
};

class Scene {
public:
    virtual ~Scene() = default;
    virtual void frame(Ctx& c, float dt) = 0;  // update + draw
};

}  // namespace liq
