#include "save/Save.hpp"

#include <cstdlib>
#include <sstream>

#include "core/Platform.hpp"

namespace liq {

std::string Save::path() const { return platform::prefPath() + "save.txt"; }

void Save::load() {
    auto d = platform::readFile(path());
    std::istringstream in(std::string(d.begin(), d.end()));
    std::string line;
    while (std::getline(in, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (!v.empty() && v.back() == '\r') v.pop_back();
        int iv = std::atoi(v.c_str());
        if (k.rfind("best", 0) == 0) {
            int m = std::atoi(k.c_str() + 4);
            if (m >= 0 && m < kModeCount) best[m] = iv;
        } else if (k == "volume") volume = float(std::atof(v.c_str()));
        else if (k == "fullscreen") fullscreen = iv != 0;
        else if (k == "lang") lang = v;
        else if (k == "theme") theme = iv;
        else if (k == "ach") ach = uint32_t(std::strtoul(v.c_str(), nullptr, 10));
        else if (k == "streak") streak = iv;
        else if (k == "lastDay") lastDay = iv;
        else if (k == "tutorialDone") tutorialDone = iv != 0;
        else if (k == "targetStage") targetStage = iv;
        else if (k == "dailyDay") dailyDay = iv;
        else if (k == "dailyToday") dailyToday = iv;
    }
    if (volume < 0.f || volume > 1.f) volume = 0.8f;
}

void Save::store() const {
    std::ostringstream o;
    for (int i = 0; i < kModeCount; ++i) o << "best" << i << "=" << best[i] << "\n";
    o << "volume=" << volume << "\nfullscreen=" << fullscreen << "\nlang=" << lang << "\ntheme=" << theme
      << "\nach=" << ach << "\nstreak=" << streak << "\nlastDay=" << lastDay
      << "\ntutorialDone=" << tutorialDone << "\ntargetStage=" << targetStage << "\ndailyDay=" << dailyDay
      << "\ndailyToday=" << dailyToday << "\n";
    platform::writeFile(path(), o.str());
}

bool Save::unlock(int id) {
    if (has(id)) return false;
    ach |= (1u << id);
    return true;
}

void Save::touchDay(int today) {
    if (lastDay == today) return;
    streak = (lastDay == today - 1) ? streak + 1 : 1;
    lastDay = today;
}

}  // namespace liq
