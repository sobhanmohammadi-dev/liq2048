// Settings panel: frame, title, tab strip, page dispatch and Back. Pages live in SettingsPages.cpp.
//
//   +-------------------------------+
//   |           SETTINGS            |
//   | [General][Game][Liquid][Access] |   <- tabs: one tap to any page
//   |   rows of the selected page   |
//   |            [Back]             |
//   +-------------------------------+
#include "scenes/settings/SettingsForm.hpp"

namespace liq {

namespace {
int g_page = 0;  // 0 = general, 1 = game feel, 2 = accessibility, 3 = liquid
constexpr int kPages = 4;
constexpr int kRows = 8;                       // every page is laid out on the same eight-row panel
constexpr float kRowsTop = 66.f, kRowsBottom = 40.f;
const int kTabPage[kPages] = {0, 1, 3, 2};  // display order: General, Game, Liquid, Access
}  // namespace

void setSettingsPage(int p) { g_page = p < 0 || p >= kPages ? 0 : p; }

bool drawSettings(Ctx& c, float y0, bool allowCustom) {
    using namespace settings;
    Ui& ui = c.ui;
    const float panelH = kRowsTop + float(kRows) * kRowH + kRowsBottom;
    ui.panel(14, y0, 242, panelH);
    c.gfx.text(c.loc.tr("settings.title"), 135, y0 + 20, 2, c.theme->accent, Align::Center, TextFx::Outline, c.theme->ink);

    // tab strip
    const float tw = 56.f, gap = 2.f;
    for (int i = 0; i < kPages; ++i) {
        const int page = kTabPage[i];
        const char* key = page == 0 ? "settings.general" : page == 1 ? "tab.feel" : page == 2 ? "settings.access" : "tab.liquid";
        if (ui.button(c.loc.tr(key), ui.mx(19.f + float(i) * (tw + gap), tw), y0 + 34, tw, 20, true, true,
                      g_page == page ? BtnStyle::Primary : BtnStyle::Normal))
            g_page = page;
    }

    SettingsForm f(c, y0 + kRowsTop);
    switch (g_page) {
        case 1: feelPage(f); break;
        case 2: accessPage(f); break;
        case 3: liquidPage(f); break;
        default: generalPage(f, allowCustom); break;
    }

    if (ui.button(c.loc.tr("common.back"), 55, y0 + panelH - 34, 160, 24)) {
        g_page = 0;  // the next visit starts on the general page again
        return true;
    }
    return false;
}

}  // namespace liq
