#pragma once
// Platform abstraction: asset/storage paths, locale, calendar day.
#include <string>
#include <vector>

namespace liq::platform {

std::vector<unsigned char> readFile(const std::string& path);
// Looks in <exe>/assets/, ./assets/ and the bare path (Android APK assets).
std::vector<unsigned char> readAsset(const std::string& rel);
bool writeFile(const std::string& path, const std::string& data);
std::string prefPath();          // writable per-user dir (save file)
std::string systemLanguage();    // "en", "fa", ...
bool isMobile();
void vibrate(float strength, int ms);  // haptic pulse on phones (strength 0..1); no-op elsewhere
int dayNumber();                 // days since Unix epoch (UTC)

}  // namespace liq::platform
