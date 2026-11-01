#include "core/App.hpp"

#include <SDL.h>

#include <algorithm>
#include <memory>

#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "scenes/Scenes.hpp"

namespace liq {

namespace {

SceneId sceneFromName(const std::string& n, Ctx& c) {
    if (n == "modes") return SceneId::ModeSelect;
    if (n == "progress") return SceneId::Progress;
    if (n == "settings") return SceneId::Settings;
    if (n == "game") return SceneId::Game;
    if (n == "logo" || n == "banner" || n == "social") {
        c.brandVariant = n;
        return SceneId::Brand;
    }
    return SceneId::Menu;
}

// Scripted input for headless captures: a drop every half second at varying x.
void scriptedInput(Input& in, int frame) {
    if (frame % 30 == 5) {
        in.mx = 60.f + float((frame / 30) * 83 % 150);
        in.my = 300.f;
        in.down = in.pressed = true;
    } else if (frame % 30 == 6) {
        in.down = false;
        in.released = true;
    }
}

}  // namespace

int App::run(const AppOptions& opt) {
    const bool shot = !opt.shotPath.empty();
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");  // nearest: crisp pixels
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    if (platform::isMobile()) SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait");
    if (shot && !SDL_getenv("SDL_VIDEODRIVER")) SDL_setenv("SDL_VIDEODRIVER", "dummy", 0);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (platform::isMobile()) flags |= SDL_WINDOW_FULLSCREEN;
    const int ww = shot ? cfg::kW * 2 : cfg::kW * 3, wh = shot ? cfg::kH * 2 : cfg::kH * 3 / 2 * 2;
    SDL_Window* window = SDL_CreateWindow("Liq2048", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh, flags);
    if (!window) {
        SDL_Log("Window failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    int exitCode = 0;
    {
        Ctx c;
        c.window = window;
        if (!c.gfx.init(window, !platform::isMobile())) {
            SDL_Log("Renderer failed: %s", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        c.gfx.loadFonts();
        if (!shot) {  // captures must not read or write the player's save
            c.save.load();
            c.save.touchDay(platform::dayNumber());
        } else {
            c.save.tutorialDone = true;
        }

        std::string lang = !opt.shotLang.empty() ? opt.shotLang
                           : c.save.lang.empty() ? platform::systemLanguage() : c.save.lang;
        if (!c.setLanguage(lang == "fa" ? "fa" : "en")) c.setLanguage("en");
        c.setTheme(c.save.theme);
        if (!shot) {
            c.audio.init();
            c.applySettings();
            c.save.store();
        }

        const SceneId first = shot ? sceneFromName(opt.shotScene, c) : SceneId::Menu;
        std::unique_ptr<Scene> scene = makeScene(first, c);
        float cover = shot ? 0.f : 1.f;  // blinds transition: 1 = fully covered
        bool closing = false;
        int frame = 0;
        Uint64 last = SDL_GetPerformanceCounter();
        const double freq = double(SDL_GetPerformanceFrequency());
        constexpr float kBlinds = 0.22f;

        while (!c.quit) {
            c.in.beginFrame();
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                switch (e.type) {
                    case SDL_QUIT: c.quit = true; break;
                    case SDL_MOUSEMOTION: c.in.mx = float(e.motion.x); c.in.my = float(e.motion.y); break;
                    case SDL_MOUSEBUTTONDOWN:
                        if (e.button.button == SDL_BUTTON_LEFT) {
                            c.in.mx = float(e.button.x); c.in.my = float(e.button.y);
                            c.in.down = c.in.pressed = true;
                        }
                        break;
                    case SDL_MOUSEBUTTONUP:
                        if (e.button.button == SDL_BUTTON_LEFT) {
                            c.in.mx = float(e.button.x); c.in.my = float(e.button.y);
                            c.in.down = false;
                            c.in.released = true;
                        }
                        break;
                    case SDL_KEYDOWN:
                        if (e.key.repeat) break;
                        switch (e.key.keysym.scancode) {
                            case SDL_SCANCODE_ESCAPE:
                            case SDL_SCANCODE_AC_BACK: c.in.esc = true; break;
                            case SDL_SCANCODE_RETURN:
                            case SDL_SCANCODE_KP_ENTER: c.in.confirm = true; break;
                            case SDL_SCANCODE_SPACE: c.in.confirm = c.in.drop = true; break;
                            case SDL_SCANCODE_UP:
                            case SDL_SCANCODE_W: c.in.up = true; break;
                            case SDL_SCANCODE_DOWN:
                            case SDL_SCANCODE_S: c.in.dn = c.in.drop = true; break;
                            case SDL_SCANCODE_F11:
                                c.save.fullscreen = !c.save.fullscreen;
                                c.applySettings();
                                break;
                            default: break;
                        }
                        break;
                    case SDL_APP_WILLENTERBACKGROUND:
                    case SDL_APP_TERMINATING: if (!shot) c.save.store(); break;
                    default: break;
                }
            }
            const Uint8* ks = SDL_GetKeyboardState(nullptr);
            c.in.left = ks[SDL_SCANCODE_LEFT] || ks[SDL_SCANCODE_A];
            c.in.right = ks[SDL_SCANCODE_RIGHT] || ks[SDL_SCANCODE_D];

            float dt;
            if (shot) {
                dt = 1.f / 60.f;  // deterministic simulated time
                if (first == SceneId::Game) scriptedInput(c.in, frame);
            } else {
                const Uint64 now = SDL_GetPerformanceCounter();
                dt = std::min(0.05f, float(double(now - last) / freq));
                last = now;
            }
            c.time += dt;

            c.ui.beginFrame();
            scene->frame(c, dt);
            c.ui.endFrame();

            // Blinds transition between scenes.
            if (c.hasNext && !closing) closing = true;
            if (closing) {
                cover += dt / kBlinds;
                if (cover >= 1.f) {
                    cover = 1.f;
                    c.hasNext = false;
                    closing = false;
                    c.ui.resetFocus();
                    scene = makeScene(c.next, c);
                }
            } else if (cover > 0.f) {
                cover = std::max(0.f, cover - dt / kBlinds);
            }
            if (cover > 0.f) c.gfx.blinds(cover, c.theme->ink);

            // Smoothed FPS counter.
            static float fpsAvg = 60.f;
            if (dt > 0.f) fpsAvg += (1.f / dt - fpsAvg) * 0.05f;
            if (c.save.showFps && !shot)
                c.gfx.number(std::to_string(int(fpsAvg + 0.5f)), 4, 472, 1, c.theme->accent, Align::Left, TextFx::Outline, c.theme->ink);

            ++frame;
            if (shot && frame >= opt.shotFrames) {
                if (!c.gfx.screenshot(opt.shotPath)) {
                    SDL_Log("Screenshot failed: %s", SDL_GetError());
                    exitCode = 2;
                }
                c.quit = true;
            }
            c.gfx.present();

            // Frame cap (VSync handled by the renderer when the setting is VSync).
            const int cap = c.save.fpsLimit();
            if (!shot && cap > 0) {
                const double target = 1.0 / double(cap);
                for (;;) {
                    const double el = double(SDL_GetPerformanceCounter() - last) / freq;
                    if (el >= target) break;
                    if (target - el > 0.002) SDL_Delay(1);
                }
            }
        }
        if (!shot) c.save.store();
        scene.reset();
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}

}  // namespace liq
