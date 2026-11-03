#include "save/Save.hpp"

#include <cstdlib>
#include <sstream>

#include "core/Platform.hpp"

namespace liq {

std::string Save::path() const { return pathOverride().empty() ? platform::prefPath() + "save.txt" : pathOverride(); }

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
        } else if (k == "music") musicVol = float(std::atof(v.c_str()));
        else if (k == "sfx") sfxVol = float(std::atof(v.c_str()));
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
        else if (k == "fps") fpsIdx = iv;
        else if (k == "showFps") showFps = iv != 0;
        else if (k == "haptics") haptics = iv != 0;
        else if (k == "guide") guide = iv != 0;
        else if (k == "shake") shake = iv != 0;
        else if (k == "slowmo") slowmo = iv != 0;
        else if (k == "calm") calm = iv != 0;
        else if (k == "colorVision") colorVision = iv;
        else if (k == "highContrast") highContrast = iv != 0;
        else if (k == "reduceMotion") reduceMotion = iv != 0;
        else if (k == "leftHand") leftHand = iv != 0;
        else if (k == "bigTouch") bigTouch = iv != 0;
        else if (k == "altLang") altLang = v;
        else if (k == "modifier") modifier = iv;
        else if (k == "weeklyWeek") weeklyWeek = iv;
        else if (k == "weeklyBest") weeklyBest = iv;
        else if (k == "seenModes") seenModes = uint32_t(std::strtoul(v.c_str(), nullptr, 10));
        else if (k == "puzzle") {
            for (size_t i = 0; i < v.size() && i < size_t(kPuzzleMax); ++i) puzzleStars[i] = (unsigned char)(v[i] >= '0' && v[i] <= '3' ? v[i] - '0' : 0);
        } else if (k == "gamesPlayed") gamesPlayed = iv;
        else if (k == "totalDrops") totalDrops = iv;
        else if (k == "totalMerges") totalMerges = iv;
        else if (k == "longestChain") longestChain = iv;
        else if (k == "highestLevel") highestLevel = iv;
        else if (k == "playSeconds") playSeconds = iv;
    }
    if (musicVol < 0.f || musicVol > 1.f) musicVol = 0.6f;
    if (sfxVol < 0.f || sfxVol > 1.f) sfxVol = 0.9f;
    if (modifier < 0 || modifier > 4) modifier = 0;
    if (colorVision < 0 || colorVision > 2) colorVision = 0;
    if (fpsIdx < 0 || fpsIdx >= kFpsChoiceCount) fpsIdx = 0;
}

void Save::store() const {
    std::ostringstream o;
    for (int i = 0; i < kModeCount; ++i) o << "best" << i << "=" << best[i] << "\n";
    o << "music=" << musicVol << "\nsfx=" << sfxVol << "\nfullscreen=" << fullscreen << "\nlang=" << lang << "\ntheme=" << theme
      << "\nach=" << ach << "\nstreak=" << streak << "\nlastDay=" << lastDay
      << "\ntutorialDone=" << tutorialDone << "\ntargetStage=" << targetStage << "\ndailyDay=" << dailyDay
      << "\ndailyToday=" << dailyToday << "\nfps=" << fpsIdx << "\nshowFps=" << showFps << "\nhaptics=" << haptics
      << "\nguide=" << guide << "\nshake=" << shake << "\nslowmo=" << slowmo << "\ncalm=" << calm << "\ncolorVision=" << colorVision
      << "\nhighContrast=" << highContrast << "\nreduceMotion=" << reduceMotion << "\nleftHand=" << leftHand
      << "\nbigTouch=" << bigTouch << "\naltLang=" << altLang << "\n";
    o << "modifier=" << modifier << "\nweeklyWeek=" << weeklyWeek << "\nweeklyBest=" << weeklyBest << "\nseenModes=" << seenModes << "\npuzzle=";
    for (int i = 0; i < kPuzzleMax; ++i) o << char('0' + puzzleStars[i]);
    o << "\ngamesPlayed=" << gamesPlayed << "\ntotalDrops=" << totalDrops << "\ntotalMerges=" << totalMerges
      << "\nlongestChain=" << longestChain << "\nhighestLevel=" << highestLevel << "\nplaySeconds=" << playSeconds << "\n";
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
