#include "scenes/Ctx.hpp"

namespace liq {

void Ctx::applySettings() {
    if (window) SDL_SetWindowFullscreen(window, save.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    audio.setVolume(save.volume);
}

bool Ctx::setLanguage(const std::string& code) {
    if (code == "fa" && !gfx.hasPersianFont()) return false;
    loc.load(code);
    gfx.setPersian(code == "fa");
    save.lang = code;
    return true;
}

bool Ctx::themeUnlocked(int i) const {
    int a = themeUnlockAch(i);
    return a < 0 || save.has(a);
}

void Ctx::setTheme(int i) {
    if (!themeUnlocked(i)) i = 0;
    save.theme = i;
    theme = &themeAt(i);
}

void Ctx::cycleTheme() {
    for (int k = 1; k <= kThemeCount; ++k) {
        int i = (save.theme + k) % kThemeCount;
        if (themeUnlocked(i)) {
            setTheme(i);
            return;
        }
    }
}

}  // namespace liq
