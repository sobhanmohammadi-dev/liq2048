#pragma once
// Audio: plays the OGG files from assets/sounds (effects) and assets/music (loops). Nothing is synthesized in
// the game; the files are produced offline by tools/audiogen. Without an audio device (headless CI) or without
// the files, every call is a harmless no-op.
#include <cstdint>
#include <memory>
#include <vector>

namespace liq {

enum class Sfx { Drop, Splash, Merge, Chain, Click, Swipe, Fail, Success, Unlock, Count };
enum class MusicMood { Off, Menu, Game };

// A decoded clip: interleaved 16-bit stereo at 44.1 kHz.
struct Clip {
    std::vector<int16_t> pcm;
    int frames = 0;
};
// Decodes an OGG Vorbis file held in memory (any channel count, converted to stereo; 44.1 kHz expected).
Clip decodeOgg(const unsigned char* data, int size);

class Audio {
public:
    Audio();
    ~Audio();
    void init();
    // amount 0..1 = strength; level = tile level (picks the pitch of a merge); chain = merge chain length.
    void play(Sfx s, float amount = 0.5f, int level = 0, int chain = 1);
    void setMood(MusicMood m);
    void setIntensity(float v);  // 0..1, cross-fades the calm and intense game loops
    void setMusicVolume(float v);
    void setSfxVolume(float v);
    void pause(bool paused);  // app in background

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace liq
