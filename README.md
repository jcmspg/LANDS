# LANDS

**LANDS** is a fast, competitive 1v1 card game built around the five classic land types. Both players draw from a single shared library; every card is a permanent choice — **deploy** it toward a win, or **pitch** it as a spell-like trick. Matches aim for a 3–5 minute loop of bluffing, shared-zone memory, and stack timing.

This repository is a learning project: a bit-by-bit build of a rules engine, networking, and a real client — with an authoritative server and zero client-side game authority as the north star.

> **Rules source of truth:** [GameDesignDocument.md](GameDesignDocument.md) (v2.1.0). When this README and the GDD disagree, the GDD wins.

---

## Status

| Area | State |
| :--- | :--- |
| **Game design** | Finalized ruleset in the GDD (v2.1.0) |
| **Phase 1 — C++ rules engine** | **In progress** — foundations under `engine/` |
| **Phases 2–5** (server, Godot client, auth, deploy) | Planned — not started |

### What’s implemented now (`engine/`)

Early Phase 1 scaffolding with CMake + GoogleTest:

- **`LandType`** — Plains, Island, Swamp, Mountain, Forest
- **`Card`** — land type + stable instance id
- **`Deck`** — 50-card shared library (10 of each type), seeded shuffle, draw, put-on-top
- **`WinEvaluator`** — count-based **Domain** / **Mono** checks
- **`PlayerState`** — hand + per-type battlefield counts; draw and play land
- **`LandGameMatch`** — seeded shuffle, opening deal (P1: 5 / P2: 6), play land, win check after play

Not yet in the engine (still ahead in Phase 1): pitch/ETB effects, LIFO stack & priority, shared graveyard, turn structure/timers, action log / replay, bot stress harness.

---

## Core gameplay (high level)

Both players share one **50-card library** (10 of each land) and one **shared graveyard**. Player 1 opens with 5 cards; Player 2 with 6. On your turn you must **deploy** exactly one land if you can; you may also **pitch** at most one main-phase trick (Mountain / Swamp / Forest). Island and Plains pitches are **instant-speed** answers on the stack (counter / save).

**Win** (checked after every stack resolution, for both players):

1. **Domain** — at least one of each land type on your battlefield  
2. **Mono** — five lands of the same type  

Matches are bounded (mandatory deploys, deck-out play, hard turn cap). Full card text, stack grammar, timers, and edge cases live in the [GDD](GameDesignDocument.md).

---

## Tech stack

| Layer | Choice | Status |
| :--- | :--- | :--- |
| Rules engine | C++20, CMake, GoogleTest | Phase 1 (in progress) |
| Game server | Authoritative C++ headless process | Planned |
| Transport | JSON over WebSockets | Planned |
| Client | Godot 4 (desktop + WebAssembly) | Planned |
| Persistence | SQLite (accounts, ELO, rewards) | Planned |
| Deploy | Docker + reverse proxy (TLS) on Hetzner | Planned |

The long-term split is strict: **C++ owns all game state**; the Godot client renders snapshots and sends intents only.

---

## Repository layout

```
.
├── GameDesignDocument.md   # Full rules & architecture (source of truth)
├── README.md
└── engine/                 # Phase 1 C++ rules engine
    ├── CMakeLists.txt
    ├── include/lands/      # Public headers
    ├── src/                # Implementations
    └── tests/              # GoogleTest suites
```

Client, server gateway, and deploy configs will land in later phases.

---

## Build & test (C++ engine)

**Requirements:** a C++20 compiler, CMake ≥ 3.20, and [GoogleTest](https://google.github.io/googletest/) available to CMake (`find_package(GTest REQUIRED)`).

From the repository root:

```bash
cmake -S engine -B engine/build
cmake --build engine/build
ctest --test-dir engine/build --output-on-failure
```

Or from `engine/`:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Individual test binaries (e.g. `test_Deck`, `test_WinEvaluator`, `test_PlayerState`, `test_LandGameMatch`) are also produced under the build directory.

---

## Development phases

Each phase is meant to be tested and “frozen” before the next (see GDD §10 for detail).

1. **Bare-metal C++ engine** — deterministic `LandGameMatch`, stack/priority, win eval, seeded replay, bot stress tests  
2. **WebSocket & protocol** — JSON intents/snapshots, rooms, sessions, resync  
3. **Godot 4 client** — board UI, deploy/pitch interactions, Tweened state  
4. **Lobby, matchmaker & SQLite** — auth, queue, ELO / coin rewards  
5. **Docker & VPS** — containerized server + TLS proxy on Hetzner  

---

## Learning / contributing

This project grows in **small slices**: one module, a few tests, then the next rule. Prefer shipping a thin vertical (type → behavior → GoogleTest) over large unfinished layers.

If you’re collaborating later: match existing naming and layout under `engine/`, keep network/UI out of the rules library, and treat the GDD as normative for mechanics.

---

## Further reading

- [GameDesignDocument.md](GameDesignDocument.md) — ruleset, stack/priority, protocol sketch, and phase criteria  
