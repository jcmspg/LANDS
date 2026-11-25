# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

LANDS is a fast, competitive 1v1 card game built around the five MTG basic land types (Plains, Island, Swamp, Mountain, Forest). Both players draw from one shared 50-card library and discard into one shared graveyard; every card is either **deployed** (board progress) or **pitched** (a spell-like trick), never both.

This is a learning project built in phases (see README.md "Development phases" and GDD §10). Only **Phase 1** — a standalone C++ rules engine with zero network/UI code — is underway; everything else (WebSocket server, Godot client, auth/matchmaking, Docker deploy) is planned but not started. Don't assume networking, persistence, or client code exists anywhere in this repo yet.

**`GameDesignDocument.md` (GDD, currently v2.1.0) is the single source of truth for rules, protocol, and architecture.** If README.md or code comments disagree with it, the GDD wins. Read the GDD sections relevant to whatever mechanic you're touching before changing engine behavior — it specifies exact edge cases (e.g. Plains anti-loop rule, Memory Lapse semantics, Forest ramp being vanilla) that are easy to get subtly wrong.

**Important gap to keep in mind:** the GDD describes the *finished* Phase 1 engine (full LIFO stack with reactive priority windows, timers, AFK strikes, action log replay, all five card abilities). The current code in `engine/` is an earlier slice of that: deploys, the main-phase pitches Ramp and Rummage, Island's Draw ETB, the priority window, and Island Counter (see `docs/ENGINE_STATUS.md` for the exact state). All five deploy ETBs and all five pitches are in (Destroy, Save, Reclaim, Extract, Surveil and Flicker via a pause-and-answer choice mechanism). Game end (win, concede, deploy lock, turn cap with tiebreak) is in too. The action log (`apply`, `action_log`, `replay`), `legal_actions`, and the timer-event effects (auto-deploy, random choice, AFK strikes) exist. A `RandomBot` and the `lands_sim` harness (`engine/sim/`, including the three `MatchConfig` compensation packages) exist; what remains of Phase 1 is smarter bots and a close-out review. When implementing a new mechanic, check what's actually in `engine/src` rather than assuming the GDD's full design is already live.

## Working agreement

Read these before changing code:

- `docs/CONVENTIONS.md` — code style, error-handling split, GoogleTest etiquette, commit rules. Match it exactly; don't restyle untouched code.
- `docs/ENGINE_STATUS.md` — what is built vs. the GDD, known deviations, open decisions, and the slice roadmap. Update it in the same commit that changes the status.

Process rules:

1. **Read first.** Before touching a mechanic, read its GDD section and the code in `engine/src` that it will touch. Check what actually exists; don't assume the GDD's finished design is live.
2. **One slice at a time.** Work one roadmap slice (see `ENGINE_STATUS.md`). Don't start the next slice, and don't refactor unrelated code, in the same change.
3. **Tests with the behavior.** Every new rule ships with GoogleTest cases: happy path, each rejection path, each applicable §3.5 edge case. A bug fix ships with a regression test that fails on the old code.
4. **Verify before reporting done.** Build and run the full suite (`ctest`) and say what was actually run. If the build tooling is unavailable, say so rather than claiming a pass.
5. **Separate commits.** Bug fixes, features, and doc changes are separate commits. Don't commit or push unless asked.
6. **Ask on design forks.** If the GDD is ambiguous or two reasonable API shapes exist (e.g. hand indices vs. instance IDs), surface the choice instead of picking silently, and record the outcome in `ENGINE_STATUS.md`.
7. **Update the docs** (`ENGINE_STATUS.md`, and this file if architecture changes) when reality changes. The GDD itself is only edited when the user decides a rule changes.

## Build & test

Requirements: C++20 compiler, CMake ≥ 3.20, GoogleTest discoverable via `find_package(GTest REQUIRED)`.

From the repo root:

```bash
cmake -S engine -B engine/build
cmake --build engine/build
ctest --test-dir engine/build --output-on-failure
./engine/build/lands_sim --matches 10000    # simulation harness (also runs under ctest as simulation_*)
```

Run a single test suite binary directly (built under `engine/build/`), e.g.:

```bash
./engine/build/test_LandGameMatch
```

Filter to a single test case with GoogleTest's own filter flag:

```bash
./engine/build/test_LandGameMatch --gtest_filter=LandGameMatch.SomeTestName
```

Each `.cpp` in `engine/tests/` (`test_ActionStack`, `test_Deck`, `test_Graveyard`, `test_LandGameMatch`, `test_LandRules`, `test_PlayerState`, `test_WinEvaluator`) compiles to its own executable and is registered with CTest via `gtest_discover_tests`. When adding a new module, add both a `src/*.cpp` to the `lands` static library and a matching `tests/test_*.cpp` target in `engine/CMakeLists.txt` (executable + `target_link_libraries` + `gtest_discover_tests`), following the existing pattern.

