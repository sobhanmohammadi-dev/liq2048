#pragma once
// The player's own theme: every colour plus the sound of every slot, a name in English (required) and optional
// names in other languages. It is stored in a `.liqtheme` file, which is also the export / import format.
//
// File layout (little endian):
//   "LIQTHEME" | u32 version (1) | u32 headerLength | header (text, "key=value" lines) | audio files | u32 CRC-32
// The CRC covers everything before it. The header holds name=, name.<lang>=, c0..c33=RRGGBB and slot0..slot11=
// either "pal:N" (built-in palette N) or "file:<bytes>:<file name>"; the audio files follow in slot order.
#include <map>
#include <string>
#include <vector>

#include "audio/Audio.hpp"
#include "core/Themes.hpp"

namespace liq {

constexpr int kCustomTheme = 4;                 // theme index of the custom theme
constexpr int kThemeSlots = kThemeCount + 1;    // built-in themes + the custom one
constexpr int kThemeColors = 34;                // editable colours

struct CustomTheme {
    Theme theme = themeAt(0);                   // the colours (key is "theme.custom")
    std::string nameEn = "My theme";            // required, ASCII
    std::map<std::string, std::string> names;   // language code -> name (UTF-8), optional
    AudioPack pack;                             // sound of each slot
    std::string fileName[kAudioSlots];          // original file names of custom sounds (display only)

    CustomTheme();
    // Name for a language, falling back to English.
    std::string displayName(const std::string& lang) const;
    // Copies colours and sounds of built-in theme i (0..3) and, when withName, its English name.
    void fromBuiltin(int i, bool withName);
    bool hasFiles() const;

    std::vector<unsigned char> serialize() const;
    // Strict parser: any malformed, truncated, oversized or corrupted input is rejected and `out` stays unchanged.
    static bool parse(const unsigned char* data, size_t size, CustomTheme& out);
    // Text form for the clipboard (only for themes without audio files).
    std::string toClipboard() const;
    static bool fromClipboard(const std::string& text, CustomTheme& out);

    static Color& colorAt(Theme& t, int i);     // i in 0..kThemeColors-1
    static const char* colorKey(int i);         // localization key of colour i, "ct.c.*"
    static std::string cleanEnglish(const std::string& s);   // ASCII printable, trimmed, at most 24 characters
    static std::string cleanLocal(const std::string& s);     // valid UTF-8 without control characters, at most 48 bytes
    static const char* slotKey(int slot);       // localization key of a sound slot, "ct.s.*"
    static const char* slotFile(int slot);      // file stem used for custom sounds ("menu", "drop", ...)
};

uint32_t crc32(const unsigned char* d, size_t n);
std::string base64Encode(const unsigned char* d, size_t n);
bool base64Decode(const std::string& s, std::vector<unsigned char>& out);

}  // namespace liq
