// Custom theme editor: name, every colour, and the sound of every slot.
#include <algorithm>
#include <cmath>
#include <string>

#include "core/Access.hpp"
#include "core/Config.hpp"
#include "core/CustomTheme.hpp"
#include "core/Platform.hpp"
#include "scenes/Scenes.hpp"

namespace liq {

namespace {

class ThemeEditorScene final : public Scene {
public:
    ThemeEditorScene() {
        if (const char* tb = SDL_getenv("LIQ_TAB")) tab_ = std::clamp(std::atoi(tb), 0, 2);  // dev: screenshots
    }
    ~ThemeEditorScene() override {
        if (field_ >= 0) SDL_StopTextInput();
    }

    void frame(Ctx& c, float dt) override {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        Ui& ui = c.ui;
        if (!started_) {
            started_ = true;
            c.audio.setMood(MusicMood::Menu);
            c.audio.setIntensity(0.f);
        }
        g.drawBackdrop(t, c.save.reduceMotion ? 0.f : c.time);
        g.text(L.tr("ct.title"), cfg::kW / 2.f, 22, 2, t.accent, Align::Center, TextFx::Outline, t.ink);

        // Tabs.
        const char* tabs[3] = {"ct.tab.name", "ct.tab.colors", "ct.tab.sounds"};
        for (int i = 0; i < 3; ++i)
            if (ui.button(L.tr(tabs[i]), 22.f + float(i) * 78.f, 38, 70, 20, true, true, tab_ == i ? BtnStyle::Primary : BtnStyle::Normal)) {
                tab_ = i;
                stopField();
            }
        preview(c);

        if (tab_ == 0) nameTab(c);
        else if (tab_ == 1) colorTab(c);
        else soundTab(c);

        if (msgT_ > 0.f) {
            msgT_ -= dt;
            g.text(msg_, cfg::kW / 2.f, 412, 1, msgBad_ ? t.danger : t.accent, Align::Center, TextFx::Outline, t.ink);
        }
        if (ui.button(L.tr("common.back"), 22, 430, 108, 26) || c.in.esc) {
            if (field_ >= 0) stopField();
            else leave(c);
        }
        if (ui.button(L.tr("ct.use"), 140, 430, 108, 26, true, true, BtnStyle::Primary)) {
            commit(c);
            c.saveCustom();
            c.setTheme(kCustomTheme);
            c.applyAudioTheme(true);
            c.save.store();
            say(c, "ct.used");
        }
    }

private:
    void say(Ctx& c, const char* key, bool bad = false) {
        msg_ = c.loc.tr(key);
        msgBad_ = bad;
        msgT_ = 2.6f;
    }
    void stopField() {
        if (field_ >= 0) SDL_StopTextInput();
        field_ = -1;
    }
    void leave(Ctx& c) {
        commit(c);
        c.saveCustom();
        c.applyAudioTheme(true);  // back to the sounds of the active theme
        c.go(SceneId::Settings);
    }
    void commit(Ctx& c) {
        c.custom.nameEn = CustomTheme::cleanEnglish(c.custom.nameEn);
        if (c.custom.nameEn.empty()) c.custom.nameEn = "My theme";
        c.custom.theme.key = "theme.custom";
        bump(c);
    }
    void bump(Ctx& c) {
        ++c.customRev;
        if (c.save.theme == kCustomTheme) {
            c.themeStore = accessTheme(c.custom.theme, c.save.colorVision, c.save.highContrast);
            c.gfx.buildBackdrop(c.themeStore);
            c.applyAudioTheme();
        }
    }

