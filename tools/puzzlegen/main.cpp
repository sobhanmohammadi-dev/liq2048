// Generates and verifies the puzzle pack (assets/puzzles/pack1.txt).
//   liq_puzzlegen <out.txt> [levels=30] [attempts=60] [seed=1] [firstIndex=0]
// (firstIndex lets several processes each generate a slice of the difficulty ladder.)
// A candidate is a random pre-filled pot, a random drop queue and a goal tile. A randomised solver (random
// and merge-seeking drop positions) plays each candidate with the real physics; only candidates it can solve
// are kept, and the fewest drops it needed becomes the level's par (3 stars). Difficulty rises with the index.

#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "core/Config.hpp"
#include "core/Util.hpp"
#include "game/Puzzle.hpp"
#include "game/World.hpp"

using namespace liq;

namespace {

constexpr int kStepsPerDrop = 60;

struct Cand {
    PuzzleDef def;
};

World makePot(const PuzzleDef& d) {
    World w;
    for (const auto& p : d.pot) w.spawn(p.x, p.y, p.level);
    for (int i = 0; i < 150; ++i) w.step(cfg::kStep, cfg::kGravity);
    w.events.clear();
    w.impacts.clear();
    return w;
}

// Plays one random attempt. Returns the number of drops used to reach the goal, or -1.
int attempt(const PuzzleDef& d, const World& pot, Rng& rng) {
    World w = pot;
    int maxMerged = 0;
    for (size_t k = 0; k < d.queue.size(); ++k) {
        const int lv = d.queue[k];
        const float r = World::radiusFor(lv);
        float x;
        if (!w.drops.empty() && rng.range(0, 99) < 60) {
            const Drop& t = w.drops[size_t(rng.range(0, int(w.drops.size()) - 1))];
            x = t.x + rng.uni(-6.f, 6.f);
        } else {
            x = rng.uni(cfg::kFieldL + r, cfg::kFieldR - r);
        }
        x = std::min(std::max(x, cfg::kFieldL + r), cfg::kFieldR - r);
        w.spawn(x, cfg::kSpawnY, lv);
        for (int s = 0; s < kStepsPerDrop; ++s) {
            w.step(cfg::kStep, cfg::kGravity);
            for (const auto& e : w.events) maxMerged = std::max(maxMerged, e.level);
            w.events.clear();
            w.impacts.clear();
            w.updateOverflow(cfg::kDeadY, cfg::kStep);
        }
        if (maxMerged >= d.goal) return int(k) + 1;
        if (w.overflow() > cfg::kOverflowSeconds) return -1;
    }
    return -1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: liq_puzzlegen <out.txt> [levels] [attempts] [seed]\n");
        return 2;
    }
    const int levels = argc > 2 ? std::atoi(argv[2]) : 30;
    const int attempts = argc > 3 ? std::atoi(argv[3]) : 60;
    Rng rng;
    rng.seed(argc > 4 ? uint32_t(std::atoi(argv[4])) : 1u);
    std::FILE* f = std::fopen(argv[1], "w");
    if (!f) return 1;
    std::fprintf(f, "# Liq2048 puzzle pack 1, generated and verified by tools/puzzlegen (do not edit by hand)\n");
    std::fprintf(f, "# goal par | q queue levels | p level x y ...\n");
    int made = 0;
    const int first = argc > 5 ? std::atoi(argv[5]) : 0;
    for (int i = first; i < first + levels; ++i) {
        const int minPar = 2 + i / 10, maxPar = 3 + i / 5;
        for (int tries = 0; tries < 60; ++tries) {
            PuzzleDef d;
            const int potN = 2 + i / 4 + rng.range(0, 2);
            const int top = 1 + i / 7;                       // highest level in the pot
            d.goal = std::min(8, top + 2 + rng.range(0, 1));
            for (int p = 0; p < potN; ++p) {
                const int lv = rng.range(0, std::min(top, 3 + p % 2));
                const float r = World::radiusFor(lv);
                d.pot.push_back({lv, rng.uni(cfg::kFieldL + r, cfg::kFieldR - r), rng.uni(240.f, 420.f)});
            }
            const int qn = maxPar + 4;
            for (int q = 0; q < qn; ++q) d.queue.push_back(rng.range(0, 99) < 55 ? 0 : rng.range(0, 99) < 75 ? 1 : 2);
            const World pot = makePot(d);
            if (pot.highest() >= d.goal) continue;
            int best = -1, wins = 0;
            for (int a = 0; a < attempts; ++a) {
                if (a == 8 && wins == 0) break;  // unsolvable in practice: reject early
                const int r = attempt(d, pot, rng);
                if (r > 0) {
                    ++wins;
                    best = best < 0 ? r : std::min(best, r);
                }
            }
            const float maxRate = std::min(1.f, std::max(0.4f, 1.05f - 0.02f * float(i)));  // later levels must not be solved by luck
            if (best < minPar || best > maxPar || wins < 2 || float(wins) > maxRate * float(attempts)) continue;
            d.par = best;
            d.queue.resize(size_t(best + 5));
            std::fprintf(f, "%d %d | q", d.goal, d.par);
            for (int l : d.queue) std::fprintf(f, " %d", l);
            std::fprintf(f, " | p");
            for (const auto& p : d.pot) std::fprintf(f, " %d %.0f %.0f", p.level, double(p.x), double(p.y));
            std::fprintf(f, "\n");
            std::fflush(f);
            std::printf("level %2d: goal %d par %d (solved %d/%d)\n", i + 1, d.goal, d.par, wins, attempts);
            ++made;
            break;
        }
    }
    std::fclose(f);
    std::printf("%d levels\n", made);
    return made == levels ? 0 : 3;
}
