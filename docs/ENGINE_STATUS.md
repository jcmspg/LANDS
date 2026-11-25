# Engine status & roadmap

Living tracker of how far `engine/` is from what `GameDesignDocument.md` describes. The GDD is the source of truth for rules; this file records only **what is built**, **what is deliberately different for now**, and **what comes next**. Update it in the same commit that changes the status.

## Implemented

| Area | State | GDD |
| :--- | :--- | :--- |
| Shared 50-card deck, seeded shuffle, random opaque instance ids | done | §3.1, §6.3 |
| Stacked-deck constructors (`Deck(vector<Card>)`, `LandGameMatch(Deck)`) for tests, replays, and the balance-configuration simulation | done | §10 |
| Deal 5 / 6; P1 skips only its turn-1 draw, P2 draws normally on its first turn; an empty library skips draw steps instead of throwing | done | §3.1, §3.2 |
| Turn bookkeeping, one deploy per turn | done | §3.2 |
| Deploy and pitch through the stack; priority window for the pusher's opponent (`priority_player`, `pass_priority`); auto-pass when the opponent cannot respond | done | §4.2, §4.3 |
| Win check (Domain / Mono) after each resolution | done | §3.3 |
| Shared graveyard | exists, **nothing writes to it yet** | §3.1 |
| Forest Reclaim ETB: graveyard target declared up front, required iff the graveyard is non-empty, can take cards the opponent pitched, nothing if countered | done | §3.5, §8.4 |
| Plains Flicker ETB: target (another land you control) declared up front, required iff you control a land; re-triggers the land's ETB inline with fresh choices (`FLICKER_DESTROY` / `FLICKER_RECLAIM` / `FLICKER_EXTRACT`, all answered with `choose_card`); no priority window so it cannot be countered; flickering a Plains fizzles; fizzles if the target was saved | done | §3.5, §8.6 |
| Main-phase pitch: Forest Ramp, Mountain Rummage (`pitch_land`, 1/turn, goes through the stack) | done | §3.2, §3.4, §3.5 |
| Island Draw ETB (no-op on empty library) | done | §3.5 |
| Ramped deploy enters vanilla (`AbilityId::NONE`) | done, tested via Island Draw | §3.2 |
| Island Counter (`cast_counter`): counters any stack item, counter wars resolve LIFO, countered deploy goes on top of the library (Memory Lapse), countered pitch fizzles | done | §3.5, §4.2 |
| Battlefield tracks cards (not only counts); `battlefield(player)` getter | done | §6.3 |
| Mountain Destroy ETB: target declared up front at push time, required iff the opponent has a land, destroyed land goes to the graveyard, fizzles if the target is saved or gone (Mountain still enters) | done | §3.5 |
| Plains Save (`cast_save`): window opens only if the player holds a Plains and a stack item targets one of their lands; can itself be countered | done | §3.5, §4.3 |
| Choice prompts (`choice_player` / `choice_type` / `choice_options`): resolution pauses, every other action is rejected until the choosing player answers. Timeouts and random auto-choices are the server layer's job | done | §8.6 |
| Swamp Extract ETB (`choose_card`): no effect if the opponent's hand is empty (**implemented but untested**: no way to empty a hand in a test yet) | done | §3.5 |
| Swamp Surveil 2 pitch (`choose_surveil`): peeks without drawing, bins any of the peeked cards, orders the rest back on top (`library_top_order[0]` becomes the top); short or empty library handled | done | §3.5 |
| Choice prompts (Extract, Surveil, Flicker targets) | **not implemented** | §8.6 |
| Game end: result API (`game_over`, `winner_player`, `end_reason`, `is_draw`), Domain / Mono win, `concede`, deploy lock (empty hand when the deploy is due: at turn start after the draw, or at `end_turn` with no deploy made), turn cap (end of round 25 = turn 50), tiebreak scoring (`WinEvaluator::tiebreak_winner`). Every action is rejected afterwards and pending windows / choices are cleared | done | §3.3, §3.6, §8.10 |
| Timer events (`main_phase_timeout`, `priority_timeout`, `choice_timeout`): auto-deploy a uniformly random legal card + target, auto-pass, uniformly random legal choice; the match-owned seeded RNG makes them replay exactly and only the event is logged | done | §3.2, §4.4, §8.6 |
| AFK strikes (`afk_strikes`, 3 = `AFK_FORFEIT`); `disconnect_forfeit` for the server's blown-grace case | done | §3.6 |
| The clocks themselves (60s / 10s / 15s, disconnect grace) | not an engine concern: the server's event loop owns them (§6.3) and calls the timer events above | §4.4 |
| `MatchConfig` first-player compensation packages (GDD §3.1 balance watchpoint) | done | §3.1, §10 |
| `RandomBot` (uniform legal play; timer and stalling profiles) and `lands_sim` (`--matches`, `--seed`, `--timer-percent`, `--idle-percent`); `state_digest()` | done | §10 |
| `Action` type, `apply`, append-only `action_log()`, `replay(seed, log)`; rejected actions are never logged | done | §6.3 |
| `legal_actions(player)`: exactly the set of actions the engine accepts (concede aside), checked against a brute-force oracle over random games; `deploy_due()` | done | §10 |

