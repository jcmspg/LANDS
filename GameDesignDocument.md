# LANDS

# GAME DESIGN DOCUMENT (GDD) & MASTER SYSTEM ARCHITECTURE
## Project: LANDS (Working Title)

---

> **Document Version:** 2.1.0 (Master Unified — Finalized Ruleset)  
> **Changelog (2.1.0):** All open questions resolved as normative rules — disconnect grace budget (60s per drop / 90s cumulative), choice-timeout policy (random legal choice + AFK strike), new 60-second main-phase timer with auto-deploy, unified AFK strike system (3 strikes = forfeit), Player 2 opens with a 6th card (double compensation, pending Phase 1 A/B/C data), Forest ramp confirmed as written.  
> **Supersedes:** 2.0.0 and 1.0.0. Where this document conflicts with earlier versions, this document wins.  
> **Target Platforms:** PC (Godot Desktop Client) / Web (Godot WebAssembly)  
> **Architecture Pairing:** Authoritative C++ Headless Server + Reverse Proxy (TLS) + Docker + Hetzner VPS

---

## 1. System Goals and Objectives

### 1.1 Core Goals
* **Functional Objective:** Deliver a fully authoritative, lightweight, competitive 1v1 turn-based digital card game built exclusively on the 5 MTG basic land types (Plains, Island, Swamp, Mountain, Forest) with homebrew names/effects inspired by MTG color identity. The game must fully support stack-based priority, automated matchmaking, persistent user accounts, post-match currency rewards, and dynamic client card animations within 3–5 minute match loops.
* **Technical Objective:** Maintain strict 100% separation between the headless C++ game simulation engine and the Godot 4 client UI. Achieve $< 50\text{ms}$ server state calculation overhead per action and support 1,000+ concurrent active game rooms on a single lightweight Hetzner VPS instance, using a **single-threaded event loop with zero locks**.
* **Quality Objective:** Guarantee zero client-side authority over game state (player identity and room membership are derived server-side from the authenticated socket session — **never** from client payloads), provide deterministic state execution (seeded shuffle + append-only action log for full replayability), handle unexpected socket disconnects gracefully via session-token re-authentication and instant full-state resyncs, and maintain smooth UI responsiveness using asynchronous Godot Tweens.

### 1.2 Core Pillars
* **Pure Skill & Micro-Tactics:** Zero mana-gating, zero card costs. Perfect-information management of shared zones, bluffing, hand-resource tension, and stack timing drive the victory.
* **Every Card Is Two Cards:** The central mechanic — every land can be **Deployed** (board progress toward a win) or **Pitched** (a spell-like trick). Never both. Every card in hand is a permanent decision.
* **Ultra-Fast Loops:** Matches last 3–5 minutes and are **guaranteed bounded** (hard turn cap + deck-out rules). Ideal for rapid matchmaking and casual competitive play.
* **Decoupled Architecture:** 100% authoritative C++ engine execution; zero client-side game state authority.

---

## 2. Executive Summary & High Concept

### 2.1 High Concept
*LANDS* is a lightweight, turn-based, competitive 1v1 digital card game played exclusively with the 5 basic land types. Both players draw from a **single shared 50-card library** and discard into a **single shared graveyard**. Every card has exactly two uses: **Deploy** it to the battlefield (it counts toward your win condition and triggers an enters-the-battlefield ability) or **Pitch** it from hand (it never touches the battlefield and instead produces a spell-like effect). Victory is purely positional: assemble one land of every type, or five lands of the same type.

### 2.2 Core Game Loop Overview
1. Two players connect to a server instance via WebSockets and are matched into a room.
2. Players draw opening hands from a shared library of 50 basic lands (10 of each type): Player 1 draws 5, Player 2 draws 6 (first-player compensation, §3.1).
3. The active player **must deploy exactly 1 land** onto their battlefield (mandatory if able); the deploy is pushed to the LIFO Action Stack and its ETB ability will trigger on resolution.
4. The active player may also make **at most 1 main-phase pitch** (Mountain / Swamp / Forest effects).
5. After every stack push, the opposing player receives one time-bounded priority window to respond with an **instant-speed pitch**: an Island (counter) or a Plains (save).
6. Once a player passes (or times out), the stack resolves strictly top-to-bottom with no further interruption.
7. Immediately after **every** stack resolution step, the server checks win conditions **for both players**: **Domain** (all 5 land types on your battlefield) or **Mono** (5 lands of the same type).

### 2.3 Design Rationale
* **Deploy-vs-Pitch tension:** every card is either board progress or a trick, never both. Spending interaction always costs win-condition material, so counters, saves, and rummages are real decisions rather than free value.
* **Shared library & shared graveyard:** because both players draw from and discard into the same zones, the game gains a *Dandan*-style memory and zone-manipulation skill layer — countered lands sit face-up on top of the shared library, Surveil sculpts both players' draws, and Forest can steal anything either player has ever pitched.
* **The one-sentence speed rule:** *battlefield plays are sorcery-speed; hand pitches of Island and Plains are instant.* Only stack-interactive effects need instant timing — this keeps priority windows rare (fast matches) and matches MTG's color pie, where blue and white own instant-speed interaction.

---

## 3. Core Mechanics, Ruleset & Edge Cases

### 3.1 Deck & Setup
* **Shared Library:** Matches use a **single 50-card shared library** consisting of exactly **10 copies of each Basic Land** (Plains, Island, Swamp, Mountain, Forest). Both players draw from it.
* **Shared Graveyard:** There is a **single shared graveyard zone**. Pitched cards, discarded cards, destroyed lands, and surveilled cards all go to the same public pile — a direct consequence of the shared library. Graveyard contents and order are **public information**.
* **Starting Hands:** Player 1 draws **5 cards**; Player 2 draws **6 cards** (first-player compensation).
* **Opening Turn:** Player 1 skips their draw on Turn 1. Player 2 draws normally on their first turn. Thereafter each player draws 1 card at the start of their turn.
* **Balance note — double compensation (deliberate, provisional):** Player 2 is currently compensated **twice** for going second: the 6th starting card *and* Player 1's turn-1 draw skip. Both rules are kept for now; the Phase 1 simulation measures P1/P2 win rates under all three configurations (draw-skip only / 6th card only / both — see Phase 1 balance watchpoints) and the final compensation package will be chosen from that data.
* **No Maximum Hand Size.**

### 3.2 Turn Structure
Every turn follows a fixed sequence. All battlefield plays are sorcery-speed and occur only in the active player's main phase.

