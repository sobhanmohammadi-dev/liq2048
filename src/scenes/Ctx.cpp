#include "scenes/Ctx.hpp"

#include "core/Access.hpp"
#include "gfx/PixelFont.hpp"

namespace liq {

void Ctx::applySettings() {
    if (window) SDL_SetWindowFullscreen(window, save.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    audio.setMusicVolume(save.musicVol);
    audio.setSfxVolume(save.sfxVol);
    gfx.setVsync(save.fpsLimit() == 0);
}

bool Ctx::setLanguage(const std::string& code) {
    const LangInfo* info = langInfo(code);
    if (!info || (info->arabicScript && !gfx.hasPersianFont())) return false;
    loc.load(code);
    gfx.setPersian(info->arabicScript);
    PixelFont::setGermanFold(code == "de");
    save.lang = code;
    if (code != "en") save.altLang = code;
    return true;
}

bool Ctx::themeUnlocked(int i) const {
    const int a = themeUnlockAch(i);
    return a < 0 || save.has(a);
}

void Ctx::setTheme(int i) {
    if (!themeUnlocked(i)) i = 0;
    save.theme = i;
    themeStore = accessTheme(themeAt(i), save.colorVision, save.highContrast);
    theme = &themeStore;
    gfx.buildBackdrop(*theme);
}

void Ctx::cycleTheme(int dir) {
    for (int k = 1; k <= kThemeCount; ++k) {
        const int i = ((save.theme + dir * k) % kThemeCount + kThemeCount) % kThemeCount;
        if (themeUnlocked(i)) {
            setTheme(i);
            return;
        }
    }
}

}  // namespace liq
