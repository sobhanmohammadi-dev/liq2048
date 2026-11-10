// Settings panel: frame, title, page dispatch and navigation. Pages live in SettingsPages.cpp.
#include "scenes/settings/SettingsForm.hpp"

namespace liq {

namespace {
int g_page = 0;  // 0 = general, 1 = game feel, 2 = accessibility, 3 = liquid
constexpr int kPages = 4;
constexpr float kRowsTop = 40.f, kRowsBottom = 40.f;
}  // namespace

void setSettingsPage(int p) { g_page = p < 0 || p >= kPages ? 0 : p; }

bool drawSettings(Ctx& c, float y0, bool allowCustom) {
    using namespace settings;
    Ui& ui = c.ui;
    const int rows = g_page == 0 ? 9 : 8;
    const float panelH = kRowsTop + float(rows) * kRowH + kRowsBottom;
    ui.panel(14, y0, 242, panelH);
    c.gfx.text(c.loc.tr("settings.title"), 135, y0 + 20, 2, c.theme->accent, Align::Center, TextFx::Outline, c.theme->ink);

    SettingsForm f(c, y0 + kRowsTop);
    switch (g_page) {
        case 1: feelPage(f); break;
        case 2: accessPage(f); break;
        case 3: liquidPage(f); break;
        default: generalPage(f, g_page, allowCustom); break;
    }

    const float by = y0 + panelH - 34;
    if (ui.button(c.loc.tr("common.back"), 20, by, 100, 24)) {
        const bool leave = true;  // Back always closes the panel and returns to the general page next time
        g_page = 0;
        return leave;
    }
    // The second button walks the pages: general -> feel -> liquid -> accessibility -> general.
    static const int kNext[kPages] = {1, 3, 0, 2};
    static const char* kNextLabel[kPages] = {"settings.more", "settings.liquid", "settings.general", "settings.access"};
    if (ui.button(c.loc.tr(kNextLabel[g_page]), 150, by, 100, 24)) g_page = kNext[g_page];
    return false;
}

}  // namespace liq