```
 ┌──────────────────────────────────────────────────────────────────────┐
 │ 1. DRAW STEP                                                         │
 │    Active player draws 1 from the shared library.                    │
 │    Exceptions: Player 1 skips this on Turn 1; skipped if library     │
 │    is empty (play continues without draws).                          │
 ├──────────────────────────────────────────────────────────────────────┤
 │ 2. MAIN PHASE — 60-SECOND TIMER  (actions taken one at a time,       │
 │    in any order; each action fully resolves through the stack        │
 │    before the next begins)                                           │
 │    • MANDATORY DEPLOY: exactly 1 land (must deploy if hand is        │
 │      non-empty).                                                     │
 │    • OPTIONAL MAIN-PHASE PITCH: at most 1 per turn                   │
 │      (Mountain / Swamp / Forest only).                               │
 │    • RAMP DEPLOY: if a Forest was pitched this turn, the player      │
 │      may deploy 1 additional land; it enters vanilla (no ETB).       │
 │    • TIMER: one 60s clock covers the whole main phase (pauses        │
 │      while the stack or a choice prompt is live); on expiry the      │
 │      server auto-deploys a random legal card (+1 AFK strike).        │
 ├──────────────────────────────────────────────────────────────────────┤
 │ 3. END STEP                                                          │
 │    Turn passes to the opponent. (Win/end conditions were already     │
 │    checked continuously after every stack resolution step.)          │
 └──────────────────────────────────────────────────────────────────────┘
```

* **Deploying is mandatory.** If the active player's hand is non-empty, they must make their 1 deploy. If their hand is empty when the deploy is required, the game ends (see §3.6).
* **Deploy limits:** exactly 1 deploy per turn, plus at most 1 extra deploy granted by a resolved Forest ramp pitch. The **second** deploy of the turn is always the ramped one and always enters vanilla (its ETB does not trigger). A ramped deploy is still a stack item and can still be countered.
* **Pitch limits:** at most **1 main-phase pitch per turn** (Mountain, Swamp, Forest). Reactive instant-speed pitches (Island, Plains) are **not** limited per turn — only by cards in hand.
* **Main-phase timer (normative):** the active player's main phase runs on a single **60-second timer** covering their mandatory deploy and optional main-phase pitch. The clock **pauses** whenever the stack is live (priority windows) or a choice prompt is pending, and resumes when the main phase regains control. On expiry the server **auto-deploys a uniformly random legal card** from the player's hand — any ETB choices for that deploy are resolved via the same random-legal-choice rule as choice timeouts (§8.6) — and records **1 AFK strike** (§3.6).

### 3.3 Win Conditions
Checked for **both players** immediately after **every** stack resolution step. A player wins **instantly** the moment either condition holds on their battlefield:
1. **Domain Win:** Controlling at least **1 of each of the 5 basic land types** simultaneously (1 Plains, 1 Island, 1 Swamp, 1 Mountain, 1 Forest).
2. **Mono Win:** Controlling **5 of the same basic land type** simultaneously (e.g., 5 Mountains).

* **Edge case — simultaneous satisfaction:** resolutions only ever add lands to one battlefield at a time, so a genuine simultaneous dual-win should be unreachable; as a deterministic engine guard, if both players satisfy a win condition after the same resolution step, **the player whose action resolved wins**.

### 3.4 The Central Mechanic: Deploy vs. Pitch
Every card in hand has exactly two mutually exclusive uses:

| Mode | Timing | Zone Movement | Effect |
| :--- | :--- | :--- | :--- |
| **DEPLOY** | Sorcery speed — your main phase only. Mandatory, exactly 1/turn (+1 via Forest ramp). | Hand → Stack → **Your Battlefield** | Land counts toward win conditions; its **ETB ability** triggers on resolution. |
| **PITCH** | *Split by color:* **Island & Plains — INSTANT speed** (only in response to a stack push, during a priority window). **Mountain, Swamp, Forest — main phase only**, on your own turn, max 1/turn. | Hand → Stack → **Shared Graveyard** (never touches the battlefield) | Spell-like effect. |

> **Why the timing split?** Only stack-interactive effects (counter, save) need instant timing. Restricting instant speed to Island/Plains keeps priority windows rare — critical for the 3–5 minute match target — and mirrors MTG's color pie (blue/white own instants).

### 3.5 Card Reference & Technical Edge-Case Rules

| Land Type | Color Identity | **DEPLOY** — ETB Ability | **PITCH** — Effect & Timing | Exhaustive Edge-Case Rules |
| :--- | :--- | :--- | :--- | :--- |
| **PLAINS** | White (*Flicker / Protection*) | **Flicker:** Choose another land you control; re-trigger its deploy-ETB. The controller picks all targets/choices for the re-triggered ETB **fresh**. | **Save** (*INSTANT*): Target a land you control; any effect currently on the stack that targets it **fizzles**. Cost: pitch the Plains alone. | • Flicker cannot target itself.<br>• Re-triggered ETBs resolve **immediately during resolution with NO priority window** — they cannot be countered.<br>• Only **deploy-ETBs** can be flickered, never pitch effects.<br>• **Anti-loop rule:** a flickered ETB that is itself a Plains flicker **fizzles** (no Plains-flicker chains).<br>• If no valid flicker target exists (e.g., empty battlefield), Plains enters vanilla.<br>• Save example: a Mountain's destroy whiffs, but the Mountain land itself **still enters the battlefield** — it was saved against, not countered.<br>• A Save on the stack can itself be countered by an Island. |
| **ISLAND** | Blue (*Counterspell / Card Draw*) | **Draw:** Draw a card from the shared library. | **Counter** (*INSTANT*): Pitch the Island **+ 1 additional card** from hand; counter target item on the stack (a deployed land **or** another pitch effect). | • **Memory Lapse semantics:** a countered *deployed land* goes on **TOP of the shared library** — face-up knowledge for both players. This is a deliberate Dandan-style mind game: the next player to draw (usually the counterer) will draw it.<br>• A countered *pitch effect* simply fizzles — the pitched card is already in the shared graveyard.<br>• **Counter wars are fully allowed** (strict LIFO): counters can counter counters.<br>• If the library is empty, Island's draw ETB does nothing; the land still enters. |
| **SWAMP** | Black (*Thoughtseize / Surveil*) | **Extract:** Look at your opponent's hand; choose 1 card — they discard it to the shared graveyard. | **Surveil 2** (*main phase*): Look at the top 2 cards of the shared library; put any number of them into the shared graveyard, the rest back on top in any order. | • If opponent's hand is empty, Swamp enters with no effect.<br>• Hand contents are revealed **only to the Swamp's controller** via a private server payload — never broadcast.<br>• The chosen discard becomes public the moment it hits the shared graveyard.<br>• Surveil peek information is private to the surveilling player; only the cards binned to the graveyard become public. Cards returned to top remain hidden knowledge. |
| **MOUNTAIN** | Red (*Stone Rain / Rummage*) | **Destroy:** Target an opponent's land; destroy it (it goes to the shared graveyard). | **Rummage** (*main phase*): Pitch the Mountain + discard 1 additional card from hand; draw 2. | • Must target a valid opponent land if at least one exists.<br>• If the opponent controls zero lands, Mountain enters vanilla.<br>• If the target leaves the battlefield before resolution (e.g., its controller fizzled the destroy with a Plains Save), the effect **fizzles safely** and Mountain still enters.<br>• Rummage is hand-neutral ($-2$, $+2$) but feeds the shared graveyard — fuel for either player's Forest. |
| **FOREST** | Green (*Regrowth / Ramp*) | **Reclaim:** Return **any card** from the shared graveyard to your hand. | **Ramp** (*main phase*): You may deploy **one ADDITIONAL land** this turn; that extra land enters **vanilla** (its ETB does not trigger). | • If the shared graveyard is empty, Forest enters vanilla.<br>• Because the graveyard is shared, **you may take cards your opponent pitched, discarded, or lost** — graveyard theft is intended.<br>• Max 1 ramp per turn (implied by the 1-main-phase-pitch limit).<br>• **Balance note:** ramp costing a card from hand is deliberate — board $+1$, hand $-2$ that turn. |

