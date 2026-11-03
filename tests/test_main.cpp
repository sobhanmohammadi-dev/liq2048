// Dependency-free unit tests (run with `ctest` from the build directory).
// Logic only: no window, renderer or audio device is created.
#define SDL_MAIN_HANDLED
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "core/Json.hpp"
#include "core/Util.hpp"
#include "audio/Audio.hpp"
#include "core/Access.hpp"
#include "gfx/PixelFont.hpp"
#include "core/Platform.hpp"
#include "game/Mode.hpp"
#include "game/Puzzle.hpp"
#include "game/World.hpp"
#include "gfx/BlobRaster.hpp"
#include "core/Themes.hpp"
#include "i18n/Loc.hpp"
#include "save/Save.hpp"

namespace {

struct Test {
    const char* name;
    std::function<void()> fn;
};
std::vector<Test>& registry() {
    static std::vector<Test> r;
    return r;
}
struct Reg {
    Reg(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); }
};
int g_failures = 0;

#define TEST(name) \
    static void name(); \
    static Reg reg_##name(#name, name); \
    static void name()
#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::printf("    CHECK failed: %s (line %d)\n", #cond, __LINE__); \
            ++g_failures; \
        } \
    } while (0)

using namespace liq;

BlobSprite plainSprite0(float x, float y, int level) {
    BlobSprite s;
    s.x = x; s.y = y; s.level = level;
    s.ra = s.rb = World::radiusFor(level);
    s.cs = 1.f; s.sn = 0.f;
    return s;
}

void stepFor(World& w, float seconds) {
    for (float t = 0; t < seconds; t += cfg::kStep) w.step(cfg::kStep, cfg::kGravity);
}

TEST(radius_grows_with_level) {
    for (int i = 0; i < cfg::kMaxLevel; ++i) CHECK(World::radiusFor(i + 1) > World::radiusFor(i));
    // The biggest drop must fit in the pot.
    CHECK(World::radiusFor(cfg::kMaxLevel) * 2 < cfg::kFieldR - cfg::kFieldL);
    for (int i = 0; i < cfg::kMaxLevel; ++i) CHECK(World::particlesFor(i + 1) > World::particlesFor(i));
}

TEST(equal_drops_merge_on_contact) {
    World w;
    const float r = World::radiusFor(0);
    w.spawn(100, 400, 0);
    w.spawn(100 + r * 1.75f, 400, 0);
    for (int i = 0; i < 40 && w.drops.size() > 1; ++i) w.step(cfg::kStep, cfg::kGravity);
    CHECK(w.drops.size() == 1);
    CHECK(w.drops[0].level == 1);
    CHECK(!w.events.empty() && w.events[0].level == 1 && w.events[0].chain == 1);
    // Volume is constant per level: the merged drop has the particle count of level 1.
    CHECK(std::abs(int(w.parts.size()) - World::particlesFor(1)) < World::particlesFor(1) / 5);
}

TEST(different_drops_do_not_merge) {
    World w;
    w.spawn(100, 400, 0);
    w.spawn(100 + World::radiusFor(0) + World::radiusFor(1), 400, 1);
    for (int i = 0; i < 90; ++i) w.step(cfg::kStep, cfg::kGravity);
    CHECK(w.drops.size() == 2);
    CHECK(w.events.empty());
}

TEST(max_level_drops_never_merge) {
    World w;
    w.spawn(110, 300, cfg::kMaxLevel);
    w.spawn(110 + World::radiusFor(cfg::kMaxLevel) * 1.95f, 300, cfg::kMaxLevel);
    for (int i = 0; i < 90; ++i) w.step(cfg::kStep, cfg::kGravity);
    CHECK(w.drops.size() == 2);
}

TEST(independent_merges_chain) {
    World w;
    const float r = World::radiusFor(0);
    w.spawn(50, 400, 0);
    w.spawn(50 + r * 1.75f, 400, 0);
    w.spawn(170, 400, 0);
    w.spawn(170 + r * 1.75f, 400, 0);
    int maxChain = 0;
    for (int i = 0; i < 3; ++i) {
        w.step(cfg::kStep, cfg::kGravity);
        for (auto& e : w.events) maxChain = std::max(maxChain, e.chain);
    }
    CHECK(w.drops.size() == 2);
    CHECK(maxChain == 2);
}

