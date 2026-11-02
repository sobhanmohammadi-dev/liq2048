#pragma once
// Audio device wrapper around the procedural Synth (see Synth.hpp). No audio files are needed.
// Without an audio device (headless CI, dummy driver) every call is a harmless no-op.
#include <memory>

#include "audio/Synth.hpp"

namespace liq {

class Audio {
public:
    Audio();
    ~Audio();
    void init();
    void play(Sfx s, float amount = 0.5f, int level = 0, int chain = 1);
    void setMood(MusicMood m);
    void setIntensity(float v);
    void setMusicVolume(float v);
    void setSfxVolume(float v);
    void pause(bool paused);  // app in background

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace liq