### 3.6 Game End, Anti-Stall & Match Boundedness
Every match is **bounded and deterministic** — a hard requirement for the Phase 1 bot simulation.

* **Mandatory deploys** (§3.2) prevent turtling: a player can never simply hold their hand.
* **Deck-out:** when the shared library empties, play continues without draw steps.
* **The game ends when the first of these occurs:**
  1. A player satisfies a win condition (Domain or Mono) — normal win.
  2. The active player **cannot make their mandatory deploy** (empty hand at the moment the deploy is required).
  3. The **hard turn cap** is reached: **turn 50** (25 turns per player) completes.
* **Tiebreaker scoring** (for endings 2 and 3): the winner is the player with the **most distinct land types** on their battlefield; if tied, **most total lands**; if still tied, the game is a **true draw** (no ELO change; both players receive the loss-tier coin reward).
* **Concede:** a player may concede at any time via an explicit `CONCEDE` intent; the opponent wins immediately.
* **Disconnect-forfeit policy (normative):** on socket drop, all of the room's timers (main-phase, priority, choice) pause and the disconnected player has **60 seconds** to reconnect (session-token re-auth + full resync). Each player additionally has a **cumulative disconnect grace budget of 90 seconds per match** — this prevents stall abuse by repeatedly dropping to pause timers. Exceeding any single 60-second reconnect window **or** exhausting the 90-second cumulative budget is a **forfeit loss**.
* **AFK strike system (normative, defined once here):**
  * Any timeout of the **main-phase timer** (60s, §3.2) or a **choice timer** (15s, §8.6) = **1 strike**; in both cases the server also makes a uniformly random legal action/choice on the player's behalf so the game always advances.
  * **3 strikes in a single match = forfeit loss.**
  * **Priority-window (10s) expiry does NOT count as a strike.** Letting a priority window run out is a legitimate bluffing outcome — a deliberate slow pass — and must never be punished.

---

## 4. The LIFO Stack, Priority System & Network Timing

### 4.1 Stack Grammar (Fixed & Exhaustive)
The stack has a deliberately rigid shape — this is a simplification, not an accident, and the engine may rely on it:

```
        TOP  ──►  [ Reactive pitch N  (Island Counter / Plains Save) ]
                  [ Reactive pitch …                                 ]
                  [ Reactive pitch 1  (Island Counter / Plains Save) ]
     BOTTOM  ──►  [ Exactly 1 initiated sorcery-speed action:        ]
                  [   a DEPLOYED land  -or-  a main-phase PITCH      ]
```

* The bottom item is always exactly one action initiated by the active player: a deployed land **or** a main-phase pitch (Mountain Rummage / Swamp Surveil / Forest Ramp). Main-phase pitches **do** use the stack precisely so that they can be countered.
* Everything above the bottom is a LIFO chain of **reactive** instant-speed pitches (Island counters / Plains saves). **Nothing else is ever on the stack.**
* Flicker re-triggered ETBs never touch the stack — they resolve inline (§3.5).

### 4.2 The Action Stack Lifecycle

```
 [P1 deploys Mountain ─ targets P2's Swamp]      STACK: [ Mountain ]
                     │
                     ▼
        [Priority window: P2 — 10s timer]
              │                    │
   (P2 pitches Island + 1)         │ (P2 passes / times out)
              ▼                    ▼
  STACK: [ Counter ]        [ Resolve top→bottom:
         [ Mountain ]         Mountain ETB destroys Swamp,
              │               Mountain enters battlefield ]
              ▼
        [Priority window: P1 — 10s timer]
              │                    │
   (P1 pitches Island + 1)         │ (P1 passes / times out)
              ▼                    ▼
  STACK: [ Counter₂ ]       [ Resolve top→bottom:
         [ Counter₁ ]         Counter₁ resolves — Mountain is
         [ Mountain ]         Memory-Lapsed to top of library ]
              │
              ▼
      ... (counter war continues, strict LIFO, until a pass) ...
```

1. **Push Phase:** The initiating player selects a card and mode (`DEPLOY` / `PITCH`) with all required targets $\rightarrow$ the item enters the stack.
2. **Priority Window:** After **every** stack push, the **opponent of the pusher** receives exactly **one** priority window with an explicit **10-second timer**:
   * *Island Counter:* pitch Island + 1 additional card; push a counter targeting any item on the stack.
   * *Plains Save:* pitch Plains alone; push a save targeting a land you control that a stack item is targeting.
   * *Pass / timeout:* the window closes.
3. **Resolution:** the moment either player passes (or times out), the **entire stack resolves top-to-bottom with no further interruption**. This is an explicit simplification: given the fixed stack grammar, nothing legal could be added mid-resolution anyway — resolving items are counters/saves (whose resolutions change only stack validity) and finally one sorcery-speed action, and reactive pitches are only legal in response to a *push*, never a resolution.
4. **Win Check:** after every individual resolution step, the server evaluates win conditions for both players (§3.3) and halts the match instantly on a win.