## Known deviations from the GDD (intentional, temporary)

- **Main-phase timeout semantics are an interpretation.** The GDD says expiry auto-deploys and records a strike; it does not say what happens when the deploy is already made. The engine ends the turn in that case (still +1 strike). An auto-deploy that opens a window or choice leaves the turn running; the server's next expiry ends it.
- **RNG portability.** Timer picks use `mt19937_64 % n` and are identical across standard libraries; the library shuffle uses `std::shuffle`, whose result may differ between libstdc++ and libc++, so a seed replays bit-for-bit on the same toolchain only.
- **Mandatory deploy is not enforced by `end_turn`.** The GDD makes the deploy mandatory but the protocol has no `END_TURN` intent: the server's main-phase timer auto-deploys a random legal card instead. The engine therefore lets `end_turn` pass without a deploy (it is also what lets tests burn through the library), and only ends the game when the hand is empty at the time the deploy is due.
- `ActionStack` is a bare LIFO; the fixed stack grammar (§4.1) is enforced only by how `LandGameMatch` pushes (one sorcery-speed bottom item, then counters), not by the container.
- No timers: the 10s window never expires on its own. The server layer will call `pass_priority` on timeout.

## Decisions

0. **Assumptions where the GDD is silent (please confirm or overrule):**
   - A deploy or main-phase pitch that is countered still uses up that turn's deploy / pitch allowance, and a countered Ramp grants no extra deploy.
   - A player can open a priority window only if they hold an Island **and** at least one other card (Counter's cost). Otherwise the engine auto-passes for them.
   - A counter may target any item on the stack, including the caster's own.
   - Flicker re-triggers a land's *printed* ETB, so a land that originally entered vanilla (ramp) still triggers it when flickered.
   - A Save may target a land that the caster's own Flicker is targeting, which makes that Flicker fizzle.

1. **Cards are addressed by instance ID, never by hand index** (GDD §6.3). Decided before the priority window, because discards mid-flight shift indices. `Deck::shuffle` assigns each card a random opaque id (a permutation of 0..49) so ids never reveal land type or library position. `play_land`, `PlayerState::take_from_hand`, and `Graveyard::take_card` take ids. Unknown ids are an illegal move (`false`) at the match level and a contract violation (throws) at the container level.

## Roadmap (one slice = one reviewable commit series)

1. ~~**Main-phase pitches without choices:** Forest Ramp, Mountain Rummage.~~ Done.
2. ~~**Deploy ETB without choices:** Island Draw.~~ Done. Destroy / Reclaim / Extract / Flicker need a target-choice mechanism and are folded into slice 5.
3. ~~**Priority window + Island Counter** (Memory Lapse semantics, counter wars).~~ Done.
4. ~~**Battlefield identity, Mountain Destroy, Plains Save.**~~ Done.
5. ~~**Forest Reclaim.**~~ Done.
6. ~~**Choice prompts:** Swamp Extract and Surveil 2.~~ Done.
7. ~~**Plains Flicker.**~~ Done.
8. ~~**Game end:** deploy lock, turn-50 cap, tiebreaker scoring, concede.~~ Done.
9. ~~**Timer events and the random-legal-action rule** (builds on `legal_actions`):~~ Done. engine-side effects of timeouts (auto-deploy a uniformly random legal card, random legal choice, auto-pass), driven by a match-owned seeded RNG so they replay; AFK strike accounting and the 3-strike forfeit.
10. ~~**Action log + replay.**~~ Done (slice 9, delivered before the timer events).
11. ~~**Simulation harness:**~~ Done. `lands_sim` plays N random-bot matches (default 10,000, ~2s), checks termination, replay (`state_digest`) and reports outcomes and action coverage; ctest runs a default and a stalling profile.
12. ~~**Compensation configurations.**~~ Done: `MatchConfig` (`draw_skip_only`, `sixth_card_only`, `both` = the default), accepted by both constructors and `replay`; `lands_sim --config all` (default) compares the three.
13. **Smarter bots** than uniform random, so the win-rate numbers mean something. Random bots only show the engine is sound; with them the shipped "both" package favors P2 (~55%) while either single rule is near even, but that is not balance evidence yet.
14. **Phase 1 close-out review:** re-read the GDD end to end against the engine, then start Phase 2 (the WebSocket server).

Order is a recommendation; reorder if a later slice unblocks something earlier.
