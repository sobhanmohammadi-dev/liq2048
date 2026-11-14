#pragma once
// The deal (v1.9.11): drops are not rolled one by one but dealt from bags. A bag holds six pairs of droplets (two of every
// level it contains), shuffled by the run's seed, so
//   * the whole sequence is fixed by the seed (same seed, same deal: daily/weekly runs and shared run codes are fair),
//   * every droplet has a partner somewhere in the same bag: a perfect player can merge all of them,
//   * the next three droplets are always known, so the pot can be planned, not just reacted to.
// How many levels the pairs are drawn from (the ramp) is decided by the caller through `draw`.
#include <vector>

#include "core/Util.hpp"

namespace liq {

class Deal {
public:
    static constexpr int kPairs = 6;       // pairs per bag
    static constexpr int kVisible = 3;     // droplets the player can see ahead (next + two more)

    void clear() { q_.clear(); }

    // The next droplet. `draw()` returns a level (0-based) for one pair; it is called once per pair of a new bag.
    template <class Draw>
    int take(Rng& rng, Draw draw) {
        refill(rng, draw);
        const int v = q_.front();
        q_.erase(q_.begin());
        refill(rng, draw);
        return v;
    }

    // Droplet i places after the one `take` returns next (0 = the one after it). Valid after a take.
    int ahead(size_t i) const { return i < q_.size() ? q_[i] : 0; }
    const std::vector<int>& queue() const { return q_; }

private:
    std::vector<int> q_;

    template <class Draw>
    void refill(Rng& rng, Draw draw) {
        while (q_.size() < size_t(kVisible + 1)) {
            const size_t from = q_.size();
            for (int p = 0; p < kPairs; ++p) {
                const int lv = draw();
                q_.push_back(lv);
                q_.push_back(lv);
            }
            for (size_t i = q_.size() - 1; i > from; --i) std::swap(q_[i], q_[from + size_t(rng.range(0, int(i - from)))]);  // Fisher-Yates
        }
    }
};

}  // namespace liq