### 4.3 Smart Auto-Pass & the Timing-Leak Problem
* **Auto-Pass:** if the defending player holds **no Island** and **no applicable Plains** (i.e., no stack item targets a land they control), the server auto-passes on their behalf — no window is shown, keeping match pacing ultra-fast.
* **Info-leak rationale:** an *instantaneous* auto-pass would let a sharp opponent deduce "they hold no Island" from response latency alone. All auto-passes are therefore delivered with a **randomized human-like delay** ($300\text{ms}$–$1{,}500\text{ms}$) so a fast pass is indistinguishable from a human snap-pass.

### 4.4 Timer Reference (All Match Timers — Normative)

| Timer | Duration | Scope | On Expiry | AFK Strike? |
| :--- | :--- | :--- | :--- | :--- |
| **Main-phase** | 60s | Active player's entire main phase (mandatory deploy + optional main-phase pitch). **Pauses** while the stack or a choice prompt is live. | Server auto-deploys a **uniformly random legal card**; its ETB choices resolve via the random-legal-choice rule. | **Yes** (+1) |
| **Priority window** | 10s | Opponent of the pusher, once per stack push. | Auto-pass; the stack resolves. | **No** — a timed-out pass is a legitimate bluffing outcome (indistinguishable from a deliberate slow pass) and is never punished. |
| **Choice prompt** | 15s | Mid-resolution private choices (§8.6): Swamp extract, Surveil ordering, flickered-ETB fresh choices. | Server makes a **uniformly random legal choice**. | **Yes** (+1) |
| **Disconnect grace** | 60s per drop / 90s cumulative per match | All other timers pause while it runs. | Forfeit loss (directly — no strike accounting needed). | n/a |

**3 strikes in a match = forfeit loss** (§3.6). All timers are per-room event-loop timers (§6.3) and all expiry events are recorded in the deterministic action log.

---

## 5. User Interface & Experience (UI/UX — Godot Terminal)

```
========================================================================================
                          OPPONENT HAND (Count Only: 4 Cards)
----------------------------------------------------------------------------------------
 OPPONENT BATTLEFIELD:  [ Plains ]  [ Swamp ]  [ Mountain ]     LIBRARY: [ 31 Left ]
                                                                TOP CARD: [ MOUNTAIN ]*
----------------------------------------------------------------------------------------
                                   THE ACTION STACK
                     [ Island Counter (You) -> Targets: Mountain ]
                     [ Mountain DEPLOY (Opp) -> Targets: Forest  ]
----------------------------------------------------------------------------------------
 PLAYER BATTLEFIELD:    [ Forest ]  [ Forest ]  [ Island ]      SHARED GRAVEYARD: [ 6 ]
----------------------------------------------------------------------------------------
                               PLAYER HAND (5 Cards Visible)
                        [ Plains ] [ Island ] [ Mountain ] [ Swamp ] [ Forest ]
========================================================================================
  * TOP CARD indicator appears only when a Memory-Lapsed land is publicly known
    to be on top of the shared library.
```

### 5.1 Visual Feedback & Animation Requirements
* **Targeting Lines:** Dragging a targeting land (Mountain / Plains) from hand renders a dynamic Spline/Vector line anchoring from the held card to valid board target nodes.
* **Stack Focus Window:** Active stack items hover in a prominent center "Stack Zone" displaying a radial countdown timer indicating remaining priority response time. Deploy items and reactive pitches are visually distinct.
* **Mode Selection:** Since every card has two uses, the hand UI must disambiguate: drag-to-battlefield = **Deploy**; drag-to-graveyard (or a dedicated pitch zone) = **Pitch**. Illegal modes (e.g., pitching an Island during your own main phase) are greyed out client-side *for UX only* — the server independently rejects illegal intents (§8.8).
* **Shared-Zone Emphasis:** the shared graveyard is browsable by both players at all times; the known top-of-library card (Memory Lapse) is rendered face-up on the library.
* **Private Peeks:** Swamp hand-reveals and Surveil peeks render in a private overlay visible only to the peeking player.
* **Tween Animations:** Smooth visual card translations between Hand $\rightarrow$ Stack $\rightarrow$ Battlefield / Shared Graveyard / Library-top using Godot `Tween` physics nodes.

---

## 6. The Tech Stack & System Architecture

### 6.1 Tech Stack Breakdown

| Layer | Technology | Justification |
| :--- | :--- | :--- |
| **Client Frontend** | Godot 4 (GDScript) | Rapid 2D UI layout, built-in WebSocket client, native Tweening/Animation pipeline. |
| **Game Server** | Modern C++ (C++17 / C++20) | Blazingly fast, zero Garbage Collection pauses, optimal for LIFO stack & win-evaluator operations. |
| **Networking** | `uWebSockets` or `ixwebsocket` | Lightweight, high-throughput C++ WebSocket libraries capable of managing thousands of concurrent sockets on one event loop. |
| **Protocol** | JSON over WebSockets | Human-readable, easy to debug, trivial to parse in both C++ (`nlohmann/json`) and Godot. |
| **Database & Auth** | SQLite3 + argon2 | Zero-config, single-file ACID database for accounts, ELO ratings, and coin balances. Passwords hashed with **argon2** (bcrypt acceptable fallback). |
| **TLS Termination** | Caddy (or nginx) reverse proxy | TLS/WSS terminates at the proxy, **not** inside the C++ server — automatic Let's Encrypt certificates, zero crypto code in the game server. |
| **Infrastructure** | Docker + Hetzner VPS | Isolated, reproducible container builds deployed on existing Hetzner cluster. |

### 6.2 High-Level System Boundaries Schematic

```
┌────────────────────────────────────────────────────────┐
│                    GODOT 4 FRONTEND                    │
│   [UI / Cards]  <--->  [NetworkAdapter (WebSocket)]    │
└───────────────────────────▲────────────────────────────┘
                            │  WSS (TLS)
┌───────────────────────────▼─────────────────────────────┐
│        REVERSE PROXY (Caddy / nginx) — TLS ENDS HERE    │
└───────────────────────────▲─────────────────────────────┘
                            │  Plain WS (localhost / docker network)
┌───────────────────────────▼─────────────────────────────┐
│               C++ SERVER (single-threaded)              │
│                                                         │
│  Boundary 1: Network & Connection Gateway (uWS / JSON)  │
│                           │                             │
│  Boundary 2: Session, Lobby, Auth & Matchmaker (SQLite) │
│                           │                             │
│  Boundary 3: Room Manager (Active Games Map + Timers)   │
│                           │                             │
│  Boundary 4: Core Engine (`LandGameMatch.hpp`)          │
└─────────────────────────────────────────────────────────┘
```

