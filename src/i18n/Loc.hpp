#pragma once
// Localization: JSON string tables in assets/lang/<code>.json. English is always
// loaded as the fallback so a missing key never shows blank text.
#include <string>
#include <unordered_map>

namespace liq {

// Supported languages. `arabicScript` languages are drawn with the Vazirmatn TTF (assets/fonts/fa.ttf);
// all others use the built-in pixel font (accents are folded to base letters, see PixelFont::ascii).
struct LangInfo {
    const char* code;
    const char* name;  // shown in the language picker (pixel-font safe)
    bool rtl;
    bool arabicScript;
};
inline constexpr int kLangCount = 8;
inline const LangInfo* langInfo(const std::string& code) {
    static const LangInfo t[kLangCount] = {
        {"en", "ENGLISH", false, false}, {"fa", "FARSI", true, true},      {"es", "ESPANOL", false, false},
        {"fr", "FRANCAIS", false, false}, {"de", "DEUTSCH", false, false}, {"tr", "TURKCE", false, false},
        {"pt", "PORTUGUES", false, false}, {"ar", "ARABIC", true, true}};
    for (const auto& l : t)
        if (code == l.code) return &l;
    return nullptr;
}
inline const LangInfo& langAt(int i) { return *langInfo(i == 1 ? "fa" : i == 2 ? "es" : i == 3 ? "fr" : i == 4 ? "de" : i == 5 ? "tr" : i == 6 ? "pt" : i == 7 ? "ar" : "en"); }

class Loc {
public:
    void load(const std::string& code);
    const std::string& tr(const std::string& key) const;
    std::string fmt(const std::string& key, const std::string& arg) const;  // replaces {0}
    bool rtl() const {
        const LangInfo* l = langInfo(code_);
        return l && l->rtl;
    }
    const std::string& code() const { return code_; }

private:
    std::string code_ = "en";
    std::unordered_map<std::string, std::string> base_, cur_;
};

}  // namespace liq
