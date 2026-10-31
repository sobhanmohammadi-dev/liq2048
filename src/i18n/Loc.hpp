#pragma once
// Localization: JSON string tables in assets/lang/<code>.json. English is always
// loaded as the fallback so a missing key never shows blank text.
#include <string>
#include <unordered_map>

namespace liq {

class Loc {
public:
    void load(const std::string& code);
    const std::string& tr(const std::string& key) const;
    std::string fmt(const std::string& key, const std::string& arg) const;  // replaces {0}
    bool rtl() const { return code_ == "fa"; }
    const std::string& code() const { return code_; }

private:
    std::string code_ = "en";
    std::unordered_map<std::string, std::string> base_, cur_;
};

}  // namespace liq