### 6.3 Engine Design Notes (Normative)
* **Single-threaded event loop, no locks.** The entire server — sockets, rooms, timers, engine — runs on one `uWS` event loop thread. There is **no shared-state concurrency and no mutex anywhere** in the game path. Scale target (1,000+ rooms) is achieved through the tiny per-room state footprint, not threads.
* **Per-room timers** (60s main-phase clock, 10s priority windows, 15s choice prompts, 60s/90s-cumulative disconnect grace) are implemented as event-loop timers (`us_timer_t` or equivalent) integrated with the same loop — never as sleeping threads. Timer expiries (auto-deploy, auto-pass, random choice) are engine events recorded in the action log.
* **Deterministic engine.** Each match records its shuffle seed (seeded `std::mt19937`) plus an **append-only action log** of every accepted intent and timer event. Replaying the log against the seed reproduces the match bit-for-bit — this is the primary debugging and dispute tool, and it powers the Phase 1 simulation harness.
* **Win evaluator uses per-type COUNTS.** Each battlefield tracks `uint8_t counts[5]`. **Mono** requires `counts[t] >= 5` for some `t` — a presence bitmask alone cannot detect this. **Domain** may additionally use a derived bitmask (`counts[t] > 0` for all `t` ⇔ mask `0b11111`) as a fast path if desired.
* **Stable card instance IDs.** Every one of the 50 physical cards carries an opaque, immutable `instance_id` assigned at shuffle time (randomized so IDs never leak library order). All zones — hand, battlefield, stack, graveyard, known library-top — and all protocol messages reference cards **exclusively by instance ID**. Hand indices are banned: they are race-prone when discards (Swamp, Rummage, Island pitch cost) mutate the hand mid-flight.

---

## 7. Master System Architecture (Integrated Layer Map)

```
========================================================================================
                      MASTER SYSTEM ARCHITECTURE (GDD INTEGRATED)
========================================================================================

                 [ DOCUMENT LAYER 1: GAME DESIGN DOCUMENT (GDD) ]
                  - 50-Card Shared Library (10x5) + Shared Graveyard
                  - Deploy/Pitch Dual-Use Cards & Fixed Stack Grammar
                  - Win Conditions: Domain (5 Unique) OR Mono (5 Same)
                  - Bounded Matches: Turn Cap 50 + Deck-Out Tiebreakers

                                         │
                                         ▼

                 [ DOCUMENT LAYER 2: C++ CORE ENGINE (PHASE 1) ]
                  - `LandGameMatch.hpp` / `LandGameMatch.cpp`
                  - Count-Based Win Evaluator & Priority/Choice Timers
                  - Seeded Shuffle + Append-Only Action Log (Replay)

                                         │
                                         ▼

              [ DOCUMENT LAYER 3: WEBSOCKET & ROOM MANAGEMENT (PHASE 2) ]
                  - `uWebSockets` Gateway on Port 8080 (single loop)
                  - JSON Serialization (`nlohmann/json`)
                  - Per-Player Masked Snapshots & Session Resync Manager

                                         │
                                         ▼

                 [ DOCUMENT LAYER 4: GODOT 4 TERMINAL UI (PHASE 3) ]
                  - `WebSocketPeer` Connection Manager
                  - `CardUI.tscn` Deploy/Pitch Drag Modes & Target Nodes
                  - Board State Renderer Engine

                                         │
                                         ▼

            [ DOCUMENT LAYER 5: AUTH, MATCHMAKER & DOCKER DEPLOYMENT (PHASE 4/5) ]
                  - SQLite User Accounts (argon2) & Coin Balances
                  - Session Tokens & Reconnect Flow
                  - Docker + Caddy TLS Proxy on Hetzner Cluster
========================================================================================
```

---

## 8. Network Protocol Specification (JSON API)

All network traffic flows over WebSockets. The client sends **Intents**; the server answers every intent with either a **State Snapshot** or a machine-readable **REJECT** — illegal intents are **never silently dropped**.

**Protocol invariants (normative):**
1. **Zero client authority:** the server derives player identity and room membership from the authenticated socket session. Client intents **do not carry** `player_id` or `room_id` — any such fields, if present, are ignored (treated at most as a non-authoritative echo for logging).
2. **Session tokens:** issued at login (and refreshed at match start). A reconnecting socket must re-authenticate with its session token **before** `REQUEST_RESYNC` is honored.
3. **Instance IDs everywhere:** all card references use stable `instance_id`s (§6.3). No hand indices.
4. **Full masked snapshots, not deltas:** on **every** state change, the server sends each player a complete **per-player masked snapshot** (own hand visible; opponent hand as count only; private peeks only to the peeking player). Game state is tiny, so this eliminates the entire class of delta-desync bugs — and resync comes for free (a resync *is* just another snapshot). There is no shared "broadcast" state message.
5. **Client `seq` numbers:** every intent carries a client-chosen `seq`, echoed back in `REJECT` so the client can correlate failures.

### 8.1 Message Set Overview

| Direction | Event | Purpose |
| :--- | :--- | :--- |
| C → S | `AUTH_LOGIN` / `AUTH_GUEST` | Authenticate; receive `session_token`. |
| S → C | `AUTH_OK` | Account info + session token. |
| C → S | `QUEUE_JOIN` / `QUEUE_LEAVE` | Enter/leave the casual matchmaking queue. |
| S → C | `MATCH_FOUND` | Room created; informational room/opponent data. |
| C → S | `PLAY_CARD` | `action: "DEPLOY"` or `"PITCH"` + mode-specific fields. |
| C → S | `PRIORITY_RESPONSE` | `COUNTER` / `SAVE` / `PASS` inside a priority window. |
| S → C | `CHOICE_REQUIRED` | Private mid-resolution prompt (Swamp reveal, Surveil peek, flickered-ETB fresh choices). |
| C → S | `CHOICE_RESPONSE` | Answer to a `CHOICE_REQUIRED`. |
| C → S | `CONCEDE` | Immediate forfeit. |
| S → C | `STATE_UPDATE` | Full per-player masked snapshot (sent on every change). |
| C → S | `REQUEST_RESYNC` | After reconnect + session re-auth. |
| S → C | `FULL_RESYNC` | Same snapshot schema as `STATE_UPDATE` + re-issued pending prompt/timer. |
| S → C | `REJECT` | Machine-readable refusal of an illegal intent. |
| S → C | `GAME_OVER` | Result, reason, rewards. |

