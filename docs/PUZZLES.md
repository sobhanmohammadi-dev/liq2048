# Puzzle pack

`assets/puzzles/pack1.txt` holds the puzzle levels, one per line (`#` starts a comment):

```
goal par | q <queue levels...> | p <level x y> <level x y> ...
```

- `goal`: droplet level to reach (3 = 8, 4 = 16, ...). `par`: drops of the best known solution (3 stars).
- `q`: the fixed queue of drops, as droplet levels, used in order. The run is lost when it is used up.
- `p`: droplets placed before the first drop (level, x, y in pot pixels); the game lets them fall and settle for 2.5
  seconds before play starts, so the starting pot is always physically valid.

The pack is **generated and verified**, not hand-typed: `tools/puzzlegen` creates random candidate pots and queues,
then a randomised solver plays each one with the real physics (random and merge-seeking drop positions). A candidate
is kept only if the solver can win it, and the fewest drops it needed becomes `par`; the target difficulty rises with
the level number (rarer solutions, higher par).

```bash
cmake -S . -B build -DLIQ_BUILD_TOOLS=ON && cmake --build build --target liq_puzzlegen
./build/liq_puzzlegen out.txt 11 60 3 22   # file, levels, attempts per candidate, seed, first level index
# (several processes can each generate a slice of the ladder; append the verified lines to pack1.txt.
#  Levels 16-26 of the pack were made this way; the very hardest slice takes too long to find, see the roadmap.)
```

Stars are saved per level. A level can be added by appending a line (hand-made levels are welcome; check them in the
game). The unit test `puzzle_pack_is_valid` checks the format and basic sanity of every level.
