#pragma once
// Audio manager. No audio is shipped: drop files into assets/sounds/ and
// assets/music/ (see README) and they are picked up automatically. Without
// files, or without SDL2_mixer, every call is a silent no-op.
#include <memory>

namespace liq {

enum class Sfx { Merge, Drop, Chain, Click, Fail, Success, Count };

class Audio {
public:
    Audio();
    ~Audio();
    void init();
    void play(Sfx s);
    void setVolume(float v);  // 0..1
    float volume() const { return volume_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    float volume_ = 0.8f;
};

}  // namespace liq
