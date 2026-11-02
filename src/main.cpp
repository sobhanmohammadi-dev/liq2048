#include <SDL.h>  // defines SDL_main on Windows/Android/iOS

#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <vector>

#include "audio/Synth.hpp"
#include "core/App.hpp"
#include "core/Version.hpp"

// Developer tool: render the soundtrack/SFX to a 16-bit stereo WAV to preview them without playing.
static int renderAudio(const char* path, float seconds, bool game) {
    liq::Synth s(44100);
    s.setMusicVolume(0.8f);
    s.setMood(game ? liq::MusicMood::Game : liq::MusicMood::Menu);
    s.setIntensity(game ? 0.7f : 0.f);
    const int total = int(seconds * 44100.f);
    std::vector<float> buf(size_t(total) * 2);
    for (int i = 0; i < total; i += 512) {
        const float t = float(i) / 44100.f;
        if (game && i % 44100 < 512 && int(t) % 4 == 3) s.sfx(liq::Sfx::Merge, 0.5f, int(t / 4.f) % 12, 1);
        s.render(&buf[size_t(i) * 2], std::min(512, total - i));
    }
    std::FILE* f = std::fopen(path, "wb");
    if (!f) return 1;
    const uint32_t dl = uint32_t(total) * 4, sz = 36 + dl, sr = 44100, br = sr * 4, sixteen = 16;
    const uint16_t fmt = 1, ch = 2, ba = 4, bits = 16;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&sz, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f);
    std::fwrite(&sixteen, 4, 1, f); std::fwrite(&fmt, 2, 1, f); std::fwrite(&ch, 2, 1, f); std::fwrite(&sr, 4, 1, f);
    std::fwrite(&br, 4, 1, f); std::fwrite(&ba, 2, 1, f); std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&dl, 4, 1, f);
    for (float v : buf) {
        const float c = v < -1.f ? -1.f : (v > 1.f ? 1.f : v);
        const int16_t x = int16_t(c * 32767.f);
        std::fwrite(&x, 2, 1, f);
    }
    std::fclose(f);
    return 0;
}

static void usage() {
    std::puts(
        "Liq2048 " LIQ_VERSION "\n"
        "Usage: liq2048 [options]\n"
        "  --help                 show this help\n"
        "  --version              print version\n"
        "  --screenshot <file>    render headlessly and save a BMP (dev tool)\n"
        "  --scene <name>         menu|modes|progress|settings|game|logo|banner|social (with --screenshot)\n"
        "  --frames <n>           simulated frames before capture (default 60)\n"
        "  --lang <en|fa>         force language (with --screenshot)\n"
        "  --render-audio <wav> [seconds] [menu|game]   render the soundtrack to a WAV file (dev tool)");
}

int main(int argc, char** argv) {
    liq::AppOptions opt;
    for (int i = 1; i < argc; ++i) {
        auto is = [&](const char* s) { return std::strcmp(argv[i], s) == 0; };
        auto arg = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
        if (is("--help") || is("-h")) { usage(); return 0; }
        if (is("--version")) { std::puts(LIQ_VERSION); return 0; }
        if (is("--render-audio")) {
            const char* path = arg();
            const float secs = i + 1 < argc && argv[i + 1][0] != '-' ? float(std::atof(argv[++i])) : 30.f;
            const bool game = i + 1 < argc && std::strcmp(argv[i + 1], "menu") != 0;
            return renderAudio(path, secs, game);
        }
        if (is("--screenshot")) opt.shotPath = arg();
        else if (is("--scene")) opt.shotScene = arg();
        else if (is("--frames")) opt.shotFrames = std::atoi(arg());
        else if (is("--lang")) opt.shotLang = arg();
    }
    liq::App app;
    return app.run(opt);
}
