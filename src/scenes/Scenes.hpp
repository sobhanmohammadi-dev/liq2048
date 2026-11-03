#pragma once
#include <memory>

#include "scenes/Ctx.hpp"

namespace liq {

std::unique_ptr<Scene> makeMenuScene();
std::unique_ptr<Scene> makeModeSelectScene();
std::unique_ptr<Scene> makeProgressScene();
std::unique_ptr<Scene> makeSettingsScene();
std::unique_ptr<Scene> makePuzzleSelectScene();
std::unique_ptr<Scene> makeGameScene(Ctx& c);
std::unique_ptr<Scene> makeBrandScene();  // dev: renders logo / banner images for docs
std::unique_ptr<Scene> makeScene(SceneId id, Ctx& c);

// ---- shared widgets and drawing helpers (Common.cpp) ----
void setSettingsPage(int page);     // dev: choose the page shown next
bool drawSettings(Ctx& c, float y0);  // settings panel; returns true when Back is pressed
void langToggle(Ctx& c, float x, float y);
void drawTankFrame(Ctx& c);           // walls, rivets and floor of the tank
void drawDeadLine(Ctx& c, bool warn, float y = cfg::kDeadY);
void drawWordmark(Ctx& c, float cx, float cy, int scale);  // LIQ2048 title lettering
BlobSprite plainSprite(float x, float y, int level, float wobble = 0.f, float phase = 0.f);

}  // namespace liq
