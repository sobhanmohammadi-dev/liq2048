#include "audio/Audio.hpp"

#include <SDL.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <thread>

#include "core/Platform.hpp"

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

namespace liq {

namespace {
constexpr int kRate = 44100;
constexpr int kVoices = 24;

struct Voice {
    const Clip* clip = nullptr;
    double pos = 0;
    double rate = 1;
    float gainL = 0, gainR = 0;
};

struct Track {
    Clip clip;
    std::atomic<bool> ready{false};
    double pos = 0;
    float gain = 0;
};
}  // namespace

Clip decodeOgg(const unsigned char* data, int size) {
    Clip c;
    int ch = 0, rate = 0;
    short* out = nullptr;
    const int n = stb_vorbis_decode_memory(data, size, &ch, &rate, &out);
    if (n <= 0 || !out) return c;
    if (rate != kRate || ch < 1 || ch > 2) {
        std::free(out);
        return c;
    }
    c.frames = n;
    c.pcm.resize(size_t(n) * 2);
    for (int i = 0; i < n; ++i) {
        c.pcm[size_t(i) * 2] = out[i * ch];
        c.pcm[size_t(i) * 2 + 1] = out[i * ch + (ch - 1)];
    }
    std::free(out);
    return c;
}

struct Audio::Impl {
    SDL_AudioDeviceID dev = 0;
    std::map<std::string, Clip> sounds;  // filled before the device starts, read-only afterwards
    Track menu, calm, intense;
    std::thread loader;
    Voice voices[kVoices];
    int nextVoice = 0;
    uint32_t rng = 12345u;

    float musicVol = 0.6f, sfxVol = 0.9f;
    float musicVolS = 0.6f, sfxVolS = 0.9f;
    MusicMood mood = MusicMood::Off;
    float intensity = 0.f, intensityS = 0.f;

    float rnd() {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return float(rng >> 8) * (1.f / 16777216.f);
    }

    const Clip* sound(const char* fmt, int a = 0) {
        char n[48];
        std::snprintf(n, sizeof n, fmt, a);
        auto it = sounds.find(n);
        return it == sounds.end() ? nullptr : &it->second;
    }

    void start(const Clip* c, float gain, float pan, float pitch) {
        if (!c) return;
        Voice* v = &voices[nextVoice];
        for (int i = 0; i < kVoices; ++i)  // prefer a free voice, otherwise steal the round-robin one
            if (!voices[i].clip) { v = &voices[i]; break; }
        nextVoice = (nextVoice + 1) % kVoices;
        v->clip = c;
        v->pos = 0;
        v->rate = pitch;
        const float a = (pan + 1.f) * 0.7853982f;  // equal-power pan
        v->gainL = gain * std::cos(a);
        v->gainR = gain * std::sin(a);
    }

    void mix(float* out, int frames) {
        const float k = 1.f - std::exp(-1.f / (0.8f * float(kRate)));
        const float kv = 1.f - std::exp(-1.f / (0.03f * float(kRate)));
        const float moodMenu = mood == MusicMood::Menu ? 1.f : 0.f;
        const float moodGame = mood == MusicMood::Game ? 1.f : 0.f;
        for (int n = 0; n < frames; ++n) {
            musicVolS += (musicVol - musicVolS) * kv;
            sfxVolS += (sfxVol - sfxVolS) * kv;
            intensityS += (intensity - intensityS) * 0.0002f;
            menu.gain += (moodMenu - menu.gain) * k;
            calm.gain += (moodGame * (1.f - intensityS) - calm.gain) * k;
            intense.gain += (moodGame * intensityS - intense.gain) * k;
            float l = 0, r = 0;
            Track* ts[3] = {&menu, &calm, &intense};
            for (Track* t : ts) {
                if (!t->ready.load(std::memory_order_acquire) || t->clip.frames == 0) continue;
                const int i = int(t->pos);
                if (t->gain > 0.0005f) {
                    const float g = t->gain * musicVolS * 0.8f;
                    l += float(t->clip.pcm[size_t(i) * 2]) * (1.f / 32768.f) * g;
                    r += float(t->clip.pcm[size_t(i) * 2 + 1]) * (1.f / 32768.f) * g;
                }
                if (++t->pos >= t->clip.frames) t->pos = 0;  // the loops are seamless
            }
            for (Voice& v : voices) {
                if (!v.clip) continue;
                const int i = int(v.pos);
                if (i + 1 >= v.clip->frames) { v.clip = nullptr; continue; }
                const float f = float(v.pos - i);
                const int16_t* p = &v.clip->pcm[size_t(i) * 2];
                const float sl = (float(p[0]) + (float(p[2]) - float(p[0])) * f) * (1.f / 32768.f);
                const float sr = (float(p[1]) + (float(p[3]) - float(p[1])) * f) * (1.f / 32768.f);
                l += sl * v.gainL * sfxVolS;
                r += sr * v.gainR * sfxVolS;
                v.pos += v.rate;
            }
            out[n * 2] = std::tanh(l);
            out[n * 2 + 1] = std::tanh(r);
        }
    }

    static void callback(void* user, Uint8* stream, int len) {
        static_cast<Impl*>(user)->mix(reinterpret_cast<float*>(stream), len / int(sizeof(float) * 2));
    }

