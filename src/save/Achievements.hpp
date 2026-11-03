#pragma once
// Milestones and the cosmetic themes they unlock.
namespace liq {

enum Ach {
    kFirst128, kFirst256, kFirst512, kFirst1024, kFirst2048,
    kChain3, kChain5, kScore1k, kScore5k,
    kRain60, kTide90, kWeekly, kPuzzle5, kPuzzle3Star, kTargetWin, kDrops1000, kChain7, kStreak3, kModded, kAchCount
};

inline const char* achKey(int i) {
    static const char* k[kAchCount] = {"ach.first128", "ach.first256", "ach.first512", "ach.first1024", "ach.first2048",
                                       "ach.chain3",   "ach.chain5",   "ach.score1k",  "ach.score5k",
                                       "ach.rain60",   "ach.tide90",   "ach.weekly",   "ach.puzzle5",  "ach.puzzle3star",
                                       "ach.targetwin", "ach.drops1000", "ach.chain7",  "ach.streak3",  "ach.modded"};
    return k[i];
}

// Achievement required to unlock each theme (-1 = free).
inline int themeUnlockAch(int theme) {
    static const int a[4] = {-1, kFirst128, kChain5, kFirst512};
    return a[theme < 0 || theme > 3 ? 0 : theme];
}

}  // namespace liq
