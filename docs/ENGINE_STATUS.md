# Engine status & roadmap

Living tracker of how far `engine/` is from what `GameDesignDocument.md` describes. The GDD is the source of truth for rules; this file records only **what is built**, **what is deliberately different**, **what was decided**, and **what comes next**. Update it in the same commit that changes the status.

**Summary:** the Phase 1 ruleset is complete and verified (180 tests, clean under ASan + UBSan, 30,000 simulated matches replay bit-for-bit). The independent close-out audit found no rule discrepancy that changes a game outcome (see Audit below). Open items: smarter bots, and the next step, a Godot demo (see Roadmap).

## Implemented

### Setup and determinism

| Area                                                                                                                                  | State | GDD        |
| :------------------------------------------------------------------------------------------------------------------------------------ | :---- | :--------- |
| Shared 50-card deck, seeded shuffle, random opaque instance ids (an id never reveals land type or library position)                   | done  | §3.1, §6.3 |
| Deal 5 / 6; P1 skips only its turn-1 draw, P2 draws normally on its first turn; an empty library skips draw steps instead of throwing | done  | §3.1, §3.2 |
| `MatchConfig` first-player compensation packages: `draw_skip_only`, `sixth_card_only`, `both` (the default)                           | done  | §3.1, §10  |
| Stacked-deck constructors (`Deck(vector<Card>)`, `LandGameMatch(Deck)`) for tests, replays and experiments                            | done  | §10        |
| `Action` type, `apply`, append-only `action_log()`, `replay(seed, log)`; rejected actions are never logged                            | done  | §6.3       |
| `state_digest()`: one hash of the full observable state, so replay equality is a single compare                                       | done  | §6.3       |
| Match-owned seeded RNG for timer-event picks, so they replay exactly                                                                  | done  | §6.3       |

### Rules

| Area                                                                                                                                                                                                                                         | State | GDD        |
| :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :---- | :--------- |
| Turn bookkeeping: one deploy per turn, one main-phase pitch per turn, ramp budget                                                                                                                                                            | done  | §3.2       |
| Deploys and pitches go through the stack; the pusher's opponent gets a priority window when they can respond (`priority_player`, `pass_priority`); auto-pass otherwise                                                                       | done  | §4.2, §4.3 |
| Island Counter (`cast_counter`): counters any stack item, counter wars resolve LIFO, a countered deploy goes on top of the library (Memory Lapse), a countered pitch fizzles                                                                 | done  | §3.5, §4.2 |
| Plains Save (`cast_save`): the window opens only if the holder has a Plains and a stack item targets one of their lands; can itself be countered                                                                                             | done  | §3.5, §4.3 |
| Deploy ETBs: Island **Draw** (no-op on an empty library), Mountain **Destroy**, Forest **Reclaim**, Swamp **Extract**, Plains **Flicker**                                                                                                    | done  | §3.5       |
| Targets declared up front at push time (Destroy: an opponent land; Reclaim: a graveyard card; Flicker: another land you control), required exactly when a valid target exists                                                                | done  | §8.4       |
| Pitches: Forest **Ramp** (one extra deploy, enters vanilla), Mountain **Rummage**, Swamp **Surveil 2**, Island Counter, Plains Save                                                                                                          | done  | §3.4, §3.5 |
| Choice prompts (`choice_player` / `choice_type` / `choice_options`, answered with `choose_card` / `choose_surveil`): resolution pauses and every other action is rejected; used by Extract, Surveil and the fresh choices of a flickered ETB | done  | §8.6       |
| Flicker re-triggers the land's ETB inline with fresh choices, opens no priority window (cannot be countered), fizzles when it flickers a Plains (anti-loop) or when its target was saved                                                     | done  | §3.5       |
| Battlefield tracks cards (identity), not only per-type counts                                                                                                                                                                                | done  | §6.3       |
| Win check (Domain / Mono) after each resolution                                                                                                                                                                                              | done  | §3.3       |

### Match flow and timers

| Area                                                                                                                                                                                                                                                                                                                                                                                    | State                                                                                            | GDD               |
| :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------- | :---------------- |
| Game end: `game_over`, `winner_player`, `end_reason`, `is_draw`; Domain / Mono, `concede`, deploy lock (empty hand when the deploy is due: at turn start after the draw, or at `end_turn` with no deploy made), turn cap (end of round 25 = turn 50), tiebreak scoring (`WinEvaluator::tiebreak_winner`); every action is rejected afterwards and pending windows / choices are cleared | done                                                                                             | §3.3, §3.6, §8.10 |
| Timer events (`main_phase_timeout`, `priority_timeout`, `choice_timeout`): auto-deploy a uniformly random legal card and target, auto-pass, uniformly random legal choice; only the event is logged                                                                                                                                                                                     | done                                                                                             | §3.2, §4.4, §8.6  |
| AFK strikes (`afk_strikes`, 3 = `AFK_FORFEIT`) and `disconnect_forfeit`                                                                                                                                                                                                                                                                                                                 | done                                                                                             | §3.6              |
| The clocks themselves (60s / 10s / 15s, disconnect grace)                                                                                                                                                                                                                                                                                                                               | not an engine concern: the server's event loop owns them (§6.3) and calls the timer events above | §4.4              |

