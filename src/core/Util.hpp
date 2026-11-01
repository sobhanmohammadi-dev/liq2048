#pragma once
#include <cstdint>
#include <random>

namespace liq {

// Deterministic RNG wrapper. Uses raw mt19937 output (not std distributions,
// whose results differ between standard libraries) so the Daily Challenge
// produces the same drop sequence on every platform.
class Rng {
public:
    void seed(uint32_t s) { g_.seed(s); }
    int range(int lo, int hi) { return lo + int(g_() % uint32_t(hi - lo + 1)); }
    float uni(float lo, float hi) { return lo + (hi - lo) * float(g_() >> 8) / 16777216.f; }

private:
    std::mt19937 g_{1};
};

template <class T>
T clampv(T v, T lo, T hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Damped spring toward 0 (semi-implicit Euler: stable for k*dt^2 < ~4).
struct Spring {
    float x = 0, v = 0;
    void step(float dt, float k, float c) {
        v += (-k * x - c * v) * dt;
        x += v * dt;
    }
};

}  // namespace liq
