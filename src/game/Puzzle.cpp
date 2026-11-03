#include "game/Puzzle.hpp"

#include <sstream>

#include "core/Platform.hpp"

namespace liq {

// Line format ('#' starts a comment):  goal par | q l l l ... | p level x y level x y ...
std::vector<PuzzleDef> parsePuzzles(const std::string& text) {
    std::vector<PuzzleDef> out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (const auto h = line.find('#'); h != std::string::npos) line.erase(h);
        const auto b1 = line.find('|');
        if (b1 == std::string::npos) continue;
        const auto b2 = line.find('|', b1 + 1);
        if (b2 == std::string::npos) continue;
        PuzzleDef d;
        std::istringstream head(line.substr(0, b1));
        if (!(head >> d.goal >> d.par)) continue;
        std::istringstream q(line.substr(b1 + 1, b2 - b1 - 1));
        std::string tag;
        q >> tag;  // "q"
        for (int l; q >> l;) d.queue.push_back(l);
        std::istringstream p(line.substr(b2 + 1));
        p >> tag;  // "p"
        PuzzleDef::Piece pc{};
        while (p >> pc.level >> pc.x >> pc.y) d.pot.push_back(pc);
        if (d.queue.empty() || d.goal < 1 || d.goal > 11) continue;
        out.push_back(d);
    }
    return out;
}

const std::vector<PuzzleDef>& puzzlePack() {
    static const std::vector<PuzzleDef> pack = [] {
        auto d = platform::readAsset("puzzles/pack1.txt");
        return parsePuzzles(std::string(d.begin(), d.end()));
    }();
    return pack;
}

int puzzleStarsFor(const PuzzleDef& d, int dropsUsed) { return dropsUsed <= d.par ? 3 : dropsUsed <= d.par + 2 ? 2 : 1; }

}  // namespace liq