    // A strip that shows the theme being edited, whatever the current UI theme is.
    void preview(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& e = c.custom.theme;
        const float x = 22, y = 62, w = 226, h = 52;
        g.rect(x - 1, y - 1, w + 2, h + 2, e.ink);
        for (int i = 0; i < 5; ++i) g.rect(x, y + float(i) * h / 5.f, w, h / 5.f + 1.f, e.sky[i]);
        g.rect(x + 8, y + 6, 3, 3, e.star);
        g.rect(x + 60, y + 12, 2, 2, e.star2);
        g.rect(x + 198, y + 8, 3, 3, e.star);
        g.rect(x + 6, y + 18, 96, 28, e.ink);
        g.rect(x + 7, y + 19, 94, 26, e.panel);
        g.rect(x + 7, y + 19, 94, 2, e.panelHi);
        g.text("2048", x + 54, y + 28, 1, e.text, Align::Center);
        g.text("ABC", x + 54, y + 38, 1, e.dim, Align::Center);
        g.rect(x + 108, y + 18, 40, 12, e.accentDark);
        g.rect(x + 108, y + 18, 40, 10, e.accent);
        g.rect(x + 154, y + 18, 40, 10, e.danger);
        for (int i = 0; i < 12; ++i) {
            const float px = x + 108.f + float(i % 6) * 19.f, py = y + 33.f + float(i / 6) * 9.f;
            g.rect(px, py, 8, 8, e.ink);
            g.rect(px + 1, py + 1, 6, 6, e.blob[i]);
        }
    }

    // ---- name tab ----
    bool textField(Ctx& c, int id, float y, std::string& value, bool ascii) {
        const Theme& t = *c.theme;
        const bool active = field_ == id;
        if (c.ui.button("", 22, y, 226, 28)) {
            if (field_ < 0) SDL_StartTextInput();
            field_ = id;
        }
        const std::string shown = value + (active && (int(c.time * 2.f) % 2 == 0) ? "_" : "");
        c.gfx.text(shown, cfg::kW / 2.f, y + 14, 1, active ? t.accent : t.text, Align::Center);
        if (!active) return false;
        const std::string typed = c.in.text;
        if (!typed.empty()) {
            if (ascii) {
                value = CustomTheme::cleanEnglish(value + typed);
                if (!typed.empty() && typed.back() == ' ' && value.size() < 24) value += ' ';
            } else {
                std::string v = value + typed;
                if (v.size() <= 48) value = v;
            }
        }
        if (c.in.backspace && !value.empty()) {
            do value.pop_back();
            while (!value.empty() && (((unsigned char)value.back()) & 0xC0) == 0x80);
            if (!value.empty() && ((unsigned char)value.back()) >= 0xC0) value.pop_back();
        }
        if (c.in.confirm) {
            stopField();
            return true;
        }
        return false;
    }

    void nameTab(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        const Align la = c.ui.start();
        const float lx = c.loc.rtl() ? 248.f : 22.f;
        float y = 122;
        g.text(L.tr("ct.name.en"), lx, y, 1, t.dim, la);
        textField(c, 0, y + 10, c.custom.nameEn, true);
        y += 46;
        const bool other = c.loc.code() != "en";
        if (other) {
            g.text(L.fmt("ct.name.local", langInfo(c.loc.code()) ? langInfo(c.loc.code())->name : c.loc.code()), lx, y, 1, t.dim, la);
            std::string& v = c.custom.names[c.loc.code()];
            std::string before = v;
            textField(c, 1, y + 10, v, false);
            if (v.empty() && before.empty() && field_ != 1) c.custom.names.erase(c.loc.code());
            y += 46;
        }
        c.ui.paragraph(L.tr("ct.name.hint"), lx, y, 226, 1, t.dim, la);
        y += 34;
        g.text(L.tr("ct.start"), lx, y, 1, t.dim, la);
        y += 12;
        for (int i = 0; i < 4; ++i) {
            const float bx = 22.f + float(i % 2) * 118.f, by = y + float(i / 2) * 28.f;
            if (c.ui.button(L.tr(themeAt(i).key), bx, by, 108, 24)) {
                const std::string n = c.custom.nameEn;
                const auto names = c.custom.names;
                c.custom.fromBuiltin(i, false);
                c.custom.nameEn = n;
                c.custom.names = names;
                for (auto& s : stash_) s = {};
                bump(c);
            }
        }
    }