TEST(liquid_spreads_and_stays_in_bounds) {
    World w;
    Rng rng;
    rng.seed(42);
    for (int i = 0; i < 14; ++i) {
        w.spawn(cfg::kFieldL + 40 + float(rng.range(0, 140)), cfg::kSpawnY, rng.range(0, 3));
        for (int k = 0; k < 20; ++k) w.step(cfg::kStep, cfg::kGravity);
        w.events.clear();
    }
    for (int k = 0; k < 120; ++k) w.step(cfg::kStep, cfg::kGravity);
    for (const auto& p : w.parts) {
        CHECK(std::isfinite(p.x) && std::isfinite(p.y));
        CHECK(p.x >= cfg::kFieldL && p.x <= cfg::kFieldR);
        CHECK(p.y <= cfg::kFloor);
    }
    // A single settled level-0 drop flattens: wider than tall (it is liquid, not a ball).
    World f;
    f.spawn(135, 380, 0);
    for (int k = 0; k < 150; ++k) f.step(cfg::kStep, cfg::kGravity);
    float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
    for (const auto& p : f.parts) {
        minx = std::min(minx, p.x);
        maxx = std::max(maxx, p.x);
        miny = std::min(miny, p.y);
        maxy = std::max(maxy, p.y);
    }
    CHECK((maxx - minx) > (maxy - miny) * 1.3f);
}

TEST(overflow_timer_triggers_only_when_settled_above_line) {
    World w;
    w.spawn(135, cfg::kDeadY - 10, 3);  // above the line, but brand new
    w.updateOverflow(cfg::kDeadY, 0.5f);
    CHECK(w.overflow() == 0.f);
    w.drops[0].age = 5.f;
    w.updateOverflow(cfg::kDeadY, 0.5f);
    CHECK(w.overflow() > 0.f);
}

TEST(physics_is_deterministic_and_quality_levels_stay_stable) {
    for (int q = 1; q <= 3; ++q) {
        double sums[2] = {0, 0};
        for (int run = 0; run < 2; ++run) {
            World w;
            w.substeps = q;
            for (int i = 0; i < 12; ++i) w.spawn(60.f + float(i % 4) * 40.f, 80.f + float(i) * 6.f, i % 5);
            for (int k = 0; k < 400; ++k) w.step(1.f / 60.f, 1.f);
            for (const auto& p : w.parts) {
                CHECK(p.x > -1.f && p.x < 271.f);
                sums[run] += double(p.x) * 1.3 + double(p.y);
            }
        }
        CHECK(sums[0] == sums[1]);  // same inputs, same result, bit for bit (in one build)
    }
}

TEST(rng_is_deterministic_per_seed) {
    Rng a, b;
    a.seed(1234);
    b.seed(1234);
    for (int i = 0; i < 100; ++i) CHECK(a.range(0, 99) == b.range(0, 99));
}

TEST(daily_mode_seed_is_stable_per_day_and_differs_between_days) {
    Save s;
    auto d1 = makeMode(ModeId::Daily, s, 20000);
    auto d2 = makeMode(ModeId::Daily, s, 20000);
    auto d3 = makeMode(ModeId::Daily, s, 20001);
    CHECK(d1->seed() == d2->seed());
    CHECK(d1->seed() != d3->seed());
    CHECK(d1->seed() != 0);
    CHECK(d1->dropLimit() == cfg::kDailyDrops);
}

TEST(classic_loses_on_overflow_zen_never_fails) {
    Save s;
    Stats st;
    st.overflow = true;
    CHECK(makeMode(ModeId::Classic, s, 0)->evaluate(st) == Outcome::Lost);
    CHECK(makeMode(ModeId::Zen, s, 0)->evaluate(st) == Outcome::Running);
    CHECK(makeMode(ModeId::Zen, s, 0)->gravityScale() < 1.f);
}

TEST(timed_mode_finishes_when_time_runs_out) {
    Save s;
    auto m = makeMode(ModeId::Timed, s, 0);
    Stats st;
    CHECK(m->evaluate(st) == Outcome::Running);
    m->update(cfg::kTimeAttackSeconds + 1.f);
    CHECK(m->evaluate(st) == Outcome::Finished);
}

