# Line Move & Passive Identities — design bank

> **What this file is.** How to give a **creature line** (not each species) a distinct combat
> identity: a signature passive + a named attack/defense move track. The Ransomware, Phishing,
> Trojan and Worm lines are built this way — their moves, passives, and stacking rules live in
> `src/core/content/content_moves.cpp` / `combat.cpp`, not here. This file is the **design bank**
> for lines not built yet: the grounding a new line's identity must slot into, plus the open items
> for whoever picks the next one up.

---

## 0. How a new line slots into the existing engine

Everything below is written against the **actual current schema**, not a hypothetical one — key
facts so a new line's identity stays buildable:

- **Moves are a single shared pool, not per-line by default.** `MoveDef` (`src/core/content/
  defs.h`) has `id`, `displayName`, `kind` (Attack/Defend), `power`, `channelTurns`, `effect` text,
  and `minStage` (the evolution stage a move unlocks at). `CreatureDef.line` + `MoveDef.line` +
  `moveAllowedForLine()` gate a move to one line while leaving the generic pool open to everyone;
  `quick_jab` is the line-agnostic default every pet starts with.
- **A line's signature ability is a passive hook**, not a stat field — today the per-line passives
  are Ransomware's Ransom Note, Phishing's Feed-Frenzy + Perfect Bite, Trojan's Execution-Override,
  and Worm's Shared Resources, each a bespoke check in `combat.cpp`/`game_combat.cpp`. Ransom Note,
  Execution-Override and Shared Resources gate explicitly on `CreatureDef.line`; Perfect Bite
  instead gates on generic combat state (`Combatant::shieldHp > 0`) that only Phishing content
  happens to populate — either shape is fine, pick whichever the passive naturally keys off. A new
  line's passive needs the same kind of gated hook, not a generic field on `Combatant`. Per-line
  passive TUNING constants
  (stage-scaled chances/percentages, floors, caps) live in `src/core/content/content_passives.h`,
  grouped by line, not in `tunables.h` — they only ever move together with that line's move
  magnitudes in the matching `content_moves.cpp` rows.
- **The persistent per-pet stat block** a line's mechanics can key off: `Game::statPoints_[4]`
  (`src/core/app/game.h`) — 0 Power · 1 Defense · 2 Speed · 3 Max-Health, applied in
  `Game::buildPlayerCombatant()`. This is the real 4-stat vocabulary; there is no other RPG stat
  block.
- **Combat state is per-fight and never persists** (wiped every `Combat::begin()`): Health, guard,
  shield pools, stat siphons, stacking buffs — anything transient a new line invents must follow
  this rule, never touch `statPoints_`.
- **Mods/branch passives are read once, at fight start.** A new line passive that reacts to the
  opponent should evaluate once when `Combat::begin()` builds both `Combatant`s, not re-roll every
  turn.
- **Move-naming has no locked convention** (`src/core/content/CREATURE_NAMING.md` explicitly
  excludes move names). The seeded roster is plain two-word Title Case (`Packet Storm`, `Buffer
  Overflow`); Phishing's names (`Smish-Hook`, `Spear-Strike`) break that on purpose — a stylistic
  branch point, not a mistake to fix.
- **A per-line default-move override doesn't exist yet** — today's default move is the single
  global `quick_jab` for every pet regardless of line. A line wanting its own default needs a
  lookup keyed by `CreatureDef.line`, not a global constant.

---

## Open items for whoever picks this up next

- **A new line's engine hooks are additive, not free** — a shield/absorb pool, a stat-siphon list,
  or a held-damage pool (Ransomware's Ransom Note) each need their own `Combatant` field or hook
  the first time a line needs them; check `combat.h`/`combat.cpp` for what already exists before
  assuming a new primitive is required.
- **A line may add BODIES to the fight, but not ACTORS.** The Worm's replicas
  (`Combatant::wormReplicas`) are the precedent and the ceiling: they are drawn separately, they
  soak attacks aimed at them and they scale their parent's damage — and they never take a turn,
  never roll a move and never enter the speed scheduler. Turn order is the thing a duel's two
  devices reconstruct independently, so a third initiative is a third thing for their RNG streams
  to disagree about. Anything a new line wants on the board should be state the parent owns.
- **A passive that can fire in a DUEL must be decided per TURN, not per incoming hit.** Both
  devices resolve the same seeded fight independently (`core/model/pvp_battle.h`), so anything
  whose roll timing depends on how many actions the opponent's speed buys is a desync risk and a
  fairness problem — Ransom Note's window (`Combat::ransomArmRolls`, rolled once at the ransomer's
  turn-start) is the shape to copy.
- **Budget a passive against how few chances a real fight gives it.** Fights are short and a
  healthy pet often out-speeds its opponent — a Process pet against the softest practice dummy
  takes about two hits in six turns. A trigger gated on "something happened during this one
  turn" therefore misses almost every time it rolls true, and reads as broken. Ransom Note's
  window stays open until it catches a hit for exactly this reason, and
  `test_ransom_note_shows_up_in_pve` pins the resulting rate at that worst case. Give a new
  passive the same measurement before trusting its percentages.
- **Balance numbers for any new line are placeholders until measured** — trigger chances, stack
  caps, siphon percentages, shield multipliers all need a calibration pass against a fresh-level
  fight the way the shipped lines were tuned, not just an internally-consistent ramp.
- **Iconic per-line move VFX** (on the board) is a natural pairing once a
  new line's identity mechanic exists to hang the effect off of — a stack counter or passive
  trigger is usually the right visual hook.
