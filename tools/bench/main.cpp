// liq_bench: micro benchmarks of the hot paths. Prints one line per benchmark (median of several runs, in milliseconds).
//   liq_bench            run everything
//   liq_bench world      physics step only
//   liq_bench raster     liquid rasteriser only
// Run it from a Release build; numbers are only comparable on the same machine.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

#include "core/Themes.hpp"
#include "core/Util.hpp"
#include "game/Liquid.hpp"
#include "gfx/BlobRaster.hpp"

using namespace liq;
using Clock = std::chrono::steady_clock;

namespace {

template <class F>
double medianMs(int runs, int iterations, F&& f) {
    std::vector<double> t;
    for (int r = 0; r < runs; ++r) {
        const auto t0 = Clock::now();
        for (int i = 0; i < iterations; ++i) f();
        t.push_back(std::chrono::duration<double, std::milli>(Clock::now() - t0).count() / iterations);
    }
    std::sort(t.begin(), t.end());
    return t[t.size() / 2];
}

World filled(int particles, int substeps, int liquid) {
    World w;
    applyLiquid(w, liquid);
    w.substeps = substeps;
    Rng r;
    r.seed(3);
    while (int(w.parts.size()) < particles) {
        w.spawn(r.uni(40.f, 230.f), cfg::kFloor - 20.f - r.uni(0.f, 300.f), r.range(0, 4));
        for (int i = 0; i < 3; ++i) {
            w.step(cfg::kStep, cfg::kGravity);
            w.events.clear();
            w.impacts.clear();
        }
    }
    return w;
}

void benchWorld() {
    for (const int liquid : {int(kLiqWater), int(kLiqBubble)}) {
        for (const int substeps : {1, 2}) {
            World w = filled(1500, substeps, liquid);
            const double ms = medianMs(5, 60, [&] {
                w.step(cfg::kStep, cfg::kGravity);
                w.events.clear();
                w.impacts.clear();
            });
            std::printf("world  %-8s substeps %d  %5zu particles  %6.3f ms/step  (%.2f us per particle)\n", liquidProps(liquid).key + 7,
                        substeps, w.parts.size(), ms, ms * 1000.0 / double(w.parts.size()));
        }
    }
}

void benchRaster() {
    World w = filled(1500, 2, kLiqWater);
    std::vector<BlobSprite> pile;
    w.particleSprites(1.f, 0.f, pile);
    BlobRaster r(cfg::kW, cfg::kH);
    const Theme& t = themeAt(0);
    const double ms = medianMs(5, 60, [&] {
        r.clear();
        r.drawLiquid(pile, t, 9, 6, 1.1f);
    });
    std::printf("raster %5zu particles   %6.3f ms/frame (clear + drawLiquid)\n", pile.size(), ms);
    std::vector<BlobSprite> few(pile.begin(), pile.begin() + 40);
    const double ms2 = medianMs(5, 200, [&] {
        r.clear();
        r.drawLiquid(few, t, 9, 6, 1.1f);
    });
    std::printf("raster %5zu particles   %6.3f ms/frame (a few drops: shows the fixed cost per frame)\n", few.size(), ms2);
}

}  // namespace

int main(int argc, char** argv) {
    const bool all = argc < 2;
    if (all || !std::strcmp(argv[1], "world")) benchWorld();
    if (all || !std::strcmp(argv[1], "raster")) benchRaster();
    return 0;
}
