# Building

Requirements: CMake ≥ 3.21, a C++17 compiler, SDL2, SDL2_ttf and (optionally) SDL2_mixer.
Presets are defined in [`CMakePresets.json`](../CMakePresets.json).

## CMake options

| Option | Default | Meaning |
|--------|---------|---------|
| `LIQ_FETCH_DEPS` | `OFF` (forced `ON` for Android/iOS) | Download and build SDL2, SDL2_ttf, SDL2_mixer with FetchContent instead of `find_package` |
| `LIQ_BUILD_TESTS` | `ON` | Build `liq_tests` and register it with CTest |
| `LIQ_WARNINGS_AS_ERRORS` | `OFF` (ON in CI) | Treat warnings as errors in `liq_core` |

## Linux

```bash
sudo apt install cmake ninja-build libsdl2-dev libsdl2-ttf-dev libsdl2-mixer-dev   # Debian/Ubuntu
sudo dnf install cmake ninja-build SDL2-devel SDL2_ttf-devel SDL2_mixer-devel      # Fedora
cmake --preset linux
cmake --build --preset linux
./build/linux/liq2048
```

## macOS

```bash
brew install cmake ninja sdl2 sdl2_ttf sdl2_mixer
cmake --preset macos && cmake --build --preset macos
./build/macos/liq2048
```

## Windows

Option A, vcpkg:

```powershell
vcpkg install sdl2 sdl2-ttf sdl2-mixer --triplet x64-windows
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
cmake --preset windows-vcpkg
cmake --build --preset windows-vcpkg
```

Option B, no package manager (downloads and statically links SDL):

```powershell
cmake --preset windows-fetch
cmake --build --preset windows-fetch
```

## Android

Prerequisites: Android Studio with NDK, `ANDROID_NDK_HOME` set.

```bash
cmake --preset android-arm64
cmake --build build/android-arm64        # produces libmain.so and libSDL2.so
```

Package with SDL's Android Gradle template (`android-project/` in the SDL2 source tree):

1. Copy `libmain.so` and `libSDL2.so` into `app/src/main/jniLibs/arm64-v8a/`.
2. Copy this repo's `assets/` into `app/src/main/assets/`. Asset lookups fall back to the bare relative path, which
   resolves inside the APK.
3. Build with Gradle. Touch input arrives as mouse events; the Back button maps to Esc.

## iOS

```bash
cmake --preset ios
open build/ios/Liq2048.xcodeproj
```

Add the `assets/` folder to the app bundle as a *folder reference*, choose your signing team, build in Xcode.

## Tests

```bash
ctest --test-dir build/linux --output-on-failure
```

Tests need no display, audio device or window. They read `assets/lang/*.json`, so CTest runs them from the source dir.

## Packaging

```bash
cmake --build build/linux --target package   # CPack ZIP + TGZ with assets
```

Pushing a tag `vX.Y.Z` runs the **Release** workflow, which builds, tests, packages for Windows/macOS/Linux and
publishes a GitHub release.

## Headless screenshots

```bash
./build/linux/liq2048 --screenshot shot.bmp --scene game --frames 600 [--lang fa]
```

Scenes: `menu`, `modes`, `progress`, `settings`, `game`. The game scene is driven by scripted input at a fixed
simulated 60 FPS, so output is reproducible. Captures never read or write the player's save.

## Troubleshooting

- **`Could NOT find SDL2_ttf`**: install the dev package or use `-DLIQ_FETCH_DEPS=ON`.
- **No sound**: normal until you add files to `assets/sounds/` (see [ASSETS.md](ASSETS.md)); also needs SDL2_mixer.
- **Persian shows as `?` or the FA button is greyed out**: no Persian-capable font was found; add `assets/fonts/fa.ttf`.
- **Black window in a VM**: try `SDL_RENDER_DRIVER=software`.