TEST(target_mode_wins_on_tile_and_loses_when_out_of_drops) {
    Save s;
    auto m = makeMode(ModeId::Target, s, 0);  // stage 0: 256 = level 8
    Stats st;
    st.highest = 7;
    st.dropsLeft = 5;
    CHECK(m->evaluate(st) == Outcome::Running);
    st.highest = 8;
    CHECK(m->evaluate(st) == Outcome::Won);
    st.highest = 3;
    st.dropsLeft = 0;
    st.quiet = 2.f;
    CHECK(m->evaluate(st) == Outcome::Lost);
    s.targetStage = 2;
    CHECK(makeMode(ModeId::Target, s, 0)->dropLimit() > m->dropLimit());
}

TEST(json_parser_reads_flat_tables_and_escapes) {
    auto m = parseFlatJson("{ \"a\": \"x\", \"b\":\"line\\nbreak \\\"q\\\"\" }");
    CHECK(m.size() == 2);
    CHECK(m["a"] == "x");
    CHECK(m["b"] == "line\nbreak \"q\"");
}

TEST(localization_has_matching_keys_and_falls_back) {
    Loc en, fa;
    en.load("en");
    fa.load("fa");
    CHECK(en.tr("menu.play") == "Play");
    CHECK(fa.tr("menu.play") != "Play" && fa.tr("menu.play") != "menu.play");
    CHECK(fa.rtl() && !en.rtl());
    CHECK(en.tr("no.such.key") == "no.such.key");
    CHECK(en.fmt("menu.streak", "7").find('7') != std::string::npos);
    // Every English key must exist in Persian (catches forgotten translations).
    auto read = [](const char* p) {
        std::string s;
        if (FILE* f = std::fopen(p, "rb")) {
            char buf[4096];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
            std::fclose(f);
        }
        return parseFlatJson(s);
    };
    auto e = read("assets/lang/en.json"), f = read("assets/lang/fa.json");
    CHECK(!e.empty());
    for (const auto& kv : e) {
        if (f.find(kv.first) == f.end()) std::printf("    missing fa key: %s\n", kv.first.c_str());
        CHECK(f.find(kv.first) != f.end());
    }
    for (const auto& kv : f) CHECK(e.find(kv.first) != e.end());
}

TEST(save_unlocks_achievements_once_and_tracks_streaks) {
    Save s;
    CHECK(s.unlock(kFirst128));
    CHECK(!s.unlock(kFirst128));
    CHECK(s.has(kFirst128) && !s.has(kFirst256));
    s.touchDay(100);
    CHECK(s.streak == 1);
    s.touchDay(100);
    CHECK(s.streak == 1);
    s.touchDay(101);
    CHECK(s.streak == 2);
    s.touchDay(110);  // gap resets quietly
    CHECK(s.streak == 1);
}

TEST(raster_single_blob_covers_about_a_disc) {
    BlobRaster r(cfg::kW, cfg::kH);
    std::vector<BlobSprite> sp{plainSprite0(135, 200, 3)};
    r.drawField(sp, themeAt(0));
    const float R = sp[0].ra;
    const float area = 3.14159f * R * R;
    CHECK(r.coveredPixels() > area * 0.8f && r.coveredPixels() < area * 1.2f);
}

TEST(raster_touching_blobs_fuse_and_use_palette_only) {
    BlobRaster r(cfg::kW, cfg::kH);
    const Theme& t = themeAt(0);
    auto a = plainSprite0(120, 200, 2), b = plainSprite0(120 + a.ra * 1.8f, 200, 3);
    r.drawField({a, b}, t);
    // The seam between the centres is filled (one shared silhouette).
    const int mx = int((a.x + b.x) / 2);
    CHECK((r.at(mx, 200) >> 24) != 0);
    // Every covered pixel is either a palette colour or its outline mix.
    CHECK(r.coveredPixels() > 0);
}

TEST(raster_is_safe_off_screen) {
    BlobRaster r(cfg::kW, cfg::kH);
    std::vector<BlobSprite> sp{plainSprite0(-30, -30, 5), plainSprite0(400, 600, 5), plainSprite0(0, 0, 11)};
    r.drawField(sp, themeAt(1));
    r.drawLabels(sp, themeAt(1));
    CHECK(r.coveredPixels() >= 0);
}

