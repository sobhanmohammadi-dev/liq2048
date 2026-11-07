#include "audio/Audio.hpp"

#include <SDL.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
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

namespace {
uint32_t rd32(const unsigned char* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
uint16_t rd16(const unsigned char* p) { return uint16_t(p[0] | p[1] << 8); }
constexpr int kMaxFrames = kRate * 60;

// Linear resampling of interleaved stereo; rate > 1 plays faster (higher).
Clip resample(const Clip& in, double rate) {
    Clip o;
    if (in.frames < 2 || rate <= 0.01) return o;
    const double outLen = double(in.frames - 1) / rate;
    if (outLen < 2 || outLen > kMaxFrames) return o;
    o.frames = int(outLen);
    o.pcm.resize(size_t(o.frames) * 2);
    for (int i = 0; i < o.frames; ++i) {
        const double p = double(i) * rate;
        const int a = std::min(int(p), in.frames - 2);
        const float f = float(p - a);
        for (int c = 0; c < 2; ++c) {
            const float x = float(in.pcm[size_t(a) * 2 + c]), y = float(in.pcm[size_t(a + 1) * 2 + c]);
            o.pcm[size_t(i) * 2 + c] = int16_t(std::lrint(x + (y - x) * f));
        }
    }
    return o;
}

Clip decodeWav(const unsigned char* d, int size) {
    Clip none;
    if (size < 44 || std::memcmp(d, "RIFF", 4) != 0 || std::memcmp(d + 8, "WAVE", 4) != 0) return none;
    int fmtTag = 0, ch = 0, bits = 0, rate = 0;
    const unsigned char* data = nullptr;
    uint32_t dlen = 0;
    size_t pos = 12;
    while (pos + 8 <= size_t(size)) {
        const uint32_t len = rd32(d + pos + 4);
        const size_t body = pos + 8;
        if (std::memcmp(d + pos, "fmt ", 4) == 0 && len >= 16 && body + 16 <= size_t(size)) {
            fmtTag = rd16(d + body);
            ch = rd16(d + body + 2);
            rate = int(rd32(d + body + 4));
            bits = rd16(d + body + 14);
            if (fmtTag == 0xFFFE && len >= 26 && body + 26 <= size_t(size)) fmtTag = rd16(d + body + 24);  // extensible
        } else if (std::memcmp(d + pos, "data", 4) == 0) {
            data = d + body;
            dlen = uint32_t(std::min<size_t>(len, size_t(size) - body));
            break;
        }
        pos = body + size_t(len) + (len & 1u);
        if (pos < body) return none;
    }
    if (!data || ch < 1 || ch > 2 || rate < 3000 || rate > 192000) return none;
    const bool pcm = fmtTag == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32);
    const bool flt = fmtTag == 3 && bits == 32;
    if (!pcm && !flt) return none;
    const size_t bps = size_t(bits / 8), frameBytes = bps * size_t(ch);
    const size_t n = dlen / frameBytes;
    if (n < 2 || double(n) * double(kRate) / double(rate) > kMaxFrames) return none;
    Clip src;
    src.frames = int(n);
    src.pcm.resize(n * 2);
    auto sample = [&](const unsigned char* q) -> float {
        if (flt) { float f; std::memcpy(&f, q, 4); return std::isfinite(f) ? std::clamp(f, -1.f, 1.f) : 0.f; }
        switch (bits) {
            case 8: return (float(q[0]) - 128.f) / 128.f;
            case 16: return float(int16_t(rd16(q))) / 32768.f;
            case 24: { int32_t v = int32_t(uint32_t(q[0]) << 8 | uint32_t(q[1]) << 16 | uint32_t(q[2]) << 24); return float(v >> 8) / 8388608.f; }
            default: return float(int32_t(rd32(q))) / 2147483648.f;
        }
    };
    for (size_t i = 0; i < n; ++i) {
        const unsigned char* q = data + i * frameBytes;
        const float l = sample(q), r = ch == 2 ? sample(q + bps) : l;
        src.pcm[i * 2] = int16_t(std::lrint(std::clamp(l, -1.f, 1.f) * 32767.f));
        src.pcm[i * 2 + 1] = int16_t(std::lrint(std::clamp(r, -1.f, 1.f) * 32767.f));
    }
    return rate == kRate ? src : resample(src, double(rate) / double(kRate));
}
}  // namespace

Clip decodeAudio(const unsigned char* data, int size) {
    if (!data || size < 12 || size > 24 * 1024 * 1024) return Clip{};
    Clip c = std::memcmp(data, "RIFF", 4) == 0 ? decodeWav(data, size) : decodeOgg(data, size);
    if (c.frames > kMaxFrames) return Clip{};
    return c;
}

