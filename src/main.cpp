#include <SDL.h>  // defines SDL_main on Windows/Android/iOS

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/App.hpp"
#include "core/Version.hpp"

static void usage() {
    std::puts(
        "Liq2048 " LIQ_VERSION "\n"
        "Usage: liq2048 [options]\n"
        "  --help                 show this help\n"
        "  --version              print version\n"
        "  --screenshot <file>    render headlessly and save a BMP (dev tool)\n"
        "  --scene <name>         menu|modes|progress|settings|game|battle|battlegame|hotseat|logo|banner|social (with --screenshot)\n"
        "  --frames <n>           simulated frames before capture (default 60)\n"
        "  --lang <code>          force language: en fa es fr de tr pt ar (with --screenshot)\n"
        "  --page <0-2>           settings page (with --scene settings)\n"
        "  --set k=v,k=v          override options, e.g. colorVision=1,highContrast=1 (dev tool)\n"
        "  --render-audio <wav> [seconds] [menu|game]   render the soundtrack to a WAV file (dev tool)");
}

int main(int argc, char** argv) {
    liq::AppOptions opt;
    for (int i = 1; i < argc; ++i) {
        auto is = [&](const char* s) { return std::strcmp(argv[i], s) == 0; };
        auto arg = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
        if (is("--help") || is("-h")) { usage(); return 0; }
        if (is("--version")) { std::puts(LIQ_VERSION); return 0; }
        if (is("--screenshot")) opt.shotPath = arg();
        else if (is("--scene")) opt.shotScene = arg();
        else if (is("--frames")) opt.shotFrames = std::atoi(arg());
        else if (is("--lang")) opt.shotLang = arg();
        else if (is("--page")) opt.shotPage = std::atoi(arg());
        else if (is("--set")) opt.shotSet = arg();
    }
    liq::App app;
    return app.run(opt);
}
