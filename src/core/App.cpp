#define _CRT_SECURE_NO_WARNINGS

#include "core/App.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <vector>
#include <memory>

#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "scenes/Scenes.hpp"

namespace liq {

namespace {

SceneId sceneFromName(const std::string& n, Ctx& c) {
    if (n == "modes") return SceneId::ModeSelect;
    if (n == "progress") return SceneId::Progress;
    if (n == "puzzles") return SceneId::Puzzle;
    if (n == "settings") return SceneId::Settings;
    if (n == "game") return SceneId::Game;
    if (n == "credits") return SceneId::Credits;
    if (n == "battle") return SceneId::Battle;
    if (n == "mirror" || n == "bigbox") {  // dev: the party modes
        c.mode = n == "mirror" ? ModeId::Mirror : ModeId::BigBox;
        return SceneId::Party;
    }
    if (n == "hotseat") {
        c.mode = ModeId::Classic;
        c.hotseat = true;
        return SceneId::Game;
    }
    if (n == "modes") return SceneId::ModeSelect;
    if (n == "extras") return SceneId::Extras;
    if (n == "card") return SceneId::Card;
    if (n == "themeeditor") return SceneId::ThemeEditor;
    if (n == "replaygame") {  // dev: watch a made-up run code
        Replay r;
        r.mode = int(ModeId::Classic);
        r.seed = 5;
        r.gravity = cfg::kGravity;
        r.endStep = 1500;
        r.outcome = 3;
        for (uint32_t i = 0; i < 30; ++i) r.drops.push_back({i * 40u + 10u, 40.f + float((i * 53u) % 180u), int(i % 3u)});
        c.pendingReplay = r;
        c.hasPendingReplay = true;
        return SceneId::Game;
    }
    if (n == "battlegame") {  // dev: a bot match with scripted input
        c.mode = ModeId::Battle;
        c.battle.cfg.seed = 7;
        c.battle.cfg.myName = "You";
        c.battle.cfg.oppName = "Medium";
        return SceneId::Game;
    }
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
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);  // optional: no gamepad support is not an error
    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (platform::isMobile()) flags |= SDL_WINDOW_FULLSCREEN;
    int ww = shot ? cfg::kW * 2 : cfg::kW * 3, wh = shot ? cfg::kH * 2 : cfg::kH * 3 / 2 * 2;
    if (shot)  // dev: LIQ_SHOT_SIZE=720x1600 previews phones with a different aspect ratio
        if (const char* sz = SDL_getenv("LIQ_SHOT_SIZE")) {std::sscanf(sz, "%dx%d", &ww, &wh);}
    SDL_Window* window = SDL_CreateWindow("Liq2048", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh, flags);
    if (!window) {
        SDL_Log("Window failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    if (!platform::isMobile()) {  // window/taskbar icon from assets/icon.bmp
        const auto icon = platform::readAsset("icon.bmp");
        if (!icon.empty()) {
            if (SDL_Surface* s = SDL_LoadBMP_RW(SDL_RWFromConstMem(icon.data(), int(icon.size())), 1)) {
                SDL_SetWindowIcon(window, s);
                SDL_FreeSurface(s);
            }
        }
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
        if (!langInfo(lang) || !c.setLanguage(lang)) c.setLanguage("en");
        if (c.save.altLang.empty() || !langInfo(c.save.altLang)) c.save.altLang = langInfo(lang) && lang != "en" ? lang : "fa";
        if (shot && !opt.shotSet.empty()) {  // dev overrides for screenshots
            std::string rest = opt.shotSet + ",";
            for (size_t p; (p = rest.find(',')) != std::string::npos; rest.erase(0, p + 1)) {
                const std::string kv = rest.substr(0, p);
                const size_t eq = kv.find('=');
                if (eq == std::string::npos) continue;
                const std::string k = kv.substr(0, eq);
                const int v = std::atoi(kv.c_str() + eq + 1);
                Save& s = c.save;
                if (k == "colorVision") s.colorVision = v;
                else if (k == "highContrast") s.highContrast = v != 0;
                else if (k == "leftHand") s.leftHand = v != 0;
                else if (k == "reduceMotion") s.reduceMotion = v != 0;
                else if (k == "theme") s.theme = v;
                else if (k == "mode") c.mode = ModeId(std::min(std::max(v, 0), kModeCount - 1));
                else if (k == "puzzle") c.puzzleLevel = v;
                else if (k == "modifier") s.modifier = v;
                else if (k == "liquid") s.liquid = v;
            }
        }
        if (shot) setSettingsPage(opt.shotPage);
        c.loadCustom();
        c.setTheme(c.save.theme);
        if (!shot) {
            c.audio.init();
            c.applySettings();
            c.save.store();
        }

        const SceneId first = shot ? sceneFromName(opt.shotScene, c) : SceneId::Menu;
        std::unique_ptr<Scene> scene = makeScene(first, c);
        c.audio.setMood(first == SceneId::Game ? MusicMood::Game : MusicMood::Menu);
        float cover = shot ? 0.f : 1.f;  // blinds transition: 1 = fully covered
        bool closing = false, hadFocus = false;
        int frame = 0;
        Uint64 last = SDL_GetPerformanceCounter();
        const double freq = double(SDL_GetPerformanceFrequency());
        constexpr float kBlinds = 0.22f;

        std::vector<SDL_GameController*> pads;
        bool perfOverlay = false;
        float frameMs[90] = {};
        int frameIdx = 0;
        while (!c.quit) {
            c.in.beginFrame();
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                switch (e.type) {
                    case SDL_QUIT: c.quit = true; break;
                    case SDL_TEXTINPUT: c.in.text += e.text.text; break;
                    case SDL_DROPFILE:
                        if (e.drop.file) {
                            if (c.in.dropped.size() < 8) c.in.dropped.push_back(e.drop.file);
                            SDL_free(e.drop.file);
                        }
                        break;
                    case SDL_MOUSEWHEEL: c.in.wheel += float(e.wheel.y) * (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.f : 1.f); break;
                    case SDL_CONTROLLERDEVICEADDED:
                        if (SDL_IsGameController(e.cdevice.which))
                            if (SDL_GameController* gc = SDL_GameControllerOpen(e.cdevice.which)) pads.push_back(gc);
                        break;
                    case SDL_CONTROLLERDEVICEREMOVED:
                        for (size_t k = 0; k < pads.size(); ++k)
                            if (SDL_GameControllerFromInstanceID(e.cdevice.which) == pads[k]) {
                                SDL_GameControllerClose(pads[k]);
                                pads.erase(pads.begin() + long(k));
                                break;
                            }
                        break;
                    case SDL_CONTROLLERBUTTONDOWN:
                        switch (e.cbutton.button) {
                            case SDL_CONTROLLER_BUTTON_A: c.in.confirm = c.in.drop = true; break;
                            case SDL_CONTROLLER_BUTTON_B:
                            case SDL_CONTROLLER_BUTTON_START:
                            case SDL_CONTROLLER_BUTTON_BACK: c.in.esc = true; break;
                            case SDL_CONTROLLER_BUTTON_X: c.in.undo = true; break;
                            case SDL_CONTROLLER_BUTTON_DPAD_UP: c.in.up = true; break;
                            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: c.in.dn = c.in.drop = true; break;
                            default: break;
                        }
                        break;
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
                    case SDL_WINDOWEVENT:
                        if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) hadFocus = true;
                        if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST && hadFocus && !shot) c.in.focusLost = true;
                        if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) hadFocus = false;
                        break;
                    case SDL_KEYDOWN:
                        if (e.key.repeat) break;
                        switch (e.key.keysym.scancode) {
                            case SDL_SCANCODE_ESCAPE:
                            case SDL_SCANCODE_AC_BACK: c.in.esc = true; break;
                            case SDL_SCANCODE_BACKSPACE: c.in.backspace = true; break;
                            case SDL_SCANCODE_RETURN:
                            case SDL_SCANCODE_KP_ENTER: c.in.confirm = true; break;
                            case SDL_SCANCODE_SPACE: c.in.confirm = c.in.drop = true; break;
                            case SDL_SCANCODE_U:
                            case SDL_SCANCODE_Z: c.in.undo = true; break;
                            case SDL_SCANCODE_UP:
                            case SDL_SCANCODE_W: c.in.up = true; break;
                            case SDL_SCANCODE_DOWN:
                            case SDL_SCANCODE_S: c.in.dn = c.in.drop = true; break;
                            case SDL_SCANCODE_F3: perfOverlay = !perfOverlay; break;
                            case SDL_SCANCODE_F11:
                                c.save.fullscreen = !c.save.fullscreen;
                                c.applySettings();
                                break;
                            default: break;
                        }
                        break;
                    case SDL_APP_WILLENTERBACKGROUND:
                        c.in.focusLost = true;
                        if (!shot) c.save.store();
                        c.audio.pause(true);
                        break;
                    case SDL_APP_DIDENTERFOREGROUND: c.audio.pause(false); break;
                    case SDL_APP_TERMINATING: if (!shot) c.save.store(); break;
                    default: break;
                }
            }
            const Uint8* ks = SDL_GetKeyboardState(nullptr);
            c.in.left = ks[SDL_SCANCODE_LEFT] || ks[SDL_SCANCODE_A];
            c.in.right = ks[SDL_SCANCODE_RIGHT] || ks[SDL_SCANCODE_D];
            for (SDL_GameController* gc : pads) {  // d-pad or left stick steers the aim
                const int ax = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX);
                if (ax < -12000 || SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) c.in.left = true;
                if (ax > 12000 || SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) c.in.right = true;
            }

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

            c.gfx.fillBars(*c.theme);
            c.ui.beginFrame();
            c.ui.setMirror(false);  // the game scene turns this on for the left-handed layout
            scene->frame(c, dt);
            c.ui.endFrame();

            // Blinds transition between scenes.
            if (c.hasNext && !closing && c.save.reduceMotion) {  // reduced motion: switch scenes instantly
                c.hasNext = false;
                c.ui.resetFocus();
                scene = makeScene(c.next, c);
                c.audio.setIntensity(0.f);
                c.audio.setMood(c.next == SceneId::Game ? MusicMood::Game : MusicMood::Menu);
            }
            if (c.hasNext && !closing) {
                closing = true;
                c.audio.play(Sfx::Swipe);
            }
            if (closing) {
                cover += dt / kBlinds;
                if (cover >= 1.f) {
                    cover = 1.f;
                    c.hasNext = false;
                    closing = false;
                    c.ui.resetFocus();
                    scene = makeScene(c.next, c);
                    c.audio.setIntensity(0.f);
                    c.audio.setMood(c.next == SceneId::Game ? MusicMood::Game : MusicMood::Menu);
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

            if (perfOverlay && !shot) {  // F3: frame-time graph (bars: 0..33 ms, line = 16.7 ms)
                frameMs[frameIdx % 90] = dt * 1000.f;
                for (int k = 0; k < 90; ++k) {
                    const float ms = frameMs[(frameIdx + 1 + k) % 90];
                    const int h = std::min(40, int(ms * 1.2f));
                    c.gfx.rect(4.f + float(k) * 2.f, float(440 - h), 2, float(h), ms > 20.f ? c.theme->danger : c.theme->accent);
                }
                c.gfx.rect(4.f, 440.f - 20.f, 180, 1, c.theme->text);
                c.gfx.number(std::to_string(int(dt * 1000.f + 0.5f)) + "MS", 4, 444, 1, c.theme->text, Align::Left, TextFx::Outline, c.theme->ink);
                ++frameIdx;
            }
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
