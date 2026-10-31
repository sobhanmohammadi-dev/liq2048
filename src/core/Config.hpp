#pragma once
// All tunable constants live here (layout, physics, scoring).
namespace liq::cfg {

// Logical (resolution-independent) screen. SDL scales it to any window.
inline constexpr int kW = 360;
inline constexpr int kH = 640;

// Play field (logical pixels).
inline constexpr float kFieldL = 30.f;
inline constexpr float kFieldR = 330.f;
inline constexpr float kFloor = 596.f;
inline constexpr float kDeadY = 180.f;   // overflow line
inline constexpr float kSpawnY = 142.f;  // where the held blob hovers

// Blobs: value = 2^level, level 0..kMaxLevel (1 .. 2048).
inline constexpr int kMaxLevel = 11;
inline constexpr float kBaseRadius = 11.f;
inline constexpr float kRadiusGrowth = 1.17f;

// Physics (position based dynamics, fixed step).
inline constexpr float kStep = 1.f / 60.f;
inline constexpr int kSubsteps = 4;
inline constexpr int kIterations = 8;
inline constexpr float kGravity = 1400.f;
inline constexpr float kDamping = 0.995f;
inline constexpr float kMaxSpeed = 1500.f;

// Gameplay.
inline constexpr float kDropCooldown = 0.45f;
inline constexpr float kChainWindow = 0.7f;
inline constexpr float kOverflowSeconds = 2.0f;
inline constexpr float kAimSpeed = 240.f;
inline constexpr float kZenGravity = 0.35f;
inline constexpr float kTimeAttackSeconds = 90.f;
inline constexpr int kDailyDrops = 60;

}  // namespace liq::cfg
