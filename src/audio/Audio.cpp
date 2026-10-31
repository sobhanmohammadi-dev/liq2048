#include "audio/Audio.hpp"

#include <SDL.h>

#include <string>
#include <vector>

#include "core/Platform.hpp"

#ifdef LIQ_HAS_MIXER
#include <SDL_mixer.h>
#endif

namespace liq {

static const char* kNames[] = {"merge", "drop", "chain", "click", "fail", "success"};

struct Audio::Impl {
#ifdef LIQ_HAS_MIXER
    Mix_Chunk* chunks[int(Sfx::Count)] = {};
    Mix_Music* music = nullptr;
    SDL_RWops* musicRw = nullptr;
    std::vector<unsigned char> musicData;
    bool open = false;
#endif
};

Audio::Audio() : impl_(new Impl) {}

Audio::~Audio() {
#ifdef LIQ_HAS_MIXER
    if (!impl_->open) return;
    for (auto* c : impl_->chunks)
        if (c) Mix_FreeChunk(c);
    if (impl_->music) Mix_FreeMusic(impl_->music);
    if (impl_->musicRw) SDL_RWclose(impl_->musicRw);
    Mix_CloseAudio();
#endif
}

void Audio::init() {
#ifdef LIQ_HAS_MIXER
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return;
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) != 0) return;
    impl_->open = true;
    for (int i = 0; i < int(Sfx::Count); ++i) {
        for (const char* ext : {".wav", ".ogg"}) {
            auto d = platform::readAsset(std::string("sounds/") + kNames[i] + ext);
            if (d.empty()) continue;
            SDL_RWops* rw = SDL_RWFromConstMem(d.data(), int(d.size()));
            impl_->chunks[i] = Mix_LoadWAV_RW(rw, 1);  // decoded fully, buffer can go
            if (impl_->chunks[i]) break;
        }
    }
    for (const char* ext : {".ogg", ".mp3"}) {
        impl_->musicData = platform::readAsset(std::string("music/theme") + ext);
        if (impl_->musicData.empty()) continue;
        impl_->musicRw = SDL_RWFromConstMem(impl_->musicData.data(), int(impl_->musicData.size()));
        impl_->music = Mix_LoadMUS_RW(impl_->musicRw, 0);
        if (impl_->music) {
            Mix_PlayMusic(impl_->music, -1);
            break;
        }
    }
    setVolume(volume_);
#endif
}

void Audio::play(Sfx s) {
#ifdef LIQ_HAS_MIXER
    if (impl_->open && impl_->chunks[int(s)]) Mix_PlayChannel(-1, impl_->chunks[int(s)], 0);
#else
    (void)s;
#endif
}

void Audio::setVolume(float v) {
    volume_ = v < 0 ? 0 : (v > 1 ? 1 : v);
#ifdef LIQ_HAS_MIXER
    if (impl_->open) {
        Mix_Volume(-1, int(volume_ * MIX_MAX_VOLUME));
        Mix_VolumeMusic(int(volume_ * MIX_MAX_VOLUME));
    }
#endif
}

}  // namespace liq
