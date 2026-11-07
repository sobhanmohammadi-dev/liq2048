#pragma once
// Audio: plays the OGG files from assets/sounds/<palette>/ (effects) and assets/music/<palette>/ (loops), where
// palette is default, steel, sunset or jungle. Nothing is synthesized in the game; the files are produced offline
// by tools/audiogen. A custom theme can replace any slot with its own WAV or OGG file. Without an audio device
// (headless CI) or without the files, every call is a harmless no-op.
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
// Decodes a WAV (8/16/24/32-bit PCM or 32-bit float, mono or stereo, any rate) or an OGG file; the result is
// 44.1 kHz stereo, at most 60 seconds. Anything malformed or too large gives an empty clip.
Clip decodeAudio(const unsigned char* data, int size);

// The sound slots of a theme: three music loops and nine effect groups.
enum AudioSlot { kSlotMenu, kSlotCalm, kSlotIntense, kSlotDrop, kSlotSplash, kSlotMerge, kSlotChain, kSlotClick,
                 kSlotSwipe, kSlotFail, kSlotSuccess, kSlotUnlock, kAudioSlots };
struct SlotSource {
    int palette = 0;                    // 0 default, 1 steel, 2 sunset, 3 jungle (used when `data` is empty)
    std::vector<unsigned char> data;    // a custom WAV or OGG file
};
struct AudioPack {
    SlotSource slot[kAudioSlots];
};

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
    // Switches every sound to the given pack; decoding happens on a background thread, the old sounds play until then.
    void setPack(const AudioPack& pack);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace liq
