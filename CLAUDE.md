# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

LANDS is a fast, competitive 1v1 card game built around the five MTG basic land types (Plains, Island, Swamp, Mountain, Forest). Both players draw from one shared 50-card library and discard into one shared graveyard; every card is either **deployed** (board progress) or **pitched** (a spell-like trick), never both.

This is a learning project built in phases (see README.md "Development phases" and GDD §10). Only **Phase 1** — a standalone C++ rules engine with zero network/UI code — is underway; everything else (WebSocket server, Godot client, auth/matchmaking, Docker deploy) is planned but not started. Don't assume networking, persistence, or client code exists anywhere in this repo yet.

**`GameDesignDocument.md` (GDD, currently v2.1.0) is the single source of truth for rules, protocol, and architecture.** If README.md or code comments disagree with it, the GDD wins. Read the GDD sections relevant to whatever mechanic you're touching before changing engine behavior — it specifies exact edge cases (e.g. Plains anti-loop rule, Memory Lapse semantics, Forest ramp being vanilla) that are easy to get subtly wrong.

**Important gap to keep in mind:** the GDD describes the *finished* Phase 1 engine (full LIFO stack with reactive priority windows, timers, AFK strikes, action log replay, all five card abilities). The current code in `engine/` is an earlier slice of that: deploy-only lands with no pitch abilities wired up yet, and `LandGameMatch::resolve_top_action` pops and resolves the stack immediately after every push rather than opening a priority window. When implementing a new mechanic, check what's actually in `engine/src` rather than assuming the GDD's full design is already live.

## Build & test

Requirements: C++20 compiler, CMake ≥ 3.20, GoogleTest discoverable via `find_package(GTest REQUIRED)`.

From the repo root:

```bash
cmake -S engine -B engine/build
cmake --build engine/build
ctest --test-dir engine/build --output-on-failure
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
- **`Card`** — immutable land type + stable numeric identifier (the eventual `instance_id` from the GDD protocol). Cards are value types passed/copied through hands, stack, graveyard.
- **`Deck`** — the shared 50-card library. Seeded shuffle (`std::mt19937`-based) for deterministic replay per the GDD's determinism requirement; `put_on_top` exists for the eventual Memory Lapse (countered-land) mechanic.
- **`PlayerState`** — one player's hand plus per-land-type battlefield counts (`array<uint8_t, 5>`), indexed by `LandType`. `add_to_board` / `land_counts()` back the win evaluator directly (see GDD §6.3: counts, not just a presence bitmask, because Mono needs `>= 5`).
- **`Graveyard`** — the single shared discard pile; `take_card(index)` is the hook Forest's Reclaim will use.
- **`ActionStack`** — a bare LIFO `vector<StackItem>` (push/pop/top). It does not yet enforce the GDD's fixed stack grammar (one sorcery-speed bottom item + a chain of reactive pitches) or open priority windows — that logic still needs to be layered on in `LandGameMatch`.
- **`WinEvaluator`** — pure function over a player's `land_counts()` returning `NONE` / `DOMAIN` / `MONO`.
- **`LandGameMatch`** — the top-level match object owning both `PlayerState`s, the shared `Deck`/`Graveyard`/`ActionStack`, turn/active-player bookkeeping, and the win check. Constructed with a seed; deals P1 five cards and P2 six (GDD's first-player compensation, §3.1). `play_land` builds a `StackItem` from `LandRules::get()`, pushes it, and immediately calls `resolve_top_action()` — there is no opponent response step yet. Win is checked once per resolved deploy.

**Error-handling convention (stated in `LandRules.hpp`, applies engine-wide):** contract/programmer violations (bad indices, invalid enum values) throw `std::out_of_range` / `std::invalid_argument`. Illegal-but-expected game moves (e.g. playing out of turn, playing when the hand doesn't have that index) are surfaced by bool-returning methods (`play_land`, `end_turn`) returning `false` — never by throwing. Keep new engine methods consistent with this split.

When extending an ability (deploy or pitch), the intended flow is: extend `AbilityId` if needed, wire the land's ability in `LandRules.cpp`'s `kLandDefinitions` table, then implement the resolution behavior in `LandGameMatch::resolve_top_action` (or wherever stack resolution grows to). Card-specific rule text and every edge case (fizzle conditions, vanilla ETBs, targeting restrictions) is in GDD §3.5 — implement against that table, not intuition from Magic: The Gathering.
