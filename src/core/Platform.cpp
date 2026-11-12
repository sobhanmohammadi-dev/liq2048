#include "core/Platform.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <system_error>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#elif defined(__ANDROID__)
#include <jni.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <csignal>
#include <cstdio>
#include <cstdlib>
#endif

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

namespace {
#if !defined(__ANDROID__)
std::string picturesDir() {
    namespace fs = std::filesystem;
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    std::error_code ec;
    if (home && *home) {
        for (const char* sub : {"Pictures", "Downloads"}) {
            const fs::path base = fs::path(home) / sub;
            if (fs::is_directory(base, ec)) {
                const fs::path d = base / "Liq2048";
                fs::create_directories(d, ec);
                if (fs::is_directory(d, ec)) return d.string();
            }
        }
    }
    return prefPath();
}
#endif

#if defined(_WIN32)
bool clipboardImage(const std::vector<unsigned char>& png, const unsigned char* rgb, int w, int h) {
    if (!OpenClipboard(nullptr)) return false;
    EmptyClipboard();
    bool ok = false;
    // CF_DIB (bottom-up BGR, rows padded to 4 bytes) is what Paint, Word and chat apps paste.
    const size_t stride = (size_t(w) * 3 + 3) & ~size_t(3);
    HGLOBAL hd = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + stride * size_t(h));
    if (hd) {
        auto* b = static_cast<unsigned char*>(GlobalLock(hd));
        if (b) {
            BITMAPINFOHEADER bi{};
            bi.biSize = sizeof(bi);
            bi.biWidth = w;
            bi.biHeight = h;
            bi.biPlanes = 1;
            bi.biBitCount = 24;
            bi.biCompression = BI_RGB;
            std::memcpy(b, &bi, sizeof(bi));
            unsigned char* px = b + sizeof(bi);
            std::memset(px, 0, stride * size_t(h));
            for (int y = 0; y < h; ++y) {
                unsigned char* o = px + size_t(h - 1 - y) * stride;
                const unsigned char* i = rgb + size_t(y) * size_t(w) * 3;
                for (int x = 0; x < w; ++x) {
                    o[x * 3] = i[x * 3 + 2];
                    o[x * 3 + 1] = i[x * 3 + 1];
                    o[x * 3 + 2] = i[x * 3];
                }
            }
            GlobalUnlock(hd);
            ok = SetClipboardData(CF_DIB, hd) != nullptr;
        }
        if (!ok) GlobalFree(hd);
    }
    const UINT fmt = RegisterClipboardFormatA("PNG");
    HGLOBAL hp = png.empty() ? nullptr : GlobalAlloc(GMEM_MOVEABLE, png.size());
    if (hp) {
        void* d = GlobalLock(hp);
        if (d) {
            std::memcpy(d, png.data(), png.size());
            GlobalUnlock(hp);
            if (!SetClipboardData(fmt, hp)) GlobalFree(hp);
        } else {
            GlobalFree(hp);
        }
    }
    CloseClipboard();
    return ok;
}
#elif defined(__ANDROID__)
// handled in exportImage through LiqActivity.exportImage (MediaStore insert + clipboard URI)
#elif defined(__APPLE__)
bool clipboardImage(const std::string& pngPath) {
    if (pngPath.find_first_of("\"\\'") != std::string::npos) return false;
    const std::string cmd = "osascript -e 'set the clipboard to (read (POSIX file \"" + pngPath + "\") as \xC2\xABclass PNGf\xC2\xBB)' >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}
#elif defined(__unix__)
bool clipboardImage(const std::vector<unsigned char>& png) {
    if (png.empty()) return false;
    void (*old)(int) = std::signal(SIGPIPE, SIG_IGN);
    bool ok = false;
    // Wayland first, then X11; both tools read the PNG from stdin and keep serving it in the background.
    for (const char* cmd : {"wl-copy --type image/png 2>/dev/null", "xclip -selection clipboard -t image/png -i 2>/dev/null"}) {
        FILE* f = popen(cmd, "w");
        if (!f) continue;
        const size_t n = std::fwrite(png.data(), 1, png.size(), f);
        const int rc = pclose(f);
        if (n == png.size() && rc == 0) {
            ok = true;
            break;
        }
    }
    std::signal(SIGPIPE, old);
    return ok;
}
#endif
}  // namespace

int exportImage(const std::string& pngPath, const std::string& name, const unsigned char* rgb, int w, int h) {
    int res = 0;
#if defined(__ANDROID__)
    (void)rgb; (void)w; (void)h;
    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject act = static_cast<jobject>(SDL_AndroidGetActivity());
    if (env && act) {
        jclass cls = env->GetObjectClass(act);
        jmethodID m = cls ? env->GetMethodID(cls, "exportImage", "(Ljava/lang/String;Ljava/lang/String;)I") : nullptr;
        if (m) {
            jstring jp = env->NewStringUTF(pngPath.c_str());
            jstring jn = env->NewStringUTF(name.c_str());
            res = int(env->CallIntMethod(act, m, jp, jn));
            env->DeleteLocalRef(jp);
            env->DeleteLocalRef(jn);
        }
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            res = 0;
        }
        if (cls) env->DeleteLocalRef(cls);
        env->DeleteLocalRef(act);
    }
    if (!(res & kImgSaved) && !readFile(pngPath).empty()) res |= kImgSaved;  // at least the private copy exists
#else
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path dst = fs::path(picturesDir()) / name;
    if (fs::path(pngPath) == dst || fs::copy_file(pngPath, dst, fs::copy_options::overwrite_existing, ec)) res |= kImgSaved;
    const std::vector<unsigned char> png = readFile(pngPath);
#if defined(_WIN32)
    if (!png.empty() && rgb && w > 0 && h > 0 && clipboardImage(png, rgb, w, h)) res |= kImgCopied;
#elif defined(__APPLE__)
    (void)rgb; (void)w; (void)h; (void)png;
    if (clipboardImage(pngPath)) res |= kImgCopied;
#elif defined(__unix__)
    (void)rgb; (void)w; (void)h;
    if (clipboardImage(png)) res |= kImgCopied;
#else
    (void)rgb; (void)w; (void)h; (void)png;
#endif
#endif
    return res;
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
