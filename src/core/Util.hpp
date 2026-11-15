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


// Portable, bit-exact maths for the simulation. libm's hypot and sin may differ in the last bit between glibc, Apple libm
// and the MSVC runtime; sqrt, +, -, *, / and fmod are exactly specified by IEEE 754, so everything below is built from those.
inline float detLen(float x, float y) { return std::sqrt(x * x + y * y); }
inline float detSin(float x) {
    constexpr float kPi = 3.14159265f, k2Pi = 6.2831853f;
    x = std::fmod(x, k2Pi);
    if (x > kPi) x -= k2Pi;
    else if (x < -kPi) x += k2Pi;
    if (x > kPi * 0.5f) x = kPi - x;
    else if (x < -kPi * 0.5f) x = -kPi - x;
    const float x2 = x * x;
    return x * (1.f + x2 * (-1.f / 6.f + x2 * (1.f / 120.f + x2 * (-1.f / 5040.f + x2 * (1.f / 362880.f)))));
}

}  // namespace liq
