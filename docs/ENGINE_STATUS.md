# Engine status & roadmap

Living tracker of how far `engine/` is from what `GameDesignDocument.md` describes. The GDD is the source of truth for rules; this file records only **what is built**, **what is deliberately different for now**, and **what comes next**. Update it in the same commit that changes the status.

## Implemented

| Area | State | GDD |
| :--- | :--- | :--- |
| Shared 50-card deck, seeded shuffle, random opaque instance ids | done | §3.1, §6.3 |
| Deal 5 / 6, P1 skips turn-1 draw | done | §3.1, §3.2 |
| Turn bookkeeping, one deploy per turn | done | §3.2 |
| Deploy and pitch through the stack; priority window for the pusher's opponent (`priority_player`, `pass_priority`); auto-pass when the opponent cannot respond | done | §4.2, §4.3 |
| Win check (Domain / Mono) after each resolution | done | §3.3 |
| Shared graveyard | exists, **nothing writes to it yet** | §3.1 |
| Deploy ETBs Flicker, Extract, Destroy, Reclaim | **not implemented**; `AbilityId` is stored on the stack item only | §3.5 |
| Main-phase pitch: Forest Ramp, Mountain Rummage (`pitch_land`, 1/turn, goes through the stack) | done | §3.2, §3.4, §3.5 |
| Island Draw ETB (no-op on empty library) | done | §3.5 |
| Ramped deploy enters vanilla (`AbilityId::NONE`) | done, tested via Island Draw | §3.2 |
| Island Counter (`cast_counter`): counters any stack item, counter wars resolve LIFO, countered deploy goes on top of the library (Memory Lapse), countered pitch fizzles | done | §3.5, §4.2 |
| Swamp Surveil, Plains Save | **not implemented** (`pitch_land` returns false for them; Save has no valid targets until Destroy/Flicker exist) | §3.5 |
| Choice prompts (Extract, Surveil, Flicker targets) | **not implemented** | §8.6 |
| Mandatory deploy / empty-hand end / turn cap / tiebreak | **not implemented** | §3.6 |
| Timers, AFK strikes | **not implemented** (belong to the server layer, not the pure engine) | §4.4 |
| Action log + replay | **not implemented** | §6.3 |

## Known deviations from the GDD (intentional, temporary)

- `ActionStack` is a bare LIFO; the fixed stack grammar (§4.1) is enforced only by how `LandGameMatch` pushes (one sorcery-speed bottom item, then counters), not by the container.
- No timers: the 10s window never expires on its own. The server layer will call `pass_priority` on timeout.

## Decisions

0. **Assumptions where the GDD is silent (please confirm or overrule):**
   - A deploy or main-phase pitch that is countered still uses up that turn's deploy / pitch allowance, and a countered Ramp grants no extra deploy.
   - A player can open a priority window only if they hold an Island **and** at least one other card (Counter's cost). Otherwise the engine auto-passes for them.
   - A counter may target any item on the stack, including the caster's own.

1. **Cards are addressed by instance ID, never by hand index** (GDD §6.3). Decided before the priority window, because discards mid-flight shift indices. `Deck::shuffle` assigns each card a random opaque id (a permutation of 0..49) so ids never reveal land type or library position. `play_land`, `PlayerState::take_from_hand`, and `Graveyard::take_card` take ids. Unknown ids are an illegal move (`false`) at the match level and a contract violation (throws) at the container level.

## Roadmap (one slice = one reviewable commit series)

1. ~~**Main-phase pitches without choices:** Forest Ramp, Mountain Rummage.~~ Done.
2. ~~**Deploy ETB without choices:** Island Draw.~~ Done. Destroy / Reclaim / Extract / Flicker need a target-choice mechanism and are folded into slice 5.
3. ~~**Priority window + Island Counter** (Memory Lapse semantics, counter wars).~~ Done.
4. **Battlefield identity:** track the cards on each battlefield (not just counts) so lands can be targeted; then Mountain Destroy, Plains Save (fizzle on target) and its interaction with Counter.
5. **Choice prompts:** Swamp Surveil 2, Extract, Plains Flicker (including the anti-loop rule).
6. **Game end:** mandatory deploy, empty-hand loss, turn-50 cap, tiebreaker scoring.
7. **Action log + replay** from seed, then the bot simulation harness (GDD Phase 1).

Order is a recommendation; reorder if a later slice unblocks something earlier.
