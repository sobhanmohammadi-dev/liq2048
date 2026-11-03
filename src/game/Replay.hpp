#pragma once
// Replay recorder. The liquid simulation is deterministic, so a run is fully described by the
// drops the player made: at which fixed physics step, where, and which level. Replaying those
// drops into a fresh World reproduces the pot exactly (on the same build and platform).
#include <cstdint>
#include <string>
#include <vector>

namespace liq {

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
    std::vector<ReplayDrop> drops;

    bool empty() const { return drops.empty(); }
    void clear() { *this = Replay{}; }
    // Remove drops at or after a step (used by undo).
    void truncateFrom(uint32_t step);

    std::string toText() const;
    static bool fromText(const std::string& s, Replay& out);
};

// Headless playback: plays the drops into a fresh World, then simulates `tailSteps` more steps,
// and returns a checksum of all particle positions. Used by the regression tests.
uint64_t replayChecksum(const Replay& r, int tailSteps);

}  // namespace liq
