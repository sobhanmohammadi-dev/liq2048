#pragma once
#include <string>

namespace liq {

struct AppOptions {
    // Headless capture used for README screenshots / smoke tests:
    //   liq2048 --screenshot out.bmp --scene game --frames 600
    std::string shotPath;
    std::string shotScene = "menu";  // menu | modes | progress | settings | game
    int shotFrames = 60;
    std::string shotLang;            // optional: force "en" / "fa"
};

// Owns SDL, the window and the main loop. Scene switching (with fades) is
// driven by Ctx::go(); no global state.
class App {
public:
    int run(const AppOptions& opt = {});
};

}  // namespace liq