    static Clip load(const std::string& rel) {
        auto f = platform::readAsset(rel);
        return f.empty() ? Clip{} : decodeOgg(f.data(), int(f.size()));
    }
};

Audio::Audio() : impl_(new Impl) {}

Audio::~Audio() {
    if (impl_->dev) {
        SDL_CloseAudioDevice(impl_->dev);
        impl_->dev = 0;
    }
    if (impl_->loader.joinable()) impl_->loader.join();
}

void Audio::init() {
    Impl& m = *impl_;
    if (m.dev) return;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_Log("Audio unavailable: %s", SDL_GetError());
        return;
    }
    // Sound effects are small: decode them right away. Music loops decode on a background thread.
    static const char* names[] = {"drop_0", "drop_1", "splash_0", "splash_1", "splash_2", "click", "swipe", "fail", "success", "unlock",
                                  "chain_2", "chain_3", "chain_4", "chain_5", "chain_6"};
    for (const char* n : names) m.sounds[n] = Impl::load(std::string("sounds/") + n + ".ogg");
    for (int l = 0; l <= 11; ++l) {
        char n[16];
        std::snprintf(n, sizeof n, "merge_%02d", l);
        m.sounds[n] = Impl::load(std::string("sounds/") + n + ".ogg");
    }
    SDL_AudioSpec want{}, have{};
    want.freq = kRate;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = &Impl::callback;
    want.userdata = &m;
    m.dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!m.dev || have.format != AUDIO_F32SYS || have.channels != 2 || have.freq != kRate) {
        if (m.dev) SDL_CloseAudioDevice(m.dev);
        m.dev = 0;
        SDL_Log("Audio unavailable: %s", SDL_GetError());
        return;
    }
    m.loader = std::thread([&m] {
        Track* ts[3] = {&m.menu, &m.calm, &m.intense};
        const char* fs[3] = {"music/menu.ogg", "music/game_calm.ogg", "music/game_intense.ogg"};
        for (int i = 0; i < 3; ++i) {
            ts[i]->clip = Impl::load(fs[i]);
            ts[i]->ready.store(true, std::memory_order_release);
        }
    });
    SDL_PauseAudioDevice(m.dev, 0);
}

#define LIQ_LOCKED(stmt)                       \
    do {                                       \
        if (impl_->dev) {                      \
            SDL_LockAudioDevice(impl_->dev);   \
            stmt;                              \
            SDL_UnlockAudioDevice(impl_->dev); \
        }                                      \
    } while (0)

void Audio::play(Sfx s, float amount, int level, int chain) {
    Impl& m = *impl_;
    if (!m.dev) return;
    amount = std::clamp(amount, 0.f, 1.f);
    const float pitch = 1.f + (m.rnd() - 0.5f) * 0.06f;  // slight variation so repeats never sound identical
    const float pan = (m.rnd() - 0.5f) * 0.4f;
    SDL_LockAudioDevice(m.dev);
    switch (s) {
        case Sfx::Drop: m.start(m.sound("drop_%d", amount > 0.5f ? 1 : 0), 0.7f, pan, pitch); break;
        case Sfx::Splash: m.start(m.sound("splash_%d", amount < 0.4f ? 0 : (amount < 0.8f ? 1 : 2)), 0.5f + 0.4f * amount, pan, pitch); break;
        case Sfx::Merge:
            m.start(m.sound("merge_%02d", std::clamp(level, 0, 11)), 0.8f, pan, 1.f);
            if (chain >= 2) m.start(m.sound("chain_%d", std::clamp(chain, 2, 6)), 0.7f, 0.f, 1.f);
            break;
        case Sfx::Chain: m.start(m.sound("chain_%d", std::clamp(chain, 2, 6)), 0.7f, 0.f, 1.f); break;
        case Sfx::Click: m.start(m.sound("click"), 0.8f, 0.f, pitch); break;
        case Sfx::Swipe: m.start(m.sound("swipe"), 0.7f, 0.f, pitch); break;
        case Sfx::Fail: m.start(m.sound("fail"), 0.9f, 0.f, 1.f); break;
        case Sfx::Success: m.start(m.sound("success"), 0.9f, 0.f, 1.f); break;
        case Sfx::Unlock: m.start(m.sound("unlock"), 0.9f, 0.f, 1.f); break;
        case Sfx::Count: break;
    }
    SDL_UnlockAudioDevice(m.dev);
}
void Audio::setMood(MusicMood m) { LIQ_LOCKED(impl_->mood = m); impl_->mood = m; }
void Audio::setIntensity(float v) { LIQ_LOCKED(impl_->intensity = std::clamp(v, 0.f, 1.f)); }
void Audio::setMusicVolume(float v) { LIQ_LOCKED(impl_->musicVol = std::clamp(v, 0.f, 1.f)); impl_->musicVol = std::clamp(v, 0.f, 1.f); }
void Audio::setSfxVolume(float v) { LIQ_LOCKED(impl_->sfxVol = std::clamp(v, 0.f, 1.f)); impl_->sfxVol = std::clamp(v, 0.f, 1.f); }
void Audio::pause(bool paused) {
    if (impl_->dev) SDL_PauseAudioDevice(impl_->dev, paused ? 1 : 0);
}

}  // namespace liq
