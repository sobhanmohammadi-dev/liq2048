#pragma once
// Platform abstraction: asset/storage paths, locale, calendar day.
#include <string>
#include <vector>

namespace liq::platform {

std::vector<unsigned char> readFile(const std::string& path);
// Looks in <exe>/assets/, ./assets/ and the bare path (Android APK assets).
std::vector<unsigned char> readAsset(const std::string& rel);
bool writeFile(const std::string& path, const std::string& data);
bool ensureDir(const std::string& path);  // creates the directory (and parents) if needed
// Names (not paths) of the regular files in `dir` that end with `ext` (for example ".liqtheme"), sorted, at most `max`.
std::vector<std::string> listFiles(const std::string& dir, const std::string& ext, int max = 64);
std::string prefPath();          // writable per-user dir (save file)
std::string systemLanguage();    // "en", "fa", ...
bool isMobile();
// Motion sensors (phones). accel() gives the accelerometer in m/s^2 in device axes (x right, y up, z out of the screen);
// returns false when the device has none. shake() is true once per shake of the phone.
bool accel(float& x, float& y, float& z);
bool shake();
void vibrate(float strength, int ms);  // haptic pulse on phones (strength 0..1); no-op elsewhere
int dayNumber();                 // days since Unix epoch (UTC)

}  // namespace liq::platform
