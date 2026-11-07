// Renders every sound effect and music loop of Liq2048, for each of the four sound palettes, to 24-bit stereo
// WAV masters (44.1 kHz).
//   liq_audiogen <out_dir>        writes <out_dir>/<palette>/sounds/*.wav and <out_dir>/<palette>/music/*.wav
//                                 with palette = default, steel, sunset, jungle
// tools/audiogen/build_audio.py then stores the masters as FLAC in original_assets/ and encodes the optimized
// OGG files into assets/. Re-run it after changing Synth.cpp.

#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "Synth.hpp"

using namespace liq::audiogen;

static constexpr int kRate = 44100;

static void writeWav(const std::string& path, const std::vector<float>& stereo) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        std::fprintf(stderr, "cannot write %s\n", path.c_str());
        std::exit(1);
    }
    const uint32_t frames = uint32_t(stereo.size() / 2), dl = frames * 6, sz = 36 + dl, sr = kRate, br = sr * 6, sixteen = 16;
    const uint16_t fmt = 1, ch = 2, ba = 6, bits = 24;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&sz, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f);
    std::fwrite(&sixteen, 4, 1, f); std::fwrite(&fmt, 2, 1, f); std::fwrite(&ch, 2, 1, f); std::fwrite(&sr, 4, 1, f);
    std::fwrite(&br, 4, 1, f); std::fwrite(&ba, 2, 1, f); std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&dl, 4, 1, f);
    for (float v : stereo) {
        const int32_t x = int32_t(std::lrint(std::max(-1.f, std::min(1.f, v)) * 8388607.f));
        const unsigned char b[3] = {(unsigned char)(x & 0xFF), (unsigned char)((x >> 8) & 0xFF), (unsigned char)((x >> 16) & 0xFF)};
        std::fwrite(b, 1, 3, f);
    }
    std::fclose(f);
}

static void normalize(std::vector<float>& b, float peak) {
    float m = 0.f;
    for (float v : b) m = std::max(m, std::fabs(v));
    if (m > peak) {
        const float k = peak / m;
        for (float& v : b) v *= k;
    }
}

// One-shot effect: render, trim the silent tail, short fade-out, cap the peak.
static Style g_style = Style::Night;

static void sfx(const std::string& dir, const std::string& name, Sound s, float amount, int level, int chain, float seconds) {
    Synth syn(kRate, g_style);
    syn.setSfxVolume(1.f);
    syn.sfx(s, amount, level, chain);
    std::vector<float> b(size_t(seconds * float(kRate)) * 2);
    for (size_t i = 0; i < b.size() / 2; i += 512) syn.render(&b[i * 2], int(std::min<size_t>(512, b.size() / 2 - i)));
    size_t end = b.size() / 2;
    while (end > 1 && std::fabs(b[(end - 1) * 2]) < 0.0005f && std::fabs(b[(end - 1) * 2 + 1]) < 0.0005f) --end;
    end = std::min(b.size() / 2, end + size_t(kRate / 100));
    b.resize(end * 2);
    const size_t fade = std::min<size_t>(end, size_t(kRate / 50));
    for (size_t i = 0; i < fade; ++i) {
        const float g = float(i) / float(fade);
        b[(end - 1 - i) * 2] *= g;
        b[(end - 1 - i) * 2 + 1] *= g;
    }
    normalize(b, 0.9f);
    writeWav(dir + "/sounds/" + name + ".wav", b);
}

// Music loop. Pads/LFOs are not phase-periodic, so the loop is closed with a cross-fade: render three cycles
// plus a tail, keep cycle 3, and blend the tail (which continues the music past the loop end) into its head.
static void music(const std::string& dir, const std::string& name, MusicMood mood, float intensity) {
    Synth syn(kRate, g_style);
    syn.setMusicVolume(1.f);
    syn.setMood(mood);
    syn.setIntensity(intensity);
    const size_t L = size_t(syn.loopSamples()), T = size_t(kRate) * 3 / 2;
    std::vector<float> all((L * 3 + T) * 2);
    for (size_t i = 0; i < all.size() / 2; i += 512) syn.render(&all[i * 2], int(std::min<size_t>(512, all.size() / 2 - i)));
    const float* A = &all[L * 2 * 2];  // cycle 3 onwards (A[L..L+T) is the continuation after the loop end)
    std::vector<float> loop(A, A + L * 2);
    for (size_t i = 0; i < T; ++i) {
        const float w = float(i) / float(T);
        for (size_t c = 0; c < 2; ++c) loop[i * 2 + c] = A[i * 2 + c] * w + A[(L + i) * 2 + c] * (1.f - w);
    }
    float jump = 0.f;  // discontinuity at the loop point
    for (size_t c = 0; c < 2; ++c) jump = std::max(jump, std::fabs(loop[c] - A[(L - 1) * 2 + c]));
    std::printf("music %-12s loop-point step %.5f\n", name.c_str(), jump);
    normalize(loop, 0.85f);
    writeWav(dir + "/music/" + name + ".wav", loop);
}

static void renderPalette(const std::string& root, const char* palette, Style style) {
    g_style = style;
    const std::string dir = root + "/" + palette;
    ::mkdir(dir.c_str(), 0755);
    ::mkdir((dir + "/sounds").c_str(), 0755);
    ::mkdir((dir + "/music").c_str(), 0755);
    std::printf("== %s\n", palette);

    sfx(dir, "drop_0", Sound::Drop, 0.2f, 0, 1, 1.0f);
    sfx(dir, "drop_1", Sound::Drop, 0.8f, 0, 1, 1.0f);
    sfx(dir, "splash_0", Sound::Splash, 0.25f, 4, 1, 2.0f);
    sfx(dir, "splash_1", Sound::Splash, 0.6f, 4, 1, 2.0f);
    sfx(dir, "splash_2", Sound::Splash, 1.0f, 4, 1, 2.0f);
    for (int l = 0; l <= 11; ++l) {
        char n[16];
        std::snprintf(n, sizeof n, "merge_%02d", l);
        sfx(dir, n, Sound::Merge, 0.5f, l, 1, l >= 11 ? 5.f : 2.5f);
    }
    for (int c = 2; c <= 6; ++c) {
        char n[16];
        std::snprintf(n, sizeof n, "chain_%d", c);
        sfx(dir, n, Sound::Chain, 0.5f, 2, c, 2.0f);
    }
    sfx(dir, "click", Sound::Click, 0.5f, 0, 1, 0.5f);
    sfx(dir, "swipe", Sound::Swipe, 0.5f, 0, 1, 1.0f);
    sfx(dir, "fail", Sound::Fail, 0.5f, 0, 1, 3.0f);
    sfx(dir, "success", Sound::Success, 0.5f, 0, 1, 3.0f);
    sfx(dir, "unlock", Sound::Unlock, 0.5f, 0, 1, 2.0f);

    music(dir, "menu", MusicMood::Menu, 0.f);
    music(dir, "game_calm", MusicMood::Game, 0.15f);
    music(dir, "game_intense", MusicMood::Game, 0.9f);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: liq_audiogen <out_dir> [palette]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const std::string only = argc > 2 ? argv[2] : "";
    ::mkdir(dir.c_str(), 0755);
    const struct { const char* name; Style style; } palettes[] = {
        {"default", Style::Night}, {"steel", Style::Steel}, {"sunset", Style::Sunset}, {"jungle", Style::Jungle}};
    for (const auto& p : palettes)
        if (only.empty() || only == p.name) renderPalette(dir, p.name, p.style);
    return 0;
}