struct Audio::Impl {
    SDL_AudioDeviceID dev = 0;
    std::map<std::string, Clip> sounds;  // swapped as a whole (under the device lock) by setPack
    Track menu, calm, intense;
    std::thread loader;
    std::mutex packMutex;
    AudioPack wanted;  // last requested pack (applied by init when it was requested before the device existed)
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
    if (impl_->loader.joinable()) impl_->loader.join();
    if (impl_->dev) {
        SDL_CloseAudioDevice(impl_->dev);
        impl_->dev = 0;
    }
}

void Audio::init() {
    Impl& m = *impl_;
    if (m.dev) return;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_Log("Audio unavailable: %s", SDL_GetError());
        return;
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
    setPack(AudioPack(m.wanted));
    SDL_PauseAudioDevice(m.dev, 0);
}

namespace {
const char* const kPaletteDir[4] = {"default", "steel", "sunset", "jungle"};
const char* const kMusicFile[3] = {"menu", "game_calm", "game_intense"};

struct Built {
    std::map<std::string, Clip> sounds;
    Clip music[3];
};

Built buildPack(const AudioPack& pk, Clip (*loadRel)(const std::string&)) {
    Built b;
    auto put = [&](const std::string& n, Clip c) { if (c.frames > 0) b.sounds[n] = std::move(c); };
    for (int s = 0; s < kAudioSlots; ++s) {
        const SlotSource& src = pk.slot[s];
        const bool custom = !src.data.empty();
        const std::string pal = kPaletteDir[std::clamp(src.palette, 0, 3)];
        Clip base;
        if (custom) {
            base = decodeAudio(src.data.data(), int(src.data.size()));
            if (base.frames == 0) continue;  // unreadable file: that slot stays silent
        }
        auto fileSfx = [&](const std::string& n) { put(n, loadRel("sounds/" + pal + "/" + n + ".ogg")); };
        switch (s) {
            case kSlotMenu: case kSlotCalm: case kSlotIntense:
                b.music[s] = custom ? std::move(base) : loadRel("music/" + pal + "/" + kMusicFile[s] + ".ogg");
                break;
            case kSlotDrop:
                if (!custom) { fileSfx("drop_0"); fileSfx("drop_1"); }
                else { put("drop_0", resample(base, 1.0)); put("drop_1", resample(base, 1.12)); }
                break;
            case kSlotSplash:
                if (!custom) { fileSfx("splash_0"); fileSfx("splash_1"); fileSfx("splash_2"); }
                else { put("splash_0", resample(base, 1.12)); put("splash_1", resample(base, 1.0)); put("splash_2", resample(base, 0.88)); }
                break;
            case kSlotMerge:
                for (int l = 0; l <= 11; ++l) {
                    char n[16];
                    std::snprintf(n, sizeof n, "merge_%02d", l);
                    if (!custom) fileSfx(n);
                    else put(n, resample(base, std::pow(2.0, double(l - 5) * 2.0 / 12.0)));
                }
                break;
            case kSlotChain:
                for (int c = 2; c <= 6; ++c) {
                    char n[16];
                    std::snprintf(n, sizeof n, "chain_%d", c);
                    if (!custom) fileSfx(n);
                    else put(n, resample(base, std::pow(2.0, double(c - 2) * 2.0 / 12.0)));
                }
                break;
            default: {
                static const char* const one[] = {"click", "swipe", "fail", "success", "unlock"};
                const char* n = one[s - kSlotClick];
                if (!custom) fileSfx(n);
                else put(n, std::move(base));
            }
        }
    }
    return b;
}
}  // namespace

void Audio::setPack(const AudioPack& pack) {
    Impl& m = *impl_;
    m.wanted = pack;
    if (!m.dev) return;
    std::lock_guard<std::mutex> g(m.packMutex);
    if (m.loader.joinable()) m.loader.join();
    Impl* im = &m;
    m.loader = std::thread([im, pack] {
        Built b = buildPack(pack, &Impl::load);
        SDL_LockAudioDevice(im->dev);
        for (Voice& v : im->voices) v.clip = nullptr;
        im->sounds.swap(b.sounds);
        Track* ts[3] = {&im->menu, &im->calm, &im->intense};
        for (int i = 0; i < 3; ++i) {
            ts[i]->ready.store(false, std::memory_order_release);
            ts[i]->clip = std::move(b.music[i]);
            ts[i]->pos = 0;
            ts[i]->ready.store(true, std::memory_order_release);
        }
        SDL_UnlockAudioDevice(im->dev);
    });
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
