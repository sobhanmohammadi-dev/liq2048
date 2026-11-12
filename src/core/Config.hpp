#pragma once
// All tunable constants live here (layout, physics, visuals). The game renders on a
// fixed 270x480 pixel grid that is scaled up with nearest-neighbour filtering, so one
// unit below is exactly one "art pixel".
namespace liq::cfg {

// ---- Pixel grid -----------------------------------------------------------
inline constexpr int kW = 270;
inline constexpr int kH = 480;

// ---- Play field (the tank interior) ----------------------------------------
inline constexpr float kFieldL = 22.f;
inline constexpr float kFieldR = 248.f;
inline constexpr float kFloor = 446.f;
inline constexpr int kTankTop = 98;       // top of the tank walls
inline constexpr float kDeadY = 138.f;    // overflow line
inline constexpr float kSpawnY = 114.f;   // where the held blob hovers

// ---- Blobs: value = 2^level, level 0..kMaxLevel (1 .. 2048) -----------------
inline constexpr int kMaxLevel = 11;
inline constexpr float kBaseRadius = 17.f;
inline constexpr float kRadiusGrowth = 1.15f;

// ---- Liquid (particle fluid, fixed step) -------------------------------------
inline constexpr float kStep = 1.f / 60.f;
inline constexpr int kSubsteps = 2;
inline constexpr float kGravity = 600.f;
inline constexpr float kMaxSpeed = 520.f;
inline constexpr float kSpacing = 4.4f;       // rest distance between particles (px)
inline constexpr float kInteract = 9.6f;      // interaction radius (px)
inline constexpr float kStiff = 8000.f;       // pressure stiffness
inline constexpr float kStiffNear = 9000.f;   // near-pressure (anti-clumping)
inline constexpr float kViscSigma = 60.f;      // linear viscosity (water-like; the Thick modifier triples it)
inline constexpr float kViscBeta = 0.f;       // quadratic viscosity
inline constexpr float kCohesion = 0.4f;     // surface tension of a drop in motion
inline constexpr float kHold = 0.06f;        // weak pull toward drop centroid (shape while falling)
inline constexpr float kDamp = 0.9995f;         // velocity kept per substep
inline constexpr float kHardSpeed = 900.f;     // absolute velocity cap after position corrections (px/s)
inline constexpr int kPhysicsVersion = 3;      // stored in run codes: replays only match on the same physics
inline constexpr float kSleepV = 25.f;          // below this speed (px/s) extra damping applies
inline constexpr float kSleepF = 0.97f;         // ... by this factor per substep
inline constexpr float kWallDrag = 0.97f;    // velocity kept per step while touching floor/walls (water slips along walls)

// ---- Gameplay ---------------------------------------------------------------
inline constexpr float kDropCooldown = 0.40f;
inline constexpr float kChainWindow = 0.7f;
inline constexpr float kOverflowSeconds = 1.6f;
inline constexpr float kAimSpeed = 180.f;
inline constexpr float kZenGravity = 0.35f;
inline constexpr float kTimeAttackSeconds = 90.f;
inline constexpr int kDailyDrops = 60;
inline constexpr int kWeeklyDrops = 100;
inline constexpr float kTideMaxShift = 215.f;   // how far the Rising Tide line can sink (px)
inline constexpr float kTideRate = 2.3f;         // px per second
inline constexpr float kJellyDamp = 14.f;        // jelly liquids: damping of the motion inside a drop (1/s)
inline constexpr float kMadnessPeriod = 10.f;     // Madhouse: seconds between changes
inline constexpr float kTideSwell = 30.f;       // after the line has sunk fully it swells up and down by this much (px)
inline constexpr float kTideReliefCap = 45.f;     // the most a run of merges can lift the line back (px)
inline constexpr float kTideSwellPeriod = 9.f;  // seconds per swell

}  // namespace liq::cfg

// Look and feel of the droplets. Tweak these to retune the blob style.
namespace liq::vis {

// Silhouette: every blob contributes a smooth field; pixels where the summed field
// reaches the iso-threshold are "liquid". A larger support makes touching blobs
// bridge more strongly (a fatter neck).
inline constexpr float kShapeSupport = 1.8f;
// Colour: each blob also has a wider colour kernel. Where two kernels overlap the
// pixel takes a mix of both colours, so neighbours blend into each other.
inline constexpr float kColorSupport = 2.3f;
// true : blend with an ordered (Bayer) dither so only palette colours are used
//        (classic pixel-art gradient). false: smooth 5-step colour mixing.
inline constexpr bool kDitherBlend = true;
// Outline darkness (0 = same colour as fill, 1 = full ink colour).
inline constexpr float kOutlineMix = 0.62f;

// Jelly wobble (visual only; physics circles stay rigid).
inline constexpr float kPopSpring = 950.f;
inline constexpr float kPopDamp = 12.f;
inline constexpr float kBreath = 0.014f;       // idle breathing amplitude
inline constexpr float kBreathHz = 0.55f;

}  // namespace liq::vis
