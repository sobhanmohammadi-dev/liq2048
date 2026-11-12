#pragma once
// Tile pictures of the mode list: a small, endlessly looping animation per mode, centred on (cx, cy).
// Droplets are appended to `sp` (the caller rasterises them in one go); everything else is drawn directly.
#include <vector>

#include "scenes/Scenes.hpp"

namespace liq {

void drawModeIcon(Ctx& c, ModeId m, float cx, float cy, std::vector<BlobSprite>& sp);
float modeIconSStep(float e0, float e1, float x);
float modeIconPulse(float p);

}  // namespace liq
