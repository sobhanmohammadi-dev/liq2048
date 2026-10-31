#pragma once
// Milestones and the cosmetic themes they unlock.
namespace liq {

enum Ach {
    kFirst128, kFirst256, kFirst512, kFirst1024, kFirst2048,
    kChain3, kChain5, kScore1k, kScore5k, kAchCount
};

inline const char* achKey(int i) {
    static const char* k[kAchCount] = {"ach.first128", "ach.first256", "ach.first512", "ach.first1024", "ach.first2048",
                                       "ach.chain3",   "ach.chain5",   "ach.score1k",  "ach.score5k"};
    return k[i];
}

// Achievement required to unlock each theme (-1 = free).
inline int themeUnlockAch(int theme) {
    static const int a[4] = {-1, kFirst128, kChain5, kFirst512};
    return a[theme < 0 || theme > 3 ? 0 : theme];
}

}  // namespace liq
