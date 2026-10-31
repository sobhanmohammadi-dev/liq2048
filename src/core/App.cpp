#include "core/App.hpp"

#include <SDL.h>

#include <algorithm>
#include <memory>

#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "scenes/Scenes.hpp"

namespace liq {

namespace {

SceneId sceneFromName(const std::string& n) {
    if (n == "modes") return SceneId::ModeSelect;
    if (n == "progress") return SceneId::Progress;
    if (n == "settings") return SceneId::Settings;
    if (n == "game") return SceneId::Game;
    return SceneId::Menu;
}

// Scripted input for headless captures: a drop roughly every half second at
// varying x positions, so the "game" screenshot shows a lived-in board.
void scriptedInput(Input& in, int frame) {
    if (frame % 30 == 5) {
        in.mx = 60.f + float((frame / 30) * 83 % 240);
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
    if (shot && !SDL_getenv("SDL_VIDEODRIVER")) SDL_setenv("SDL_VIDEODRIVER", "dummy", 0);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (platform::isMobile()) flags |= SDL_WINDOW_FULLSCREEN;
    // Screenshots use exactly 2x the logical size for pixel-perfect output.
    const int ww = shot ? cfg::kW * 2 : 405, wh = shot ? cfg::kH * 2 : 720;
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
        if (!c.gfx.init(window)) {
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

        SceneId first = shot ? sceneFromName(opt.shotScene) : SceneId::Menu;
        std::unique_ptr<Scene> scene = makeScene(first, c);
        float fade = shot ? 0.f : 1.f;
        bool fadingOut = false;
        int frame = 0;
        Uint64 last = SDL_GetPerformanceCounter();

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
                Uint64 now = SDL_GetPerformanceCounter();
                dt = std::min(0.05f, float(double(now - last) / double(SDL_GetPerformanceFrequency())));
                last = now;
            }

            c.ui.beginFrame();
            scene->frame(c, dt);
            c.ui.endFrame();

            // Smooth fade between scenes.
            if (c.hasNext && !fadingOut) fadingOut = true;
            if (fadingOut) {
                fade += dt / 0.15f;
                if (fade >= 1.f) {
                    fade = 1.f;
                    c.hasNext = false;
                    fadingOut = false;
                    c.ui.resetFocus();
                    scene = makeScene(c.next, c);
                }
            } else if (fade > 0.f) {
                fade = std::max(0.f, fade - dt / 0.15f);
            }
            if (fade > 0.f) c.gfx.rect(0, 0, cfg::kW, cfg::kH, Color{0, 0, 0, Uint8(255 * fade)});

            ++frame;
            if (shot && frame >= opt.shotFrames) {
                if (!c.gfx.screenshot(opt.shotPath)) {
                    SDL_Log("Screenshot failed: %s", SDL_GetError());
                    exitCode = 2;
                }
                c.quit = true;
            }
            c.gfx.present();
        }
        if (!shot) c.save.store();
        scene.reset();
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}

}  // namespace liq