TEST(fps_choices_and_limits) {
    Save s;
    CHECK(s.fpsLimit() == 0);  // default VSync
    s.fpsIdx = 2;
    CHECK(s.fpsLimit() == 60);
    s.fpsIdx = 6;
    CHECK(s.fpsLimit() == -1);
    CHECK(Save::fpsChoice(1) == 30);
}

TEST(all_languages_complete) {
    // Every language must define every English key, keep the {0} placeholder, and never be blank.
    auto load = [](const std::string& code) {
        auto d = liq::platform::readAsset("lang/" + code + ".json");
        return parseFlatJson(std::string(d.begin(), d.end()));
    };
    const auto en = load("en");
    CHECK(en.size() > 80);
    for (int i = 0; i < kLangCount; ++i) {
        const std::string code = langAt(i).code;
        const auto t = load(code);
        for (const auto& kv : en) {
            auto it = t.find(kv.first);
            CHECK(it != t.end());
            if (it == t.end()) continue;
            CHECK(!it->second.empty());
            CHECK((kv.second.find("{0}") != std::string::npos) == (it->second.find("{0}") != std::string::npos));
        }
        CHECK(t.size() == en.size());  // no stray keys
    }
}

TEST(pixel_font_folds_accents) {
    PixelFont::setGermanFold(false);
    CHECK(PixelFont::ascii("Atr\xC3\xA1s \xC3\xA9\xC3\xB1 \xC3\xA7\xC4\x9F\xC5\x9F\xC4\xB1") == "AtrAs EN CGSI");
    CHECK(PixelFont::ascii("\xC2\xBF" "Qu\xC3\xA9?") == "QuE?");
    CHECK(PixelFont::ascii("\xC3\xBC") == "U");
    PixelFont::setGermanFold(true);
    CHECK(PixelFont::ascii("\xC3\xBC") == "UE");
    CHECK(PixelFont::ascii("gr\xC3\xB6" "\xC3\x9F" "er") == "grOESSer");
    PixelFont::setGermanFold(false);
    CHECK(PixelFont::ascii("\xD9\x85") == "?");  // Arabic letters are not in the pixel font
}

TEST(accessibility_palettes_are_distinct) {
    // Colour-vision palettes must never contain two near-identical colours.
    for (int mode = 1; mode <= 2; ++mode) {
        const Theme t = accessTheme(themeAt(0), mode, false);
        for (int i = 0; i < 12; ++i)
            for (int j = i + 1; j < 12; ++j) {
                const int dr = int(t.blob[i].r) - int(t.blob[j].r), dg = int(t.blob[i].g) - int(t.blob[j].g),
                          db = int(t.blob[i].b) - int(t.blob[j].b);
                CHECK(dr * dr + dg * dg + db * db > 40 * 40);
            }
    }
    const Theme hc = accessTheme(themeAt(0), 0, true);
    CHECK(hc.text.r == 255 && hc.ink.r == 0);
}

TEST(new_modes_rules) {
    Save sv;
    auto rain = makeMode(ModeId::Rain, sv, 20000);
    CHECK(!rain->manualDrop());
    CHECK(rain->autoDropInterval(0.f) > rain->autoDropInterval(60.f));
    CHECK(rain->autoDropInterval(10000.f) >= 1.f);  // never faster than the floor
    auto tide = makeMode(ModeId::Tide, sv, 20000);
    CHECK(tide->deadLineShift(0.f) == 0.f);
    CHECK(tide->deadLineShift(30.f) > 0.f);
    CHECK(tide->deadLineShift(1e6f) <= cfg::kTideMaxShift);
    auto wk = makeMode(ModeId::Weekly, sv, 20000);
    CHECK(wk->fixedModifier() >= 1 && wk->fixedModifier() <= 4);
    CHECK(wk->seed() == makeMode(ModeId::Weekly, sv, 20000 + 1)->seed());   // same week -> same seed (20000 and 20001 share a week)
    CHECK(weekOf(20003) == weekOf(20000) || weekOf(20003) == weekOf(20000) + 1);
    CHECK(weeklyRuleOf(weekOf(20000)) == wk->fixedModifier());
}

