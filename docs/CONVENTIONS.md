# Engine conventions

Rules for anything under `engine/`. They were inferred from the existing code, so match the neighboring files when in doubt. Don't "improve" style in files you aren't otherwise changing.

## Layout

- Public headers: `engine/include/lands/<Name>.hpp`. Implementations: `engine/src/<Name>.cpp`. Tests: `engine/tests/test_<Name>.cpp`.
- Everything, including tests, lives in `namespace lands`. Close with `} // namespace lands`.
- A new module adds `src/<Name>.cpp` to the `lands` library and a `test_<Name>` executable in `engine/CMakeLists.txt`: `add_executable`, `target_link_libraries(... lands GTest::gtest GTest::gtest_main)`, and `gtest_discover_tests`. Follow the existing grouping in that file.

## Code style

- C++20, 4-space indent, no tabs.
- Header guards: `#ifndef LANDS_<UPPER_SNAKE>_HPP` / `#define` / `#endif // LANDS_<UPPER_SNAKE>_HPP`.
- Include order: project headers (`"lands/X.hpp"`) first, then a blank line, then standard headers (`<vector>`).
- Types `PascalCase` (`LandGameMatch`); methods and locals `snake_case` (`play_land`); data members end with `_` (`played_land_`); enumerators `UPPER_CASE` (`PitchTiming` is the one legacy exception and uses PascalCase; don't copy it).
- Classes: access specifiers indented one level (`    public:`), members indented two. Structs: plain members at one level.
- Plain value types are copied, not shared (`Card`, `StackItem`).
- **Cards are referenced by instance ID (`Card::get_identifier()`), never by hand/graveyard index** — in match actions, zone operations, and tests. Indices are fine only inside a single function while iterating.
- File-local helpers go in an anonymous namespace in the `.cpp` (see `require_player_index`).
- Short comments, only where the intent isn't obvious. Comment game rules with a GDD section number, e.g. `// GDD §3.5`.
- Use `static_cast`, never C-style casts.

## Error handling

Two kinds of failure, never mixed:

| Situation | Mechanism |
| :--- | :--- |
| Programmer/contract violation (bad index into a getter, invalid enum value) | throw `std::out_of_range` / `std::invalid_argument` (`std::runtime_error` for empty containers, as `ActionStack` does) |
| Illegal but expected game move (out of turn, wrong timing, already pitched this turn, bad hand index passed to an action) | return `false`, change **no** state |

Rules of thumb:
- Validate everything **before** mutating anything. A rejected action must leave the match exactly as it was.
- Action methods (`play_land`, `pitch_land`, `end_turn`) return `bool`. Query methods and getters throw on bad indices.

## Tests (GoogleTest)

- One `TEST(Suite, Name)` per behavior. `Suite` is the class under test (`LandGameMatch`); `Name` is PascalCase and says what is verified (`PlayLandMovesExactlyOneCardToBoard`), not how.
- `#include <gtest/gtest.h>`, then the header under test. Wrap tests in `namespace lands`.
- Use `EXPECT_*`; use `ASSERT_*` only when continuing would crash (e.g. an index you're about to dereference).
  - `EXPECT_TRUE/FALSE` for bool-returning actions, including the illegal-move cases.
  - `EXPECT_THROW(expr, std::out_of_range)` for contract violations.
  - `EXPECT_EQ` for state; check the **whole** state a change should touch (hand, board, graveyard, deck, stack), not just one counter.
- Every new rule gets: the happy path, each rejection path, and each GDD edge case from §3.5 that applies.
- A bug fix gets a regression test that **fails on the old code**. Confirm it by running it against the unfixed source before committing.
- Determinism: matches are built from a fixed seed (`LandGameMatch match(12345);`). To act on a card, read its id from `match.hand(player)`. When a test needs exact hands or a specific library (deck-out, deploy lock, Surveil peeks), build a stacked match with `LandGameMatch(Deck({...}))` (the last card is the top; P1 is dealt the top 5, P2 the next 6) instead of searching seeds. If a test needs a specific land type in hand, search the hand (or try other seeds) with a small helper in the test file rather than making the engine expose more internals. Don't use `rand()`, wall-clock time, or sleeps.
- Test small helpers go in an anonymous namespace at the top of the test file.
- Keep tests independent; no shared mutable state between `TEST`s.
- Cross-cutting invariants (legal actions vs. accepted actions, termination, replay determinism) live in `test_MatchProperties.cpp` and run whole random games; per-rule tests stay in `test_LandGameMatch.cpp`.

## Commits

- One logical change per commit; a bug fix is never bundled with a feature.
- Subject: one imperative sentence with no trailing period (e.g. `Fix play_land removing two cards and adding two lands per deploy`). Add a short body when the *why* isn't obvious.
- Tests ship in the same commit as the behavior they cover.
- Don't push, and don't commit, unless asked.
