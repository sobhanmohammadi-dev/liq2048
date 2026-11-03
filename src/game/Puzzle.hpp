#pragma once
// Puzzle pack: hand-checked levels with a pre-filled pot, a fixed queue of drops and a goal tile.
// Levels are generated and verified by tools/puzzlegen and stored in assets/puzzles/pack1.txt.
#include <string>
#include <vector>

namespace liq {

struct PuzzleDef {
    struct Piece {
        int level;
        float x, y;
    };
    int goal = 4;                 // reach this droplet level to win
    int par = 3;                  // drops needed by the best known solution
    std::vector<int> queue;       // levels of the upcoming drops (the run ends when they are used up)
    std::vector<Piece> pot;       // droplets placed before the first drop
};

// Loaded once from assets/puzzles/pack1.txt (empty if the file is missing).
const std::vector<PuzzleDef>& puzzlePack();
// Parses the text format (exposed for tests and the generator).
std::vector<PuzzleDef> parsePuzzles(const std::string& text);
// 3 stars: solved in <= par drops, 2: <= par + 2, 1: solved.
int puzzleStarsFor(const PuzzleDef& d, int dropsUsed);

}  // namespace liq