    // ---- colour tab ----
    void colorTab(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        const int perPage = 6, pages = (kThemeColors + perPage - 1) / perPage;
        if (c.in.wheel != 0) page_ = std::clamp(page_ - int(c.in.wheel), 0, pages - 1);
        const bool rtl = L.rtl();
        for (int r = 0; r < perPage; ++r) {
            const int i = page_ * perPage + r;
            if (i >= kThemeColors) break;
            const float y = 122.f + float(r) * 29.f;
            if (c.ui.button("", 22, y, 226, 26, true, true, sel_ == i ? BtnStyle::Primary : BtnStyle::Normal)) sel_ = i;
            const Color col = CustomTheme::colorAt(c.custom.theme, i);
            const float sx = rtl ? 248.f - 24.f : 28.f;
            g.rect(sx, y + 5, 16, 16, t.ink);
            g.rect(sx + 1, y + 6, 14, 14, col);
            const std::string name = i < 5 ? L.fmt("ct.c.sky", std::to_string(i + 1)) : i >= 22 ? L.fmt("ct.c.blob", std::to_string(i - 21)) : L.tr(CustomTheme::colorKey(i));
            g.text(name, rtl ? sx - 8 : sx + 24, y + 13, 1, t.text, c.ui.start() == Align::Right ? Align::Right : Align::Left);
        }
        if (const int d = c.ui.stepper(22, 298, 226, 18, std::to_string(page_ + 1) + "/" + std::to_string(pages), true)) page_ = (page_ + d + pages) % pages;
        // RGB sliders for the selected colour.
        Color& col = CustomTheme::colorAt(c.custom.theme, sel_);
        const char* ch = "RGB";
        Uint8* v[3] = {&col.r, &col.g, &col.b};
        for (int k = 0; k < 3; ++k) {
            const float y = 324.f + float(k) * 24.f, bx = 44.f, bw = 164.f;
            g.text(std::string(1, ch[k]), 28, y + 8, 1, t.text, Align::Left);
            g.rect(bx - 1, y - 1, bw + 2, 18, t.ink);
            g.rect(bx, y, bw, 16, t.panelLo);
            const Color fill = k == 0 ? rgb(0xe0504a) : k == 1 ? rgb(0x4ac060) : rgb(0x4a80e8);
            g.rect(bx, y, bw * float(*v[k]) / 255.f, 16, fill);
            g.number(std::to_string(*v[k]), 244, y + 8, 1, t.text, Align::Right);
            if (c.in.down && c.in.mx >= bx - 4 && c.in.mx <= bx + bw + 4 && c.in.my >= y - 4 && c.in.my <= y + 20 && (drag_ == k || c.in.pressed)) {
                drag_ = k;
                *v[k] = Uint8(std::clamp(int(std::lround((c.in.mx - bx) / bw * 255.f)), 0, 255));
                dirty_ = true;
            }
        }
        if (!c.in.down) {
            drag_ = -1;
            if (dirty_) {
                dirty_ = false;
                bump(c);
            }
        }
        char hex[16];
        std::snprintf(hex, sizeof hex, "%02X%02X%02X", col.r, col.g, col.b);
        g.number(hex, cfg::kW / 2.f, 398, 1, t.dim, Align::Center);
    }

    // ---- sound tab ----
    void soundTab(Ctx& c) {
        Gfx& g = c.gfx;
        const Theme& t = *c.theme;
        Loc& L = c.loc;
        const bool rtl = L.rtl();
        const char* pal[4] = {"theme.default", "theme.mono", "theme.sunset", "theme.forest"};
        for (int s = 0; s < kAudioSlots; ++s) {
            const float y = 120.f + float(s) * 22.f;
            SlotSource& src = c.custom.pack.slot[s];
            if (sel_s_ == s) g.rect(22, y, 3, 18, t.accent);
            g.text(L.tr(CustomTheme::slotKey(s)), rtl ? 244.f : 30.f, y + 9, 1, sel_s_ == s ? t.accent : t.text, rtl ? Align::Right : Align::Left);
            const std::string label = src.data.empty() ? L.tr(pal[std::clamp(src.palette, 0, 3)]) : c.custom.fileName[s].substr(0, 9);
            const float bx = rtl ? 22.f + 40.f : 132.f;
            if (c.ui.button(label, rtl ? 66.f : 132.f, y, 80, 18)) {
                sel_s_ = s;
                cycle(c, s);
            }
            if (c.ui.button(L.tr("ct.play"), rtl ? 22.f : 216.f, y, 32, 18)) {
                sel_s_ = s;
                play(c, s);
            }
            (void)bx;
        }
        const std::string folder = platform::prefPath() + "themes";
        if (c.ui.button(L.fmt("ct.load", std::string(CustomTheme::slotFile(sel_s_))), 22, 388, 226, 20)) loadFile(c, sel_s_, folder);
        for (const std::string& f : c.in.dropped) loadPath(c, sel_s_, f);
    }