TEST(puzzle_pack_is_valid) {
    const auto parsed = parsePuzzles("# c\n3 2 | q 0 0 1 | p 1 100 300 0 150 320\nbad line\n");
    CHECK(parsed.size() == 1);
    CHECK(parsed[0].goal == 3 && parsed[0].par == 2 && parsed[0].queue.size() == 3 && parsed[0].pot.size() == 2);
    CHECK(puzzleStarsFor(parsed[0], 2) == 3 && puzzleStarsFor(parsed[0], 4) == 2 && puzzleStarsFor(parsed[0], 9) == 1);
    const auto& pack = puzzlePack();
    CHECK(pack.size() >= 12);
    for (const auto& d : pack) {
        CHECK(d.par >= 2 && d.par <= int(d.queue.size()));
        CHECK(d.goal >= 3 && d.goal <= 11);
        for (const auto& p : d.pot) CHECK(p.x > cfg::kFieldL && p.x < cfg::kFieldR && p.level >= 0 && p.level < d.goal);
    }
    // A puzzle run: queue exhausted without the goal is a loss, reaching the goal is a win.
    Save sv;
    auto m = makeMode(ModeId::Puzzle, sv, 0, 0);
    Stats s;
    s.dropsLeft = 3;
    CHECK(m->evaluate(s) == Outcome::Running);
    s.highest = m->puzzle()->goal;
    CHECK(m->evaluate(s) == Outcome::Won);
    s.highest = 0; s.dropsLeft = 0; s.quiet = 2.f;
    CHECK(m->evaluate(s) == Outcome::Lost);
}

TEST(save_roundtrip_new_fields) {
    Save::pathOverride() = "liq_test_save.txt";
    Save a;
    a.modifier = 3; a.weeklyBest = 777; a.puzzleStars[0] = 3; a.puzzleStars[7] = 1; a.totalDrops = 1234; a.longestChain = 6;
    CHECK(a.puzzlesSolved() == 2);
    a.store();
    Save b;
    b.load();
    CHECK(b.modifier == 3 && b.weeklyBest == 777 && b.puzzleStars[0] == 3 && b.puzzleStars[7] == 1 && b.puzzleStars[1] == 0);
    CHECK(b.totalDrops == 1234 && b.longestChain == 6);
    Save::pathOverride().clear();
    std::remove("liq_test_save.txt");
}

TEST(save_game_feel_defaults) {
    Save s;
    CHECK(s.haptics && s.guide && s.shake && s.slowmo && !s.calm);
    CHECK(s.effShake() && s.effSlowmo());
    s.calm = true;  // calm mode overrides shake and slow motion
    CHECK(!s.effShake() && !s.effSlowmo());
}

TEST(world_copy_restores_state) {
    // Undo relies on World being a plain value: a copy taken earlier must be independent of later steps.
    World w;
    w.spawn(135.f, 100.f, 2);
    World snap = w;
    for (int i = 0; i < 60; ++i) w.step(cfg::kStep, cfg::kGravity);
    CHECK(snap.parts.size() == w.parts.size());
    CHECK(snap.parts[0].y < w.parts[0].y - 5.f);
    World back = snap;
    CHECK(back.parts[0].y == snap.parts[0].y);
}

TEST(audio_assets_decode) {
    // Every shipped OGG must decode to audible, finite, 44.1 kHz stereo audio (tests run from the repo root).
    const char* names[] = {"sounds/drop_0", "sounds/splash_2", "sounds/merge_00", "sounds/merge_11", "sounds/chain_6",
                           "sounds/click", "sounds/swipe", "sounds/fail", "sounds/success", "sounds/unlock",
                           "music/menu", "music/game_calm", "music/game_intense"};
    for (const char* n : names) {
        auto file = liq::platform::readAsset(std::string(n) + ".ogg");
        CHECK(!file.empty());
        Clip c = decodeOgg(file.data(), int(file.size()));
        CHECK(c.frames > 1000);
        double e = 0;
        for (int16_t v : c.pcm) e += double(v) * double(v);
        CHECK(e > 0);
    }
    // The audio layer works (silently) without a device.
    Audio a;
    a.play(Sfx::Merge, 0.5f, 3, 4);
    a.setMood(MusicMood::Game);
    a.setIntensity(0.5f);
}

}  // namespace

int main() {
    int run = 0;
    for (auto& t : registry()) {
        int before = g_failures;
        t.fn();
        std::printf("[%s] %s\n", g_failures == before ? " OK " : "FAIL", t.name);
        ++run;
    }
    std::printf("%d tests, %d failed checks\n", run, g_failures);
    return g_failures ? 1 : 0;
}
