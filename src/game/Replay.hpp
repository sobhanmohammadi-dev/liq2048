#pragma once
// Replay recorder. The liquid simulation is deterministic, so a run is fully described by the
// drops the player made: at which fixed physics step, where, and which level. Replaying those
// drops into a fresh World reproduces the pot exactly (on the same build and platform).
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace liq {

// Drop positions are quantised to 1/128 px, so a run code stores them in 16 bits and loses nothing.
inline float quantX(float x) { return float(int(x * 128.f + 0.5f)) / 128.f; }

struct ReplayDrop {
    uint32_t step = 0;  // fixed physics steps simulated before this drop
    float x = 0;
    int level = 0;
};

struct Replay {
    int mode = 0;       // ModeId as int
    int modifier = 0;   // 0..4
    int substeps = 2;   // liquid quality used for the run
    float gravity = 0;  // gravity used for the run
    uint32_t seed = 0;  // drop-queue seed (so the replay scene rolls the same queue)
    int puzzle = 0;     // puzzle level (Puzzle mode)
    int physics = 2;    // cfg::kPhysicsVersion of the build that recorded it (1 = codes from before v1.7.4)
    int outcome = 0;    // 0 = recorded part only (copied while paused), 1 won, 2 lost, 3 finished
    uint32_t endStep = 0;  // physics steps simulated when the run ended or was copied; 0 = unknown (old codes)
    // Liquid-quality changes made by the performance governor: (step, substeps). They change the physics, so a
    // replay has to repeat them.
    std::vector<std::pair<uint32_t, uint8_t>> subs;
    std::vector<ReplayDrop> drops;

    bool empty() const { return drops.empty(); }
    void clear() { *this = Replay{}; }
    // Remove drops at or after a step (used by undo).
    void truncateFrom(uint32_t step);

    std::string toCode() const;  // short shareable run code, e.g. "LQ2-..." (base64url + checksum); "LQ1-" codes still load
    static bool fromCode(const std::string& s, Replay& out);
    std::string toText() const;
    static bool fromText(const std::string& s, Replay& out);
};

// Headless playback: plays the drops into a fresh World, then simulates `tailSteps` more steps,
// and returns a checksum of all particle positions. Used by the regression tests.
uint64_t replayChecksum(const Replay& r, int tailSteps);

}  // namespace liq
