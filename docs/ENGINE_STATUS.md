# Engine status & roadmap

Living tracker of how far `engine/` is from what `GameDesignDocument.md` describes. The GDD is the source of truth for rules; this file records only **what is built**, **what is deliberately different for now**, and **what comes next**. Update it in the same commit that changes the status.

## Implemented

| Area | State | GDD |
| :--- | :--- | :--- |
| Shared 50-card deck, seeded shuffle, random opaque instance ids | done | §3.1, §6.3 |
| Deal 5 / 6, P1 skips turn-1 draw | done | §3.1, §3.2 |
| Turn bookkeeping, one deploy per turn | done | §3.2 |
| Deploy through the stack, auto-resolve | done (no priority window) | §4.2 |
| Win check (Domain / Mono) after each resolution | done | §3.3 |
| Shared graveyard | exists, **nothing writes to it yet** | §3.1 |
| Deploy ETBs (Flicker, Draw, Extract, Destroy, Reclaim) | **not implemented**; `AbilityId` is stored on the stack item only | §3.5 |
| Main-phase pitch: Forest Ramp, Mountain Rummage (`pitch_land`, 1/turn, goes through the stack) | done | §3.2, §3.4, §3.5 |
| Ramped deploy enters vanilla (`AbilityId::NONE`) | done, but **not observable in tests yet** because no ETB resolves; add an assertion when the first deploy ETB lands | §3.2 |
| Swamp Surveil, Island Counter, Plains Save | **not implemented** (`pitch_land` returns false for them) | §3.5 |
| Priority windows, Counter, Save | **not implemented** | §4 |
| Choice prompts (Extract, Surveil, Flicker targets) | **not implemented** | §8.6 |
| Mandatory deploy / empty-hand end / turn cap / tiebreak | **not implemented** | §3.6 |
| Timers, AFK strikes | **not implemented** (belong to the server layer, not the pure engine) | §4.4 |
| Action log + replay | **not implemented** | §6.3 |

## Known deviations from the GDD (intentional, temporary)

- `ActionStack` is a bare LIFO; the fixed stack grammar (§4.1) is not enforced.
- `LandGameMatch::resolve_top_action` resolves immediately after every push.

## Decisions

1. **Cards are addressed by instance ID, never by hand index** (GDD §6.3). Decided before the priority window, because discards mid-flight shift indices. `Deck::shuffle` assigns each card a random opaque id (a permutation of 0..49) so ids never reveal land type or library position. `play_land`, `PlayerState::take_from_hand`, and `Graveyard::take_card` take ids. Unknown ids are an illegal move (`false`) at the match level and a contract violation (throws) at the container level.

## Roadmap (one slice = one reviewable commit series)

1. ~~**Main-phase pitches without choices:** Forest Ramp, Mountain Rummage.~~ Done.
2. **Deploy ETBs without choices:** Island Draw; Mountain Destroy / Forest Reclaim / Swamp Extract once a target-choice mechanism exists (see 5).
3. **Stack grammar + priority window:** opponent gets one window after each push; pass resolves the whole stack top-to-bottom; auto-pass when the opponent has no Island and no applicable Plains (§4.3).
4. **Reactive pitches:** Island Counter (Memory Lapse semantics, counter wars), Plains Save (fizzle on target).
5. **Choice prompts:** Swamp Surveil 2, Extract, Plains Flicker (including the anti-loop rule).
6. **Game end:** mandatory deploy, empty-hand loss, turn-50 cap, tiebreaker scoring.
7. **Action log + replay** from seed, then the bot simulation harness (GDD Phase 1).

Order is a recommendation; reorder if a later slice unblocks something earlier.
