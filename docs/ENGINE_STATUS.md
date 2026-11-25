# Engine status & roadmap

Living tracker of how far `engine/` is from what `GameDesignDocument.md` describes. The GDD is the source of truth for rules; this file records only **what is built**, **what is deliberately different for now**, and **what comes next**. Update it in the same commit that changes the status.

## Implemented

| Area | State | GDD |
| :--- | :--- | :--- |
| Shared 50-card deck, seeded shuffle | done | §3.1 |
| Deal 5 / 6, P1 skips turn-1 draw | done | §3.1, §3.2 |
| Turn bookkeeping, one deploy per turn | done | §3.2 |
| Deploy through the stack, auto-resolve | done (no priority window) | §4.2 |
| Win check (Domain / Mono) after each resolution | done | §3.3 |
| Shared graveyard | exists, **nothing writes to it yet** | §3.1 |
| Deploy ETBs (Flicker, Draw, Extract, Destroy, Reclaim) | **not implemented**; `AbilityId` is stored on the stack item only | §3.5 |
| Pitch abilities | **not implemented** | §3.4, §3.5 |
| Priority windows, Counter, Save | **not implemented** | §4 |
| Choice prompts (Extract, Surveil, Flicker targets) | **not implemented** | §8.6 |
| Mandatory deploy / empty-hand end / turn cap / tiebreak | **not implemented** | §3.6 |
| Timers, AFK strikes | **not implemented** (belong to the server layer, not the pure engine) | §4.4 |
| Action log + replay | **not implemented** | §6.3 |

## Known deviations from the GDD (intentional, temporary)

- **Hand indices** are used in action APIs (`play_land(player, hand_index)`). The GDD (§6.3) bans them in favor of stable `instance_id`s. See the open decision below.
- `ActionStack` is a bare LIFO; the fixed stack grammar (§4.1) is not enforced.
- `LandGameMatch::resolve_top_action` resolves immediately after every push.

## Open decisions

1. **Hand indices vs. instance IDs** in the match API. Not yet decided. Until it is, new actions follow the existing hand-index style, and the choice is revisited **before** the priority window lands, because Counter/Save target stack items and cards by identity.

## Roadmap (one slice = one reviewable commit series)

1. **Main-phase pitches without choices:** Forest Ramp, Mountain Rummage. 1 pitch per turn, ramped deploy enters vanilla, pitched card goes to the graveyard.
2. **Deploy ETBs without choices:** Island Draw; Mountain Destroy / Forest Reclaim / Swamp Extract once a target-choice mechanism exists (see 5).
3. **Stack grammar + priority window:** opponent gets one window after each push; pass resolves the whole stack top-to-bottom; auto-pass when the opponent has no Island and no applicable Plains (§4.3).
4. **Reactive pitches:** Island Counter (Memory Lapse semantics, counter wars), Plains Save (fizzle on target).
5. **Choice prompts:** Swamp Surveil 2, Extract, Plains Flicker (including the anti-loop rule).
6. **Game end:** mandatory deploy, empty-hand loss, turn-50 cap, tiebreaker scoring.
7. **Action log + replay** from seed, then the bot simulation harness (GDD Phase 1).

Order is a recommendation; reorder if a later slice unblocks something earlier.
