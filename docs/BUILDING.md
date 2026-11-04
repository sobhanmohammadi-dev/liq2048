# Building

Requirements: CMake ≥ 3.21, a C++17 compiler, SDL2 and SDL2_ttf.
Presets are defined in [`CMakePresets.json`](../CMakePresets.json).

## CMake options

| Option | Default | Meaning |
|--------|---------|---------|
| `LIQ_FETCH_DEPS` | `OFF` (forced `ON` for Android/iOS) | Download and build SDL2 and SDL2_ttf with FetchContent instead of `find_package` |
| `LIQ_BUILD_TESTS` | `ON` | Build `liq_tests` and register it with CTest |
| `LIQ_WARNINGS_AS_ERRORS` | `OFF` (ON in CI) | Treat warnings as errors in `liq_core` |

## Linux

```bash
sudo apt install cmake ninja-build libsdl2-dev libsdl2-ttf-dev   # Debian/Ubuntu
sudo dnf install cmake ninja-build SDL2-devel SDL2_ttf-devel      # Fedora
cmake --preset linux
cmake --build --preset linux
./build/linux/liq2048
```

## macOS

```bash
brew install cmake ninja sdl2 sdl2_ttf
cmake --preset macos && cmake --build --preset macos
./build/macos/liq2048
```

## Windows

Option A, vcpkg:

```powershell
vcpkg install sdl2 sdl2-ttf --triplet x64-windows
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

Prerequisites: JDK 17, Android SDK with NDK 26.3 (`26.3.11579264`) and CMake 3.22, Gradle 8.9+.

```bash
git clone --depth 1 --branch release-2.30.9 https://github.com/libsdl-org/SDL.git android/SDL   # Java glue + native SDL
gradle -p android assembleRelease          # android/app/build/outputs/apk/release/*.apk
gradle -p android bundleRelease            # .aab for Google Play
```

The Gradle project (`android/`) builds the game through CMake (`LIQ_FETCH_DEPS` is on), packs `assets/` into the
APK, and uses a tiny `LiqActivity` (Java) on top of SDL's `SDLActivity`. No Flutter or Dart is involved.
Signing: set `LIQ_KEYSTORE`, `LIQ_KEYSTORE_PASSWORD`, `LIQ_KEY_ALIAS`, `LIQ_KEY_PASSWORD` to use your own key;
otherwise the debug key is used (installable APK, not suitable for Play). In CI add the repository secrets
`ANDROID_KEYSTORE_BASE64`, `ANDROID_KEYSTORE_PASSWORD`, `ANDROID_KEY_ALIAS`, `ANDROID_KEY_PASSWORD`.

## iOS

Requires macOS with Xcode.

```bash
cmake -S . -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DLIQ_FETCH_DEPS=ON
open build-ios/Liq2048.xcodeproj           # choose your signing team and run on a device
```

The bundle (Info.plist, app icon, `assets/`) is configured in `CMakeLists.txt` and `ios/`. CI produces an
**unsigned** IPA (`CODE_SIGNING_ALLOWED=NO`); sign it yourself or sideload with AltStore/Sideloadly. A paid Apple
developer account is required to publish on the App Store.

Persian and Arabic use Vazirmatn (SIL OFL), which is bundled at `assets/fonts/fa.ttf`.

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
- **No sound**: check the system audio device; in CI/headless runs `SDL_AUDIODRIVER=dummy` is used.
- **Persian shows as `?` or the FA button is greyed out**: no Persian-capable font was found; add `assets/fonts/fa.ttf`.
- **Black window in a VM**: try `SDL_RENDER_DRIVER=software`.
