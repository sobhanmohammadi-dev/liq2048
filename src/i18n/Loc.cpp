#include "i18n/Loc.hpp"

#include "core/Json.hpp"
#include "core/Platform.hpp"

namespace liq {

static std::unordered_map<std::string, std::string> loadTable(const std::string& code) {
    auto d = platform::readAsset("lang/" + code + ".json");
    return parseFlatJson(std::string(d.begin(), d.end()));
}

void Loc::load(const std::string& code) {
    if (base_.empty()) base_ = loadTable("en");
    code_ = code;
    cur_ = code == "en" ? std::unordered_map<std::string, std::string>{} : loadTable(code);
}

const std::string& Loc::tr(const std::string& key) const {
    auto it = cur_.find(key);
    if (it != cur_.end()) return it->second;
    it = base_.find(key);
    if (it != base_.end()) return it->second;
    return key;
}

std::string Loc::fmt(const std::string& key, const std::string& arg) const {
    std::string s = tr(key);
    auto p = s.find("{0}");
    if (p != std::string::npos) s.replace(p, 3, arg);
    return s;
}

}  // namespace liq