### Tooling and verification

| Area                                                                                                                                                                                                                                                                                   | State | GDD |
| :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :---- | :-- |
| `legal_actions(player)`: exactly the set of actions the engine accepts (concede aside), checked against a brute-force oracle over random games; `deploy_due()`                                                                                                                         | done  | §10 |
| `RandomBot` (uniform legal play, with timer and stalling profiles) and `lands_sim` (`--matches`, `--seed`, `--timer-percent`, `--idle-percent`, `--config`): checks termination and bit-for-bit replay, reports outcomes and action coverage, compares the three compensation packages | done  | §10 |
| Property tests (`test_MatchProperties`): legal actions vs. accepted actions, termination, replay determinism with timer events, and card conservation (all 50 cards in exactly one zone) at every step of every random game                                                                                                                                                         | done  | §10 |
| Opt-in sanitizer build (`-DLANDS_SANITIZE=ON`); the full suite and ~14,500 simulated matches run clean under ASan + UBSan                                                                                                                                                              | done  | §10 |

## Close-out audit (independent read of the GDD against the engine)

A fresh reader walked GDD §3, §4, §6.3 and §8 against the code and ran a 20,000-game fuzz (default and sixth-card-only configs, ~3% timer events) checking: one player to act at a time, every listed action accepted, 50 cards conserved, no exceptions, termination. **0 violations; no rule discrepancy that changes a game outcome.** What it did report:

- **No `DRAW` end reason (§8.10).** A tied tiebreak ends with `TURN_CAP` or `DEPLOY_LOCK` and `winner_player() == -1` (`is_draw()`); the protocol layer maps that to `DRAW`. Intentional, documented in `GameEnd.hpp`.
- **Tiebreak scores are not exposed (§8.10).** `GAME_OVER` reports the scoring; the engine only returns the winner. A server can recompute distinct types and totals from `land_count`, but the demo's end screen and snapshot builder (roadmap 15) should get a small accessor. *To do with step 15.*
- **Pitched cards reach the graveyard at resolution, not at push (§3.4).** Cost cards (Rummage discard, Counter's extra card) go there immediately. This only changes `graveyard_size()` and graveyard order while a window is open; nothing in the rules can read the graveyard mid-stack. Left as is; the demo should show a pitched card on the stack, not in the graveyard, until it resolves.
- **A lone Island does not open a window (§4.3).** The counter would be unpayable, so the engine auto-passes (see the assumptions below). Rules-correct; it slightly changes the GDD's timing-leak reasoning (no window now means "no Island, or an Island with no partner card").
- **Pitching the last card before the deploy is allowed (§3.2)** and loses by deploy lock (`PitchingTheLastCardLocksTheDeploy`). The GDD does not say; kept as is.
- **Server concerns, not engine** (as the GDD scopes them): the randomized auto-pass delay (§4.3), the 60s / 90s disconnect accounting, and filtering private payloads (`choice_options()` is private to `choice_player()`; the snapshot builder must mask it).
- **Coverage notes:** the audit also listed rules it believed untested. Two were wrong (a countered Rummage still discarding its cost is asserted in `CounterFizzlesPitchEffect`), and one was real: a countered ramped deploy returning to the library, now covered. Still without a dedicated test, and judged not worth contorting a setup for: a Save fizzling the caster's own Flicker (needs a five-deep counter chain), and the turn cap meeting a pending window or choice (unreachable: `end_turn` is rejected while either is pending).

## Known deviations from the GDD (intentional, temporary)

- **Mandatory deploy is not enforced by `end_turn`.** The GDD makes the deploy mandatory but the protocol has no `END_TURN` intent: the server's main-phase timer auto-deploys a random legal card instead. The engine therefore lets `end_turn` pass without a deploy (it is also what lets tests burn through the library), and only ends the game when the hand is empty at the time the deploy is due.
- **Main-phase timeout semantics are an interpretation.** The GDD says expiry auto-deploys and records a strike; it does not say what happens when the deploy is already made. The engine ends the turn in that case (still +1 strike). An auto-deploy that opens a window or choice leaves the turn running; the server's next expiry ends it.
- **RNG portability.** Timer picks use `mt19937_64 % n` and are identical across standard libraries; the library shuffle uses `std::shuffle`, whose result may differ between libstdc++ and libc++, so a seed replays bit-for-bit on the same toolchain only.
- **`ActionStack` is a bare LIFO.** The fixed stack grammar (§4.1) is enforced only by how `LandGameMatch` pushes (one sorcery-speed bottom item, then reactive pitches), not by the container.
- **Swamp Extract with an empty opponent hand** is implemented (no choice offered) but untested: no reachable test setup empties a hand.

## Decisions

**Assumptions where the GDD is silent (please confirm or overrule):**

- A deploy or main-phase pitch that is countered still uses up that turn's deploy / pitch allowance, and a countered Ramp grants no extra deploy.
- A player can open a priority window only if they hold an Island **and** at least one other card (Counter's cost), or a Plains with a stack item aiming at one of their lands. Otherwise the engine auto-passes for them.
- A counter may target any item on the stack, including the caster's own.
- Flicker re-triggers a land's _printed_ ETB, so a land that originally entered vanilla (ramp) still triggers it when flickered.
- A Save may target a land that the caster's own Flicker is targeting, which makes that Flicker fizzle.

**Decided:**

1. **Cards are addressed by instance ID, never by hand index** (GDD §6.3), decided before the priority window because discards mid-flight shift indices. Unknown ids are an illegal move (`false`) at the match level and a contract violation (throws) at the container level.
2. **Each accepted action is one `Action` in an append-only log**; timer events are logged as events (not as the action they expand to) so replay re-derives the random pick from the seeded RNG.

## Open decisions

1. **How to embed the engine in the Godot demo** (see roadmap step 15):
   - **A. GDExtension** (godot-cpp) wrapping `LandGameMatch` in-process. Fewest moving parts at runtime, but couples the demo to a Godot / godot-cpp version and a second build system, and it does not exercise the GDD's client/server split.
   - **B. A small local host process** speaking the GDD's JSON intents / masked snapshots (§8) over a WebSocket, with Godot's built-in `WebSocketPeer` as the client. Nothing to compile on the Godot side, matches the architecture the project is heading to, and the snapshot and intent code is reusable in Phase 2. Needs a masked-snapshot (`§8.7`) builder and JSON encoding that the engine does not have yet.
   - **C. Stdio bridge:** Godot launches a CLI and exchanges JSON lines. Simplest, but throwaway.
   - Recommendation: **B**, starting with the snapshot builder, because that work is needed for Phase 2 anyway. To be decided with the user before any code.
2. **Which Godot MCP server to use**, and whether it is installed in this environment. Unverified: check what is available before planning around its capabilities.

## Roadmap

Completed slices, in order: main-phase pitches (Ramp, Rummage); Island Draw; priority window + Island Counter; battlefield identity + Destroy + Save; Reclaim; choice prompts (Extract, Surveil); Flicker; first-draw fixes; stacked decks; game end; action log + replay + `legal_actions`; timer events and strikes; `RandomBot`, `state_digest` and `lands_sim`; `MatchConfig`; sanitizer build.

Remaining, in the recommended order:

13. ~~**Close-out audit against the GDD.**~~ Done (see Audit above); its reporting items are folded into step 15.
14. **Smarter bots** than uniform random, so win-rate numbers mean something. Random bots only show the engine is sound; with them the shipped "both" compensation package favors P2 (~53-55%) while either single rule is near even, which is not balance evidence yet. May be deferred behind the demo.
15. **Godot demo: pair the engine with a playable demonstration of the mechanics.**
    - Goal: a Godot 4 scene where a human can play a match against a bot and see each mechanic work (deploy, pitch, priority window with counter and save, the choice prompts, Memory Lapse on top of the library, the shared graveyard, win and end screens).
    - Method: drive the Godot editor through an MCP server (run the project, inspect the scene tree, read errors, take screenshots) so the loop of change, run, look and fix stays closed.
    - Engine prerequisites, whatever the embedding: tiebreak scores for the end screen (the audit's open reporting item), a per-player **masked snapshot** (own hand visible, opponent hand as a count, private peeks only to the peeker, `library_top_known` after a Memory Lapse; GDD §8.7), and a serialization of `Action` to and from JSON (§8.4-8.6).
    - First step: resolve the open decisions above, then confirm what the MCP server can do before designing around it.
16. **Phase 2 (WebSocket server)** per GDD §10, reusing the snapshot and intent code from step 15 if the demo takes route B.

Order is a recommendation; reorder if a later slice unblocks something earlier.