### 8.2 Auth & Session (`AUTH_LOGIN` → `AUTH_OK`)
```json
{ "event": "AUTH_LOGIN", "username": "joao", "password": "<plaintext-over-TLS>" }
```
```json
{
  "event": "AUTH_OK",
  "account_id": 881,
  "display_name": "joao",
  "session_token": "7f3c9a52-1e0b-4d8e-9d21-c0ffee00beef",
  "elo": 1204,
  "coins": 760
}
```
`AUTH_GUEST` follows the same shape without credentials and returns an ephemeral account. Passwords are verified against **argon2** hashes; TLS is guaranteed by the reverse proxy (§6.1).

### 8.3 Matchmaking (`QUEUE_JOIN` → `MATCH_FOUND`)
```json
{ "event": "QUEUE_JOIN", "seq": 7 }
```
```json
{
  "event": "MATCH_FOUND",
  "room_id": "ROOM_9021",
  "your_player_id": 1,
  "opponent": { "display_name": "Dandan_Enjoyer", "elo": 1180 }
}
```
`room_id` / `your_player_id` here are **informational** — the server already knows both from the session; the client never sends them back as authority.

### 8.4 Client Intent Payload (`PLAY_CARD`)
Note the absence of `player_id` / `room_id` (Invariant 1) and the use of instance IDs (Invariant 3).

**Deploy (Mountain, with target):**
```json
{
  "event": "PLAY_CARD",
  "seq": 42,
  "action": "DEPLOY",
  "card_instance_id": "c29",
  "target_instance_id": "c41"
}
```

