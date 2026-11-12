#include "core/Platform.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <system_error>

namespace liq::platform {

std::vector<unsigned char> readFile(const std::string& path) {
    std::vector<unsigned char> out;
    SDL_RWops* f = SDL_RWFromFile(path.c_str(), "rb");
    if (!f) return out;
    Sint64 n = SDL_RWsize(f);
    if (n > 0) {
        out.resize(size_t(n));
        size_t got = SDL_RWread(f, out.data(), 1, size_t(n));
        out.resize(got);
    }
    SDL_RWclose(f);
    return out;
}

std::vector<unsigned char> readAsset(const std::string& rel) {
    std::vector<std::string> candidates;
    if (char* base = SDL_GetBasePath()) {
        candidates.push_back(std::string(base) + "assets/" + rel);
        SDL_free(base);
    }
    candidates.push_back("assets/" + rel);
    candidates.push_back(rel);
    for (const auto& p : candidates) {
        auto d = readFile(p);
        if (!d.empty()) return d;
    }
    return {};
}

bool writeFile(const std::string& path, const std::string& data) {
    SDL_RWops* f = SDL_RWFromFile(path.c_str(), "wb");
    if (!f) return false;
    SDL_RWwrite(f, data.data(), 1, data.size());
    SDL_RWclose(f);
    return true;
}

bool ensureDir(const std::string& path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    return !ec && std::filesystem::is_directory(path, ec);
}

std::vector<std::string> listFiles(const std::string& dir, const std::string& ext, int max) {
    std::vector<std::string> out;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code e2;
        if (!it->is_regular_file(e2)) continue;
        const std::string n = it->path().filename().string();
        if (n.size() > ext.size() && n.compare(n.size() - ext.size(), ext.size(), ext) == 0) out.push_back(n);
    }
    std::sort(out.begin(), out.end());
    if (int(out.size()) > max) out.resize(size_t(max));
    return out;
}

std::string prefPath() {
    char* p = SDL_GetPrefPath("sobhanmohammadi-dev", "Liq2048");
    std::string s = p ? p : "";
    if (p) SDL_free(p);
    return s;
}

std::string systemLanguage() {
    std::string s = "en";
    SDL_Locale* l = SDL_GetPreferredLocales();
    if (l) {
        if (l[0].language) s = l[0].language;
        SDL_free(l);
    }
    return s;
}

bool isMobile() {
    const char* p = SDL_GetPlatform();
    return p && (std::strcmp(p, "Android") == 0 || std::strcmp(p, "iOS") == 0);
}

int dayNumber() { return int(std::time(nullptr) / 86400); }

namespace {
SDL_Sensor* accelSensor() {
    static SDL_Sensor* s = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        if (const char* fake = SDL_getenv("LIQ_NO_SENSOR")) (void)fake;
        else if (SDL_InitSubSystem(SDL_INIT_SENSOR) == 0) {
            const int n = SDL_NumSensors();
            for (int i = 0; i < n && !s; ++i)
                if (SDL_SensorGetDeviceType(i) == SDL_SENSOR_ACCEL) s = SDL_SensorOpen(i);
        }
    }
    return s;
}
}  // namespace

bool accel(float& x, float& y, float& z) {
    SDL_Sensor* s = accelSensor();
    float d[3] = {0, 0, 0};
    if (!s || SDL_SensorGetData(s, d, 3) != 0) return false;
    x = d[0];
    y = d[1];
    z = d[2];
    return true;
}

// A shake is a burst of acceleration well above gravity; the cooldown stops one shake from counting twice.
bool shake() {
    static Uint32 last = 0;
    float x, y, z;
    if (!accel(x, y, z)) return false;
    const float mag = std::sqrt(x * x + y * y + z * z);
    const Uint32 now = SDL_GetTicks();
    if (std::fabs(mag - 9.81f) > 14.f && now - last > 900) {
        last = now;
        return true;
    }
    return false;
}

void vibrate(float strength, int ms) {
    if (!isMobile()) return;
    static SDL_Haptic* haptic = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        if (SDL_InitSubSystem(SDL_INIT_HAPTIC) == 0 && SDL_NumHaptics() > 0) {
            haptic = SDL_HapticOpen(0);
            if (haptic && SDL_HapticRumbleInit(haptic) != 0) {
                SDL_HapticClose(haptic);
                haptic = nullptr;
            }
        }
    }
    if (haptic) SDL_HapticRumblePlay(haptic, strength < 0.f ? 0.f : (strength > 1.f ? 1.f : strength), Uint32(ms));
}

}  // namespace liq::platform
