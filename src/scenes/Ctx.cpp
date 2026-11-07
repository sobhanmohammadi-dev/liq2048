#include "scenes/Ctx.hpp"

#include <algorithm>

#include "core/Access.hpp"
#include "core/Platform.hpp"
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
    if (i == kCustomTheme) return customReady;
    const int a = themeUnlockAch(i);
    return a < 0 || save.has(a);
}

std::string Ctx::themeName(int i) const {
    return i == kCustomTheme ? custom.displayName(loc.code()) : loc.tr(themeAt(i).key);
}

void Ctx::loadCustom() {
    const auto d = platform::readFile(platform::prefPath() + "custom.liqtheme");
    CustomTheme t;
    if (!d.empty() && CustomTheme::parse(d.data(), d.size(), t)) {
        custom = std::move(t);
        customReady = true;
    }
}

bool Ctx::saveCustom() {
    const auto b = custom.serialize();
    const bool ok = platform::writeFile(platform::prefPath() + "custom.liqtheme", std::string(b.begin(), b.end()));
    if (ok) customReady = true;
    return ok;
}

void Ctx::applyAudioTheme(bool force) {
    const int i = save.theme;
    if (!force && audioKey == i && (i != kCustomTheme || audioRev == customRev)) return;
    audioKey = i;
    audioRev = customRev;
    if (i == kCustomTheme) {
        audio.setPack(custom.pack);
    } else {
        AudioPack p;
        for (SlotSource& s : p.slot) s.palette = std::clamp(i, 0, kThemeCount - 1);
        audio.setPack(p);
    }
}

void Ctx::setTheme(int i) {
    if (i < 0 || i >= kThemeSlots || !themeUnlocked(i)) i = 0;
    save.theme = i;
    themeStore = accessTheme(i == kCustomTheme ? custom.theme : themeAt(i), save.colorVision, save.highContrast);
    theme = &themeStore;
    gfx.buildBackdrop(*theme);
    applyAudioTheme();
}

void Ctx::cycleTheme(int dir) {
    for (int k = 1; k <= kThemeSlots; ++k) {
        const int i = ((save.theme + dir * k) % kThemeSlots + kThemeSlots) % kThemeSlots;
        if (themeUnlocked(i)) {
            setTheme(i);
            return;
        }
    }
}

}  // namespace liq