**Deploy (Plains flicker — target declared up front; the re-triggered ETB's fresh choices are gathered later via `CHOICE_REQUIRED`):**
```json
{
  "event": "PLAY_CARD",
  "seq": 43,
  "action": "DEPLOY",
  "card_instance_id": "c17",
  "target_instance_id": "c08"
}
```

**Deploy (Forest — graveyard is public, so the reclaim target is declared up front):**
```json
{
  "event": "PLAY_CARD",
  "seq": 44,
  "action": "DEPLOY",
  "card_instance_id": "c33",
  "graveyard_instance_id": "c12"
}
```

**Main-phase pitch (Mountain Rummage — additional discard declared up front):**
```json
{
  "event": "PLAY_CARD",
  "seq": 45,
  "action": "PITCH",
  "card_instance_id": "c15",
  "rummage_discard_instance_id": "c22"
}
```
Swamp Surveil and Forest Ramp pitches carry no extra fields at intent time (Surveil ordering happens via `CHOICE_REQUIRED` after the private peek; Ramp simply unlocks the extra deploy).

### 8.5 Priority Window (`PRIORITY_RESPONSE`)
Sent only by the current priority holder, within the 10-second window.

**Island counter (targets any stack item by its `stack_id`):**
```json
{
  "event": "PRIORITY_RESPONSE",
  "seq": 46,
  "response": "COUNTER",
  "island_instance_id": "c02",
  "pitch_instance_id": "c33",
  "target_stack_id": "s1"
}
```

**Plains save:**
```json
{
  "event": "PRIORITY_RESPONSE",
  "seq": 47,
  "response": "SAVE",
  "plains_instance_id": "c17",
  "save_target_instance_id": "c41"
}
```

**Pass:**
```json
{ "event": "PRIORITY_RESPONSE", "seq": 48, "response": "PASS" }
```

### 8.6 Mid-Resolution Choices (`CHOICE_REQUIRED` → `CHOICE_RESPONSE`)
Private, per-player prompts used wherever a decision depends on hidden information or on a flicker's fresh choices. Each prompt runs a **15-second choice timer**; on timeout the server makes a **uniformly random legal choice** on the player's behalf and records **1 AFK strike** (§3.6 — 3 strikes = forfeit). This policy is normative. Resolution is paused while the prompt is pending (the main-phase timer also pauses, §3.2), but **no priority window opens** — flickered ETBs are explicitly not counterable.

**Swamp extract (sent only to the Swamp's controller — private payload):**
```json
{
  "event": "CHOICE_REQUIRED",
  "choice_id": "ch9",
  "choice_type": "SWAMP_EXTRACT",
  "timer_ms": 15000,
  "context": {
    "opponent_hand": [
      { "instance_id": "c05", "type": "ISLAND" },
      { "instance_id": "c19", "type": "FOREST" }
    ]
  }
}
```
```json
{ "event": "CHOICE_RESPONSE", "choice_id": "ch9", "selected_instance_ids": ["c05"] }
```

**Surveil 2 (sent only to the surveilling player):**
```json
{
  "event": "CHOICE_REQUIRED",
  "choice_id": "ch10",
  "choice_type": "SURVEIL",
  "timer_ms": 15000,
  "context": {
    "peek": [
      { "instance_id": "c44", "type": "PLAINS" },
      { "instance_id": "c09", "type": "MOUNTAIN" }
    ]
  }
}
```
```json
{
  "event": "CHOICE_RESPONSE",
  "choice_id": "ch10",
  "to_graveyard": ["c44"],
  "library_top_order": ["c09"]
}
```

**Flickered ETB fresh choices** reuse the same envelope with `choice_type` values such as `FLICKER_MOUNTAIN_TARGET`, `FLICKER_FOREST_RECLAIM`, `FLICKER_SWAMP_EXTRACT` — the controller picks targets fresh, per §3.5.

### 8.7 Server Snapshot Payload (`STATE_UPDATE`)
Sent to **each player individually** on **every** state change — a complete masked view, not a delta or broadcast. The example below is Player 1's view.

```json
{
  "event": "STATE_UPDATE",
  "snapshot_id": 118,
  "turn_number": 4,
  "active_player": 2,
  "phase": "MAIN",
  "main_phase": {
    "timer_remaining_ms": 41200,
    "paused": true
  },
  "priority": {
    "holder": 1,
    "window_open": true,
    "timer_remaining_ms": 7400
  },
  "you": {
    "id": 1,
    "afk_strikes": 0,
    "hand": [
      { "instance_id": "c17", "type": "PLAINS" },
      { "instance_id": "c02", "type": "ISLAND" },
      { "instance_id": "c33", "type": "FOREST" }
    ],
    "battlefield": [
      { "instance_id": "c41", "type": "MOUNTAIN" },
      { "instance_id": "c08", "type": "FOREST" }
    ],
    "main_pitch_used": false,
    "deploys_remaining": 0
  },
  "opponent": {
    "id": 2,
    "afk_strikes": 1,
    "hand_count": 4,
    "battlefield": [
      { "instance_id": "c25", "type": "ISLAND" }
    ],
    "main_pitch_used": true,
    "deploys_remaining": 1
  },
  "stack": [
    {
      "stack_id": "s1",
      "pusher": 2,
      "kind": "DEPLOY",
      "card": { "instance_id": "c29", "type": "MOUNTAIN" },
      "target_instance_id": "c41"
    }
  ],
  "shared": {
    "library_count": 31,
    "library_top_known": null,
    "graveyard": [
      { "instance_id": "c12", "type": "SWAMP" },
      { "instance_id": "c30", "type": "ISLAND" }
    ]
  },
  "winner_id": null
}
```
* `main_phase.timer_remaining_ms` exposes the active player's running 60-second main-phase clock (§3.2); `paused` is `true` whenever the stack or a choice prompt is live (as in this example — a priority window is open). `afk_strikes` counts are public to both players (3 = forfeit, §3.6).
* `library_top_known` becomes `{ "instance_id": "...", "type": "..." }` when a Memory-Lapsed land is publicly on top (§3.5) — public to both players by design.
* Note the v1 inconsistencies are gone: the event name is `STATE_UPDATE` everywhere (v1's heading said `GAME_STATE_UPDATE` over a `STATE_UPDATE` payload), and there is no `stack_empty` boolean — the `stack` array is the single source of truth.

### 8.8 Rejection Payload (`REJECT`)
The server answers every illegal intent with a machine-readable error — **never a silent drop**.

```json
{
  "event": "REJECT",
  "reply_to_seq": 45,
  "code": "E_PITCH_LIMIT",
  "message": "You have already made a main-phase pitch this turn."
}
```

| Code | Meaning |
| :--- | :--- |
| `E_BAD_SESSION` | Missing/invalid session token. |
| `E_NOT_YOUR_TURN` | Sorcery-speed action attempted off-turn. |
| `E_NOT_PRIORITY_HOLDER` | `PRIORITY_RESPONSE` from the wrong player or outside a window. |
| `E_WRONG_PHASE` | Action illegal in the current phase/stack state. |
| `E_TIMING` | Pitch speed violation (e.g., Island pitched at main phase, Swamp pitched reactively). |
| `E_DEPLOY_LIMIT` | Deploy count exhausted (no ramp available). |
| `E_PITCH_LIMIT` | Main-phase pitch already used this turn. |
| `E_UNKNOWN_INSTANCE` | `instance_id` not found in the required zone. |
| `E_ILLEGAL_TARGET` | Target violates card rules (e.g., Plains flickering itself). |
| `E_CHOICE_MISMATCH` | `CHOICE_RESPONSE` doesn't match the pending `choice_id`/schema. |
| `E_MALFORMED` | Unparseable or schema-invalid JSON. |

### 8.9 Reconnection (`REQUEST_RESYNC` → `FULL_RESYNC`)
A dropped client reconnects, re-authenticates with its session token, and requests a resync — honored **only** after the token is verified (Invariant 2).

```json
{ "event": "REQUEST_RESYNC", "session_token": "7f3c9a52-1e0b-4d8e-9d21-c0ffee00beef" }
```
`FULL_RESYNC` carries the **identical schema** as `STATE_UPDATE` (that is the payoff of full-snapshot design — resync is free), plus any pending `CHOICE_REQUIRED` prompt and live timer remainders, so the client can rebuild the entire scene from a single message.

### 8.10 Game End (`CONCEDE`, `GAME_OVER`)
```json
{ "event": "CONCEDE", "seq": 60 }
```
```json
{
  "event": "GAME_OVER",
  "winner_id": 1,
  "reason": "DOMAIN",
  "tiebreak": null,
  "rewards": { "coins": 50, "elo_delta": 12 }
}
```
`reason` ∈ `DOMAIN | MONO | DEPLOY_LOCK | TURN_CAP | CONCEDE | DISCONNECT_FORFEIT | AFK_FORFEIT | DRAW`. For `DEPLOY_LOCK` / `TURN_CAP` endings, `tiebreak` reports the scoring per §3.6 (distinct types, then total lands). `DISCONNECT_FORFEIT` covers both a blown 60s reconnect window and an exhausted 90s cumulative grace budget; `AFK_FORFEIT` is the 3-strike forfeit (§3.6). `DRAW` carries `winner_id: null` and zero ELO change.

---

## 9. Monetization, Progression & Scope Boundaries

### 9.1 In-Scope (Launch Features)
* Account Registration / Guest Login (SQLite back-end, argon2 password hashing).
* Casual Matchmaking Queue (1v1).
* ELO / MMR Rating Updates calculated per match win/loss (true draws: no change).
* Earned Coin Currency awarded post-match ($+50$ Win, $+20$ Loss).
* Cosmetic Shop: Unlocking custom Land Frames, Card Backs, and Board Playmats using earned coins.

### 9.2 Out-of-Scope (Saved for Post-Launch Updates)
* Custom Deckbuilder (Ratios are strictly fixed 10x5 for core competitive balance).
* Real-money transactions / Loot boxes.
* 2v2 / Free-For-All Multiplayer modes.

---

## 10. Master Implementation Lifecycle (Phase by Phase)

Each phase must be **tested**, **validated**, and **frozen** before moving to the next.

```
========================================================================================
                          MASTER IMPLEMENTATION PIPELINE
========================================================================================

                 [ PHASE 1: BARE-METAL C++ ENGINE ]
                 - `LandGameMatch.hpp` / `LandGameMatch.cpp`
                 - Count-Based Win Evaluator & Priority/Choice Timers
                 - Seeded Shuffle + Action Log (Deterministic Replay)
                 - CLI Stress Test Engine (10,000 Simulated Matches)

                                    │
                                    ▼

                 [ PHASE 2: WEBSOCKET & PROTOCOL LAYER ]
                 - `uWebSockets` Gateway on Port 8080 (Single Event Loop)
                 - JSON Serialization (`nlohmann/json`)
                 - Per-Player Masked Snapshots, Sessions & Resync Handler

                                    │
                                    ▼

                 [ PHASE 3: GODOT 4 TERMINAL CLIENT ]
                 - `WSClient.gd` Socket Manager
                 - `CardUI.tscn` Deploy/Pitch Drag Modes & Target Vectors
                 - Board State Renderer Engine & Tweens

                                    │
                                    ▼

                 [ PHASE 4: LOBBY, MATCHMAKER & SQLITE ]
                 - SQLite User Accounts (argon2) & ELO Tracking
                 - Automatic Matchmaker Queue Pairing
                 - Post-Match Reward Distribution

                                    │
                                    ▼

                 [ PHASE 5: DOCKER CONTAINER & VPS DEPLOYMENT ]
                 - Multistage Dockerfile Compilation
                 - Docker Compose: C++ Server + Caddy TLS Proxy
                 - Deployment to Hetzner VPS Node
========================================================================================
```

### Phase 1: The Bare-Metal Game Engine (Pure C++)
* **Goal:** A standalone C++ class (`LandGameMatch`) with zero network or UI code, fully deterministic (seeded shuffle + action log replay).
* **Modules:**
  * `DeckManager`: Generates and shuffles the 50-card shared library (10 of each land) with a recorded seed; assigns randomized stable `instance_id`s; manages the known-top slot for Memory Lapse.
  * `PlayerState`: Tracks Hand and Battlefield by instance ID; per-type battlefield counts; per-turn deploy/pitch budgets.
  * `SharedZones`: Shared library + shared graveyard (single public pile).
  * `ActionStack`: Fixed-grammar LIFO stack (1 sorcery-speed bottom item + reactive pitch chain) with `stack_id`s.
  * `WinEvaluator`: **Count-based** check — `counts[t] >= 5` for Mono, all `counts[t] > 0` (optionally via derived bitmask) for Domain — run on both players after every resolution step.
  * `MatchClock`: Turn counter, turn-50 cap, deck-out handling, tiebreaker scoring, timer-expiry events (auto-deploy / random choice), and per-player AFK strike accounting (3 = forfeit).
* **Test Criteria:** A local C++ CLI script runs automated stress tests (10,000 simulated matches between two mock bots) that must exercise **all v2 mechanics**: counter wars (multi-deep LIFO chains), Memory Lapse top-of-library placement and re-draw, Plains saves fizzling Mountain destroys, Forest ramp double-deploys (vanilla second land), Surveil graveyard sculpting, Swamp extraction, deck-out endings, deploy-lock endings, turn-cap tiebreakers, and both win conditions. The harness must also inject timer-expiry events to exercise the **random auto-deploy**, **random-legal-choice**, and **AFK 3-strike forfeit** code paths. Every match must terminate (boundedness proof by simulation) and be bit-for-bit replayable from seed + log — including timer events.
* **Balance Watchpoints (instrument the simulation to report on these):**
  * **Island** — likely the strongest card: counter *plus* effective card theft (the Memory-Lapsed land is usually drawn by the counterer).
  * **Forest** — graveyard theft of opponent-pitched cards may snowball.
  * **Mountain** — now both counterable **and** dodgeable (Plains save); watch for it becoming too weak.
  * **First-player compensation package** — the ruleset currently double-compensates Player 2 (§3.1: 6th starting card **and** P1's turn-1 draw skip). The simulation must measure P1 vs. P2 win rates under **three configurations**: **(a)** draw-skip only, **(b)** 6th card only, **(c)** both (the current default). The final compensation package is chosen from this data.

### Phase 2: The Network Transport Layer & Protocol Spec
* **Goal:** Wrap the engine in a C++ WebSocket server using the unified JSON API of §8 — single-threaded event loop, no locks, per-room event-loop timers.
* **Modules:**
  * `NetworkServer`: Opens port 8080 (plain WS behind the proxy) and handles socket lifecycle (Connect, Message, Disconnect).
  * `SessionManager`: Issues/validates session tokens; gates `REQUEST_RESYNC` behind re-auth; runs the disconnect-grace timers (60s per drop, 90s cumulative per player per match).
  * `RoomManager`: Maps rooms to active `LandGameMatch` instances; owns per-room priority/choice timers.
  * `SnapshotSerializer`: Builds the two per-player **masked** views for every state change (Invariant 4).
* **Test Criteria:** Connect using a raw WebSocket test client (e.g., Postman or Insomnia), authenticate, send `PLAY_CARD` intents (both `DEPLOY` and `PITCH`), receive correctly **masked** `STATE_UPDATE` snapshots on each; verify an illegal intent returns a `REJECT` with the right error code (never silence); verify drop-reconnect-resync restores a mid-counter-war game.

### Phase 3: The Godot 4 Terminal UI
* **Goal:** Build the visual layer that translates JSON snapshots into UI elements.
* **Modules:**
  * `WSClient.gd`: Wraps the `WebSocketPeer` connection, session token storage, and message handling.
  * `CardUI.tscn`: Reusable card node handling mouse hover, selection, deploy-vs-pitch drag modes, and target lines.
  * `BoardStateRenderer`: Reads full snapshots and diffs them client-side to trigger visual card moves and Godot Tweens (shared graveyard, known library-top, stack zone, private peek overlays).
* **Test Criteria:** Two Godot client windows running on `localhost` can connect to the C++ server and play a complete game start-to-finish visually, including a counter war, a Plains save, a Surveil peek, and a reconnect mid-game.

### Phase 4: Lobby, Matchmaking & SQLite Persistence
* **Goal:** Handle user identity, queuing, and persistent stats.
* **Modules:**
  * `AuthManager`: SQLite lookup for user handles & **argon2**-hashed passwords; guest account issuance.
  * `MatchmakerQueue`: Pairs two waiting authenticated sockets and instantiates a `GameRoom`.
  * `ProgressionStore`: Grants post-match coins ($+50$ Win, $+20$ Loss) and updates ELO ratings upon match finish (no ELO change on true draws).
* **Test Criteria:** A player can log in, click "Find Match", get matched automatically, play, and view their updated coin balance and ELO after the match; a disconnect-forfeit correctly awards the win and rewards to the remaining player.

### Phase 5: Containerization & Hetzner Deployment
* **Goal:** Production deployment.
* **Modules:**
  * Multistage `Dockerfile` (compiles C++ server binary with `g++`, bundling SQLite).
  * `docker-compose.yml`: **Caddy (or nginx) reverse proxy terminating TLS/WSS** (automatic Let's Encrypt) in front of the plain-WS C++ container — no TLS code inside the game server.
* **Test Criteria:** Remote deployment on Hetzner cluster allowing external web/desktop clients to join and play over public WSS endpoints, with reconnect/resync verified across the proxy.
