#include "audio/Audio.hpp"

#include <SDL.h>

namespace liq {

struct Audio::Impl {
    SDL_AudioDeviceID dev = 0;
    std::unique_ptr<Synth> synth;
    float musicVol = 0.6f, sfxVol = 0.9f;
    MusicMood mood = MusicMood::Off;
    float intensity = 0.f;

    static void callback(void* user, Uint8* stream, int len) {
        auto* s = static_cast<Synth*>(user);
        s->render(reinterpret_cast<float*>(stream), len / int(sizeof(float) * 2));
    }
};

Audio::Audio() : impl_(new Impl) {}

Audio::~Audio() {
    if (impl_->dev) {
        SDL_CloseAudioDevice(impl_->dev);
        impl_->dev = 0;
    }
}

void Audio::init() {
    if (impl_->dev) return;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_Log("Audio unavailable: %s", SDL_GetError());
        return;
    }
    SDL_AudioSpec want{}, have{};
    want.freq = 44100;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = 1024;
    // Synth is created after we know the rate the device really runs at.
    impl_->synth.reset(new Synth(44100));
    want.callback = &Impl::callback;
    want.userdata = impl_->synth.get();
    impl_->dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!impl_->dev || have.format != AUDIO_F32SYS || have.channels != 2 || have.freq != 44100) {
        if (impl_->dev) SDL_CloseAudioDevice(impl_->dev);
        impl_->dev = 0;
        SDL_Log("Audio unavailable: %s", SDL_GetError());
        impl_->synth.reset();
        return;
    }
    impl_->synth->setMusicVolume(impl_->musicVol);
    impl_->synth->setSfxVolume(impl_->sfxVol);
    impl_->synth->setMood(impl_->mood);
    impl_->synth->setIntensity(impl_->intensity);
    SDL_PauseAudioDevice(impl_->dev, 0);
}

#define LIQ_LOCKED(stmt)                              \
    do {                                              \
        if (impl_->dev && impl_->synth) {             \
            SDL_LockAudioDevice(impl_->dev);          \
            stmt;                                     \
            SDL_UnlockAudioDevice(impl_->dev);        \
        }                                             \
    } while (0)

void Audio::play(Sfx s, float amount, int level, int chain) { LIQ_LOCKED(impl_->synth->sfx(s, amount, level, chain)); }
void Audio::setMood(MusicMood m) {
    impl_->mood = m;
    LIQ_LOCKED(impl_->synth->setMood(m));
}
void Audio::setIntensity(float v) {
    impl_->intensity = v;
    LIQ_LOCKED(impl_->synth->setIntensity(v));
}
void Audio::setMusicVolume(float v) {
    impl_->musicVol = v;
    LIQ_LOCKED(impl_->synth->setMusicVolume(v));
}
void Audio::setSfxVolume(float v) {
    impl_->sfxVol = v;
    LIQ_LOCKED(impl_->synth->setSfxVolume(v));
}
void Audio::pause(bool paused) {
    if (impl_->dev) SDL_PauseAudioDevice(impl_->dev, paused ? 1 : 0);
}

}  // namespace liq