## Architecture

The engine lives entirely under `engine/`: public headers in `include/lands/`, implementations in `src/`, GoogleTest suites in `tests/`. Everything is in the `lands` namespace, including tests.

Core object graph, bottom-up:

- **`LandType`** / **`AbilityId`** / **`PlayMode`** — plain enums. `LandRules::get(LandType)` returns the static `LandDefinition` (deploy ability, pitch ability, pitch timing) for a type; this table is the one place card-to-ability wiring lives.
- **`Card`** — immutable land type + stable numeric identifier (the GDD's `instance_id`). Cards are value types passed/copied through hands, stack, graveyard, and **every zone operation addresses a card by that id, not by index**.
- **`Deck`** — the shared 50-card library. Seeded shuffle (`std::mt19937`-based) for deterministic replay per the GDD's determinism requirement; the shuffle also assigns random opaque ids so an id never leaks land type; `put_on_top` exists for the eventual Memory Lapse (countered-land) mechanic.
- **`PlayerState`** — one player's hand, battlefield cards (identity, so lands can be targeted), and per-land-type battlefield counts (`array<uint8_t, 5>`), indexed by `LandType`. `add_to_board` / `land_counts()` back the win evaluator directly (see GDD §6.3: counts, not just a presence bitmask, because Mono needs `>= 5`).
- **`Graveyard`** — the single shared discard pile; `take_card(card_id)` is the hook Forest's Reclaim will use.
- **`ActionStack`** — a bare LIFO `vector<StackItem>` (push/pop/top). It does not itself enforce the GDD's fixed stack grammar (one sorcery-speed bottom item + a chain of reactive pitches); `LandGameMatch` only ever pushes in that shape, and priority windows live there too.
- **`Action`** (`Action.hpp`) — one accepted intent (`PLAY_LAND`, `PITCH_LAND`, `CAST_COUNTER`, `CAST_SAVE`, `PASS_PRIORITY`, `CHOOSE_CARD`, `CHOOSE_SURVEIL`, `END_TURN`, `CONCEDE`). Timer events (`MAIN_PHASE_TIMEOUT`, `PRIORITY_TIMEOUT`, `CHOICE_TIMEOUT`, `DISCONNECT_FORFEIT`) are `Action`s too; their random picks come from a match-owned seeded RNG, so only the event is logged. Every accepted action method appends one to `LandGameMatch::action_log()`; `apply(Action)` is the generic entry point and `replay(seed, log)` reproduces a match. `legal_actions(player)` lists everything that player may do now.
- **`Choice`** (`Choice.hpp`) — `ChoiceType` / `PendingChoice`: a mid-resolution decision (Extract, Surveil, flickered ETBs) that pauses the match for one player. `LandGameMatch` exposes it via `choice_player()` / `choice_type()` / `choice_options()` and answers it with `choose_card` / `choose_surveil`; all other actions return `false` while it is pending.
- **`WinEvaluator`** — pure functions over `land_counts()`: `evaluate` returns `NONE` / `DOMAIN` / `MONO`; `tiebreak_winner` applies the GDD §3.6 scoring (distinct types, then total lands, else a draw). `GameEnd.hpp` holds `GameEndReason`.
- **`LandGameMatch`** — the top-level match object owning both `PlayerState`s, the shared `Deck`/`Graveyard`/`ActionStack`, turn/active-player bookkeeping, and the win check. Constructed with a seed; deals P1 five cards and P2 six (GDD's first-player compensation, §3.1). `play_land` takes the card out of the hand once, builds a `StackItem` from `LandRules::get()`, pushes it, and then `open_priority_window()` decides whether to wait for a response — after each push the opponent gets a priority window if they can respond (`cast_counter` / `pass_priority`), otherwise the stack auto-resolves. `pitch_land(player, card_id, extra_card_id)` follows the same push-and-resolve path for main-phase pitches (max 1 per turn; Ramp sets `ramp_available_`, which lets one extra deploy through, entering vanilla). Win is checked once per resolved deploy.

**Error-handling convention (stated in `LandRules.hpp`, applies engine-wide):** contract/programmer violations (bad indices, invalid enum values) throw `std::out_of_range` / `std::invalid_argument`. Illegal-but-expected game moves (e.g. playing out of turn, playing a card id that isn't in your hand) are surfaced by bool-returning methods (`play_land`, `pitch_land`, `end_turn`) returning `false` — never by throwing. Keep new engine methods consistent with this split.

When extending an ability (deploy or pitch), the intended flow is: extend `AbilityId` if needed, wire the land's ability in `LandRules.cpp`'s `kLandDefinitions` table, then implement the resolution behavior in `LandGameMatch::resolve_top_action` (or wherever stack resolution grows to). Card-specific rule text and every edge case (fizzle conditions, vanilla ETBs, targeting restrictions) is in GDD §3.5 — implement against that table, not intuition from Magic: The Gathering.
