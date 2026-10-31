#pragma once
#include <memory>

#include "scenes/Ctx.hpp"

namespace liq {

std::unique_ptr<Scene> makeMenuScene();
std::unique_ptr<Scene> makeModeSelectScene();
std::unique_ptr<Scene> makeProgressScene();
std::unique_ptr<Scene> makeSettingsScene();
std::unique_ptr<Scene> makeGameScene(Ctx& c);
std::unique_ptr<Scene> makeScene(SceneId id, Ctx& c);

// Shared widgets (Common.cpp)
bool drawSettings(Ctx& c, float y0);   // returns true when Back is pressed
void langToggle(Ctx& c, float x, float y);
void drawStars(Ctx& c);

}  // namespace liq
