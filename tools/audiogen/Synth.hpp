#pragma once
// Offline audio generator (developer tool, NOT part of the game binary). It synthesizes every sound
// effect and the music loops, which `build_audio.py` encodes to OGG into assets/sounds and assets/music.
// The game itself only loads those files.
//   * SFX: layered voices (glides, bells, filtered noise) tuned to feel like liquid: bubbly
//     "bloops" whose pitch follows the droplet level on an A-minor pentatonic scale.
//   * Music: a generative ambient track (pad, bass, plucked arpeggio with ping-pong echo,
//     water-drop pings and soft percussion) over an Am9 - Fmaj7 - Cmaj9 - G6 loop. It reacts to
//     the game: calmer in menus, more layers as the pot fills up.
//   * Shared stereo reverb and a soft limiter.
// Synth is pure DSP (no SDL) and deterministic; the music repeats exactly every kLoopSamples.
#include <cstdint>
#include <vector>

namespace liq::audiogen {

enum class Sound { Drop, Splash, Merge, Chain, Click, Swipe, Fail, Success, Unlock, Count };
enum class MusicMood { Off, Menu, Game };

// One full chord cycle: 128 sixteenth-notes at 84 BPM = 22.857 s. Music renders as seamless loops of this length.
inline constexpr int kLoopSamples = 128 * 7875;  // at 44.1 kHz

class Synth {
public:
    explicit Synth(int sampleRate = 44100);
    ~Synth();
    Synth(const Synth&) = delete;
    Synth& operator=(const Synth&) = delete;

    // amount 0..1 (impact strength, ...); level = droplet level (pitch); chain = merge chain length.
    void sfx(Sound s, float amount = 0.5f, int level = 0, int chain = 1);
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

}  // namespace liq::audiogen
