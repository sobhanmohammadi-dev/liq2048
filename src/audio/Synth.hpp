#pragma once
// Procedural audio engine: every sound effect and the background music are synthesized in
// code, so the game ships no audio files and sounds identical on every platform.
//   * SFX: layered voices (glides, bells, filtered noise) tuned to feel like liquid: bubbly
//     "bloops" whose pitch follows the droplet level on an A-minor pentatonic scale.
//   * Music: a generative ambient track (pad, bass, plucked arpeggio with ping-pong echo,
//     water-drop pings and soft percussion) over an Am9 - Fmaj7 - Cmaj9 - G6 loop. It reacts to
//     the game: calmer in menus, more layers as the pot fills up.
//   * Shared stereo reverb and a soft limiter.
// Synth is pure DSP (no SDL), deterministic, and is driven from the audio callback.
#include <cstdint>
#include <vector>

namespace liq {

enum class Sfx { Drop, Splash, Merge, Chain, Click, Swipe, Fail, Success, Unlock, Count };
enum class MusicMood { Off, Menu, Game };

class Synth {
public:
    explicit Synth(int sampleRate = 44100);
    ~Synth();
    Synth(const Synth&) = delete;
    Synth& operator=(const Synth&) = delete;

    // amount 0..1 (impact strength, ...); level = droplet level (pitch); chain = merge chain length.
    void sfx(Sfx s, float amount = 0.5f, int level = 0, int chain = 1);
    void setMood(MusicMood m);
    void setIntensity(float v);  // 0..1 game tension (pile height, chains)
    void setMusicVolume(float v);
    void setSfxVolume(float v);
    void render(float* interleavedStereo, int frames);
    int sampleRate() const { return sr_; }

private:
    struct Impl;
    Impl* p_;
    int sr_;
};

}  // namespace liq