    void cycle(Ctx& c, int s) {
        SlotSource& src = c.custom.pack.slot[s];
        if (!src.data.empty()) {  // custom file -> palette 0
            stash_[s] = src.data;
            stashName_[s] = c.custom.fileName[s];
            src.data.clear();
            src.palette = 0;
        } else if (src.palette >= 3) {
            if (!stash_[s].empty()) {
                src.data = stash_[s];
                c.custom.fileName[s] = stashName_[s];
            } else {
                src.palette = 0;
            }
        } else {
            ++src.palette;
        }
        bump(c);
        if (c.save.theme != kCustomTheme) c.applyAudioPreview();
    }

    void play(Ctx& c, int s) {
        // The preview plays the custom pack even when another theme is active.
        c.applyAudioPreview();
        switch (s) {
            case kSlotMenu: c.audio.setMood(MusicMood::Menu); break;
            case kSlotCalm: c.audio.setMood(MusicMood::Game); c.audio.setIntensity(0.f); break;
            case kSlotIntense: c.audio.setMood(MusicMood::Game); c.audio.setIntensity(1.f); break;
            case kSlotDrop: c.audio.play(Sfx::Drop, 0.8f); break;
            case kSlotSplash: c.audio.play(Sfx::Splash, 0.7f); break;
            case kSlotMerge: c.audio.play(Sfx::Merge, 0.5f, 5, 1); break;
            case kSlotChain: c.audio.play(Sfx::Chain, 0.5f, 3, 4); break;
            case kSlotClick: c.audio.play(Sfx::Click); break;
            case kSlotSwipe: c.audio.play(Sfx::Swipe); break;
            case kSlotFail: c.audio.play(Sfx::Fail); break;
            case kSlotSuccess: c.audio.play(Sfx::Success); break;
            case kSlotUnlock: c.audio.play(Sfx::Unlock); break;
        }
    }

    void loadFile(Ctx& c, int s, const std::string& folder) {
        for (const char* ext : {".wav", ".ogg"}) {
            const std::string p = folder + "/" + CustomTheme::slotFile(s) + ext;
            if (!platform::readFile(p).empty()) {
                loadPath(c, s, p);
                return;
            }
        }
        platform::ensureDir(folder);
        msg_ = c.loc.fmt("ct.load.none", std::string(CustomTheme::slotFile(s)));
        msgBad_ = true;
        msgT_ = 4.f;
    }

    void loadPath(Ctx& c, int s, const std::string& path) {
        auto d = platform::readFile(path);
        const Clip clip = d.empty() ? Clip{} : decodeAudio(d.data(), int(d.size()));
        if (clip.frames == 0 || d.size() > 8u * 1024u * 1024u) {
            say(c, "ct.load.bad", true);
            return;
        }
        c.custom.pack.slot[s].data = d;
        std::string fn = path.substr(path.find_last_of("/\\") == std::string::npos ? 0 : path.find_last_of("/\\") + 1);
        std::string clean;
        for (char ch : fn)
            if (ch >= 33 && ch <= 126) clean += ch;
        c.custom.fileName[s] = clean.empty() ? "sound" : clean.substr(0, 40);
        bump(c);
        c.applyAudioPreview();
        say(c, "ct.load.ok");
    }

    bool started_ = false, msgBad_ = false, dirty_ = false;
    int tab_ = 0, page_ = 0, sel_ = 0, sel_s_ = 0, field_ = -1, drag_ = -1;
    std::string msg_, stashName_[kAudioSlots];
    std::vector<unsigned char> stash_[kAudioSlots];
    float msgT_ = 0.f;
};

}  // namespace

std::unique_ptr<Scene> makeThemeEditorScene() { return std::make_unique<ThemeEditorScene>(); }

}  // namespace liq
