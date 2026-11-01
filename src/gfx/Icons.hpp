#pragma once
// Tiny pixel icons drawn from string art (see Gfx::icon).
namespace liq {

enum class IconId { Pause, Play, Left, Right, Check, Cross, Crown, Lock, Marker, Count };

struct IconDef {
    int w, h;
    const char* rows[10];
};

inline const IconDef& iconDef(IconId id) {
    static const IconDef defs[int(IconId::Count)] = {
        {7, 9, {"##...##", "##...##", "##...##", "##...##", "##...##", "##...##", "##...##", "##...##", "##...##", nullptr}},
        {6, 9, {"#.....", "##....", "###...", "####..", "#####.", "####..", "###...", "##....", "#.....", nullptr}},
        {4, 7, {"...#", "..#.", ".#..", "#...", ".#..", "..#.", "...#", nullptr, nullptr, nullptr}},
        {4, 7, {"#...", ".#..", "..#.", "...#", "..#.", ".#..", "#...", nullptr, nullptr, nullptr}},
        {7, 6, {"......#", ".....##", "#...##.", "##.##..", ".###...", "..#....", nullptr, nullptr, nullptr, nullptr}},
        {7, 7, {"#.....#", ".#...#.", "..#.#..", "...#...", "..#.#..", ".#...#.", "#.....#", nullptr, nullptr, nullptr}},
        {9, 6, {"#...#...#", "##..#..##", "###.#.###", "#########", "#########", ".#######.", nullptr, nullptr, nullptr, nullptr}},
        {7, 8, {"..###..", ".#...#.", ".#...#.", "#######", "#######", "###.###", "#######", "#######", nullptr, nullptr}},
        {5, 3, {"#####", ".###.", "..#..", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}},
    };
    return defs[int(id)];
}

}  // namespace liq
