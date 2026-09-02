// defs.h — content definitions, addressed by stable string id.
//
// Content is DATA, not hardcoded switch logic: creatures, egg lines and items
// are looked up from the ContentRegistry by id. Even the built-in Paypup +
// Ransomware line go through here, so adding content later (new lines, items,
// modes) is more data — supplied today by the embedded source, tomorrow by an
// SD-backed source implementing the same interface.
#pragma once

#include <cstdint>
#include <cstring>

#include "tunables.h"  // kMaxMoveSlots (CreatureDef::slotKinds)

namespace mal {

// The four-stage lifecycle.
enum class Stage { BootSector, Process, Script, Daemon };

// Attack/Defend move typing. Free-standing rather than
// nested in MoveDef (below) because CreatureDef, declared next, needs it for its
// per-slot layout — MoveDef itself aliases this as MoveDef::Kind so every
// existing `MoveDef::Kind::Attack`-style call site keeps compiling unchanged.
enum class MoveKind { Attack, Defend };

// How a creature gets around under its own power, which is what the idle habitat's resting
// motion reads (core/model/idle_wander.h). MOVEMENT only — which sprite row a mover
// animates on is the sheet's business. It sets both the wander's pace (idle_wander.cpp's
// kPaces) and whether the pose takes the shelf BOB, the 2px lift on alternate beats that is
// the only motion a single-frame sprite has:
//   Walk   — on the floor, and LIFTS. The bob is its breathing.
//   Ground — on the floor and STAYS there. For anything whose read depends on never
//            breaking contact (the worm line), and for anything with an authored idle,
//            where a second rise on a different beat reads as a bounce.
//   Fly    — holds an altitude clear of the shelf.
//   Swim   — drifts on both axes; no bob, a lift over a drift reading as jitter.
//   Static — goes NOWHERE, having no locomotion at all, which is what an egg is. It still
//            lifts: the bob is the breath rather than the travel, and a shell that neither
//            moves nor breathes reads as scenery.
//
// An egg declares one like anything else and the habitat honours it — most are Static, and
// one genuinely adrift in water says Swim.
enum class Locomotion : uint8_t { Walk, Fly, Swim, Ground, Static };

inline const char* stageName(Stage s) {
    switch (s) {
        case Stage::BootSector: return "Boot Sector";
        case Stage::Process: return "Process";
        case Stage::Script: return "Script";
        case Stage::Daemon: return "Daemon";
    }
    return "?";
}
inline int stageIndex(Stage s) { return static_cast<int>(s); }

// The care "signal" that dominates a stage decides which successor a creature
// evolves into ("chosen by dominant stat"). Balanced
// is the default / tie outcome (and the highest index, so it sorts last). The
// per-signal creature ids live in the routing tables below — this enum is only
// the key; which interactions feed each signal is Game's concern (game.cpp).
enum class DominantSignal { Feeding, Play, Maintenance, Training, Balanced };
inline constexpr int kNumDominantSignals = 5;

inline const char* dominantSignalName(DominantSignal s) {
    switch (s) {
        case DominantSignal::Feeding: return "Feeding";
        case DominantSignal::Play: return "Play";
        case DominantSignal::Maintenance: return "Maintenance";
        case DominantSignal::Training: return "Training";
        case DominantSignal::Balanced: return "Balanced";
    }
    return "?";
}

// How many named animation loops one creature may declare. A structural bound on
// CreatureDef::clips, not a balance number — raise it when a creature genuinely
// needs a fifth loop, which costs every row the slot whether it fills it or not.
constexpr int kMaxAnimClips = 4;

// One named animation loop over a single row of a creature's sprite sheet. A sheet
// can stack several rows of equal-width frames (sprite.h); a clip says which row
// plays, how many of that row's columns it uses, and how long each frame holds.
// Clips are declared on the creature's own row (CreatureDef::clips below), so a
// sheet and the loops over it are read in one place — a sprite name names ART, and
// is not also a key into a table somewhere else that can silently fail to match.
struct AnimClip {
    const char* name = nullptr;  // "idle", "attack", ...; nullptr = unused slot
    int row = 0;                 // sheet row this clip plays from
    int frames = 0;              // columns 0..frames-1 of that row
    int holdBeats = 1;           // heartbeats (kHeartbeatMs each) each frame holds

    // The frame to draw on a given heartbeat: each frame holds `holdBeats` beats,
    // wrapping every frames*holdBeats. Raise holdBeats to slow a clip down
    // (holdBeats=2 halves its speed) without redrawing or re-counting frames.
    int frameAt(int beat) const {
        if (frames <= 0) return 0;
        const int hold = holdBeats > 0 ? holdBeats : 1;
        return (beat / hold) % frames;
    }
};

struct CreatureDef {
    const char* id;          // stable id, e.g. "paypup"
    const char* displayName; // e.g. "Paypup"
    Stage stage;
    const char* spriteName;  // asset id resolved via AssetSource, e.g. "SPR_PET_PAYPUP"
    // Next-stage evolution hook: the creature this one evolves into, or nullptr
    // if no LINEAR successor is defined. Linear hops use this; a BRANCH node leaves
    // it null and defines the two successors below instead. Data-driven so wiring a
    // chain (or a branch) later is more data here, not new branching logic.
    const char* evolvesToId = nullptr;
    // Good/Bad evolution branch. When BOTH are set this is
    // a branch node: the care budget at evolution time picks the successor — 0-2
    // mistakes -> Good, 3-4 -> Bad (5/5 routes to Critical System Failure, never
    // here). Leave null for a linear hop (evolvesToId above).
    const char* evolvesToGoodId = nullptr;  // care 0-2: durable Good successor
    const char* evolvesToBadId = nullptr;   // care 3-4: glass-cannon Bad successor
    // Combat-branch multipliers the engine reads to realise the branch:
    // Bad-branch petware leans aggressive (powerMultPct > 100) but takes more Frag
    // on a loss (fragMultPct > 100); Good-branch is the durable inverse. Neutral =
    // 100 for every linear/non-branch creature.
    int powerMultPct = 100;   // attack-power lean in combat
    int fragMultPct = 100;    // loss-Frag multiplier (hook)
    // Creature LINE (see LINE_MOVE_IDENTITIES.md). Every creature in
    // one evolution family shares a line id (e.g. "ransomware"); it gates which
    // LINE-specific moves this pet can learn/equip (moveAllowedForLine). nullptr =
    // no line (e.g. a bare enemy frame) — such a pet can still learn GENERIC moves.
    const char* line = nullptr;
    // Flavour text — the GAME owns its own descriptions (single source of truth), the
    // same way ItemDef carries `effect`. `hint` is the one-line snarky read (shown on
    // STAT + the 'Pedia); `context` is the real infosec reference behind the pun ('Pedia).
    // tools/gen_pedia_data.py reads these straight off the row — do NOT re-key copy in
    // the generator. nullptr = none (a bare enemy frame with no lore).
    const char* hint = nullptr;
    const char* context = nullptr;
    // Per-slot Attack/Defend typing: each of THIS creature's kMaxMoveSlots slots is locked
    // to one MoveKind. Game::stampSlotKinds() reads slotKinds[i] the instant slot i first
    // unlocks for a pet and never rewrites it, so whichever creature occupies the evolution
    // path at that moment decides the slot for the rest of the raise — evolving Good vs Bad
    // at the Daemon hop can leave slot 3 typed differently per branch. The type-agnostic
    // default move (Quick Jab) ignores this: it fills any empty slot and is never itself
    // equippable.
    //
    // The layout: Process pets lean slot 0/1 Attack (Paypup varies at slot 1 for flavour),
    // Script adds a Defend at slot 2, and the Daemon branch splits at slot 3 to mirror the
    // power/Frag lean — Good (durable) -> Defend, Bad (glass cannon) -> Attack.
    MoveKind slotKinds[kMaxMoveSlots] = {MoveKind::Attack, MoveKind::Attack,
                                         MoveKind::Attack, MoveKind::Attack};
    // Cross-line Trojan infiltration hook. When a pet with this set evolves, a
    // kTrojanDivertPct chance (Game::fireEvolution) diverts it into this Trojan
    // creature instead of its normal successor — the pet keeps its old line's colour
    // but is now a Trojan (losing its line moves for the Trojan kit; see
    // Game::completeEvolution). The first divert unlocks the family. nullptr = no
    // divert path (every non-infiltratable creature). The row is the only gate: the
    // divert fires at whatever boundary the creature carrying it evolves across, so
    // Phishlet trades its Script for one and Rootgrub trades its Daemon.
    const char* evolvesToTrojanId = nullptr;
    // The Bad-care half of a divert that lands on a BRANCH — set only alongside the
    // field above, and only where the divert replaces a Script->Daemon hop that had a
    // care branch of its own to replace. Leaving it null makes the divert unconditional
    // (Phishlet's is), which is right when the boundary being replaced had no branch
    // either. Resolved from the same CareBranch as evolvesToGoodId/BadId, so a diverted
    // pet keeps the outcome the raise earned and loses only its line.
    const char* evolvesToTrojanBadId = nullptr;
    // How this creature moves around its habitat at rest. EVERY row states it, even
    // the walkers: how a creature moves is a fact about that creature, and it belongs
    // where the rest of it is read rather than being inferred from an absence. It
    // also changes WITHIN a line — a Tadpoll swims, the Croaken it becomes does not —
    // so there is no line-level answer to inherit. The default is a floor for a row
    // that predates a new mover kind, not a licence to leave it off.
    Locomotion locomotion = Locomotion::Walk;
    // Named animation loops over this creature's OWN sheet (spriteName above), in
    // whatever order reads best — `clip()` finds them by name, nothing by position.
    // An unfilled slot has a null name. A creature that declares no clip at all
    // falls back to sprite.h's idleFrame() breathe/blink heuristic on row 0, which
    // is what a single-frame placeholder wants; declaring one is how a multi-row
    // sheet becomes reachable. Sheet layout for a new creature: see
    // src/core/content/creatures/CREATURE_CONTENT_STANDARD.md.
    AnimClip clips[kMaxAnimClips] = {};
    // The achievement that EARNS this row a place in its line's random hatch pool
    // (content_achievements.h), or nullptr for a row that is always in it —
    // Game::hatchProcessUnlocked reads this, the same way EggLineDef::gatedBy gates a
    // whole line one tier up. Only a Process row is ever drawn for, so a gate on any
    // other stage is inert.
    const char* gatedBy = nullptr;

    // This creature's clip of the given name, or nullptr if it declares none.
    const AnimClip* clip(const char* clipName) const {
        if (!clipName) return nullptr;
        for (const AnimClip& c : clips)
            if (c.name && std::strcmp(c.name, clipName) == 0) return &c;
        return nullptr;
    }
};

// A combat passive family, carried by a LINE rather than by a creature. One bit per
// passive the turn engine implements; a line's row names the ones it has, and Combat
// tests the bit. Adding a passive is one flag here, one row edit, and one hook in
// combat.cpp — never a line-id string literal in the turn engine.
enum class LinePassive : uint8_t {
    RansomNote   = 1 << 0,  // a stage-scaled roll arms a lock (Combat::ransomArmRolls)
    Replication  = 1 << 1,  // replica spawning + Shared Resources (Combat::syncWormSpeed)
    ExecOverride = 1 << 2,  // armed traps hijack the turn (Combat::execOverrideChance)
};

// A set of the above, held as the underlying type. Rows build one with linePassives()
// — one spelling whether the line carries one passive or several — and the engine reads
// it back with hasLinePassive(). An empty set is a line with no passive of its own.
using LinePassives = uint8_t;
template <class... Ps>
constexpr LinePassives linePassives(Ps... ps) {
    return static_cast<LinePassives>((0 | ... | static_cast<uint8_t>(ps)));
}
constexpr bool hasLinePassive(LinePassives set, LinePassive p) {
    return (set & static_cast<uint8_t>(p)) != 0;
}

// One creature FAMILY — the rows of a single evolution line, which is what
// CreatureDef::line names on every one of them. Each family is defined together in
// src/core/content/creatures/<id>/line.h and listed once in creature_lines.h; the
// registry reads creatures family-by-family rather than as one flat roster, so
// adding a family is a folder plus one line, with no count to bump anywhere.
struct CreatureLine {
    const char* id;           // matches CreatureDef::line on every row it holds
    const CreatureDef* rows;
    int count;
    // Which combat passives every row on this line carries. The turn engine asks a
    // combatant for a FLAG and never for a line's name, so moving a passive to another
    // line — or landing a new line that carries one — is an edit here.
    LinePassives passives = 0;
    // The achievement earned the first time a pet arrives on this line, or nullptr for
    // a line whose arrival is not itself recognised. Fired at the one evolution seam
    // that changes a pet's line (Game::completeEvolution).
    const char* raisedAchievement = nullptr;
    // Does this family repaint itself in another line's colours? A property of the
    // FAMILY rather than of any row, and declared rather than inferred: FX_CAMO
    // (core/render/camo.h) is driven in combat by what a fighter's live cast is, which
    // says nothing to a screen with no fight on it. A caller outside combat — the
    // CHROMATOPHORE cabinet choosing whether the pet standing there can plausibly be
    // its subject — asks this instead of asking a creature's id.
    bool wearsBorrowedColours = false;
};

// Which minigame an egg line's hatch runs. One entry per interaction shape; the
// applier is Game::startHatchGame (game_lifecycle.cpp), which is the map of every
// route out of layEgg(). A line that wants a new shape adds a kind here plus one
// case there — never an `if (lineId == "...")` at a call site.
enum class HatchGame : uint8_t {
    Decrypt,   // DISK DECRYPTION: the payload's key is broken on a five-colour code
               // board, right away (Game::startDecryption).
    Clutch,    // Clutch Pick: the egg is laid among identical decoys and found by
               // halving the clutch, right away (Game::startEggPick).
    Isolation, // Isolation Protocol: the worm inside the shell is turned loose in a
               // quarantine buffer and eats the incubation clock a minute at a time,
               // right away (Game::startIsolation).
    Chroma,    // CHROMATOPHORE: the bell rehearses wearing other colours, one skin per
               // button, against a sweep on a shrinking clock, right away
               // (Game::startChroma).
};

struct EggLineDef {
    const char* id;            // e.g. "ransomware"
    const char* displayName;   // e.g. "Ransomware"
    const char* eggCreatureId; // the Boot-Sector creature, e.g. "cryptoshell"
    HatchGame hatchGame = HatchGame::Decrypt;
    // The achievement that EARNS this line (content_achievements.h), or nullptr for a
    // line that is simply given. Game::eggLineUnlocked hides a gated line from
    // line-select until its row's achievement is held — so "this line is earned" is
    // something the row states rather than something the engine knows about its id.
    const char* gatedBy = nullptr;
};

// Evolution routing ----------------------------------
// Where a creature evolves to is on its own row (CreatureDef::evolvesToId for a
// linear hop, evolvesToGoodId/BadId for a care branch, evolvesToTrojanId for the
// cross-line divert). The one shape a row cannot express is a draw between SEVERAL
// Daemons on one branch, which is what the pool below adds.

// One weighted candidate in a Daemon draw.
struct DaemonPoolWeight {
    const char* daemonId;
    int weight;                                // relative draw weight (>=1)
};
inline constexpr int kMaxDaemonPoolEntries = 4;

// A Script->Daemon hop: a weighted pool per care-branch. `badBranch` selects
// which branch (Good vs Bad) the row serves; the evolve logic reads the branch
// off the existing care-mistake gate. Single-entry pools today (the weight is
// scaffolding for the future multi-Daemon draw).
struct DaemonPoolDef {
    const char* fromId;                        // the Script creature evolving
    bool badBranch;                            // false = Good pool, true = Bad pool
    DaemonPoolWeight entries[kMaxDaemonPoolEntries];
    int count;                                 // populated entries in `entries`
};

// The most on-Use effects any single item carries (R007_B33R = Hunger+Happy+Frag).
// Bump if a future item needs more; unused slots stay Kind::None.
inline constexpr int kMaxItemEffects = 4;

// One structured lever an item pulls on the PET when USED in the ITEMS flow — the
// data-driven analog of ModEffect (defs.h below): an item's on-pet effects are a
// scannable list of kind+magnitude on its row, with magnitudes HERE (never in tunables;
// src/core/content/CONTENT_STANDARD.md). Applied by Game::applyItemEffects; order-independent, so a
// row can carry several. Combat Health / shop price / warp /
// pre-encounter XP are NOT here — they're pulled by other systems (see ItemDef).
struct ItemEffect {
    enum class Kind : uint8_t {
        None = 0,             // empty slot / inert
        Hunger,               // +magnitude Hunger (may be NEGATIVE — Null Noodles'
                              // "negative eating"; the exception to "+Hunger fills")
        HungerStacking,       // +magnitude Hunger for EACH stacking food already eaten
                              // since the pet last lost a Hunger point to decay, the
                              // first of a sitting counting as one (Polltatoes: eat
                              // four in a row and the fourth is worth four). The run
                              // is shared by every stacking food and broken by the
                              // next decay tick (Game::tickHungerAndAwardXp), so it
                              // rewards emptying a bag in one go, not hoarding.
        Happy,                // +magnitude Happiness (flat add)
        HappyToward50,        // pull Happiness TOWARD 50% by magnitude, no overshoot
                              // (Null Noodles' "empty feeling"; numbs OR lifts)
        Frag,                 // +magnitude Fragmentation (negative de-frags: Null Noodles;
                              // positive fragments: R007_B33R)
        RemoveCareMistakeOnce,// remove magnitude care mistakes, ONCE PER LIFETIME
                              // (Yubi-Cookie, "Max 1 per lifecycle")
        ClearMistakeShieldOnce,// arm the next-mistake SHIELD, ONCE PER LIFETIME
                              //   (Restore Point, save v21)
        ForceTrojanDivert,    // arm a GUARANTEED Trojan divert at the pet's next
                              // Process->Script evolution, replacing the normal
                              // kTrojanDivertPct roll (Ambig-USB, save v28). A no-op at
                              // evolution time if the pet has no evolvesToTrojanId.
        // The BRANCH OVERRIDE pair (Signed-USB / Bad-USB, save v60): point the pet's next
        // BRANCHING evolution at the Good or the Bad successor whatever its care record
        // says, instead of reading the branch off the care budget (Game::effectiveCareBranch).
        // A Kind per direction rather than one Kind with a magnitude, for the same reason
        // the four stat points below are four Kinds: the DIRECTION is the effect, and a
        // magnitude that secretly means an enum is the shape this vocabulary exists to
        // avoid. They share ONE slot on the pet — arming either replaces the other, so the
        // most recently plugged in is the one that fires — and are consumed at the
        // evolution they steer. Neither rescues a DYING pet: 5/5 is a death, not a branch.
        ForceEvolveBranchGood,
        ForceEvolveBranchBad,
        ArmEvolveSoak,        // Sandbox/Hypervisor-USB: soak this stage. While armed, the
                              // stage's evolution dwell runs magnitude times LONGER and
                              // every XP award pays magnitude times MORE, so the trade is
                              // the same pet arriving later and further along. Plugged in
                              // at Process only, consumed at the evolution it stretched,
                              // and it holds the port shut while it runs — no other USB
                              // item is usable until it is spent (itemIsUsb below).
        ArmCombatShieldBuff,  // arm a timed combat shield for magnitude MINUTES
                              // (Backup Drive, save v30): while armed, the next incoming
                              // hit in combat is negated (like the RAID Mirror mod) and
                              // the pet is healed to at least half max Health, then the
                              // shield is consumed early. Re-arming refreshes the timer.
        ArmDeepWebDepthMultiplier, // Deep-Learning Module/Core: while armed, each DeepWeb
                              // Dive win advances the dive's depth by magnitude steps
                              // instead of 1, so a pet strong enough to blitz the early
                              // depths catches back up to a real fight faster. Doesn't
                              // stack — using another one just overwrites the
                              // multiplier (Game::deepWebDepthMultiplier_).
        SetDeepWebStartDepth, // Backdoor/Rootkit/Kernel Bell: arms the NEXT DeepWeb
                              // Dive to start at depth = magnitude instead of 0
                              // (Game::pendingDeepWebStartDepth_), consumed the moment
                              // that dive starts.
        SetDeepWebStartDepthToBest, // Zero-Day Bell: arms the next DeepWeb Dive to
                              // start at THIS PET's own best-ever depth
                              // (Game::bestDeepWebDepth_) rather than a fixed number.
        BandwidthRegenBonusMin,// Tiramisudo: shave magnitude MINUTES off THIS PET's
                              // Bandwidth regen interval, permanently and once per pet
                              // (Game::bandwidthRegenBonusMin_, save v50). The first
                              // helping is the upgrade; every one after it is just the
                              // food, because the pet already has root. Per-pet and
                              // frozen with the pet, so an ARCH Store/Deploy keeps it —
                              // it is a property of the creature, not of the rig.
                              // Floored at kBandwidthRegenMinutesFloor, so no stack of
                              // helpings makes the pool refill instantly.
        Bandwidth,            // +magnitude to the LIVE Bandwidth pool, capped at
                              // Game::bandwidthMax(). The item-side twin of the Rig
                              // Shop's GrantBandwidth: an ordinary top-up anyone can
                              // eat, as against a level that raises the ceiling.
        // The four OFF-LEVEL stat points, one Kind per combat stat because the stat IS
        // the effect — "+1 Power for life" and "+1 Defence for life" are two levers, not
        // one lever with an argument, and a Kind per stat is what keeps the stat out of
        // `magnitude` (which stays what it is everywhere else: how much). Each grants
        // magnitude points into PetUpgrades::statBonus (core/model/pet_upgrades.h) the
        // FIRST time this pet eats one and latches out after that, exactly as
        // BandwidthRegenBonusMin does. The points land in combat alongside the earned
        // ones but count toward neither the level nor the Rollback picker, so a Rollback
        // can never shed what a dish granted.
        StatPointPower,
        StatPointDefense,
        StatPointSpeed,
        StatPointHealth,
        XpRateBonusPct,       // +magnitude PERCENT on every XP award this pet takes
                              // (PetUpgrades::xpRatePct, Game::addCombatXp), permanently
                              // and once per pet. The per-pet twin of the Rig Shop's
                              // player-level Passive XP Farming row: that one pays XP for
                              // hunger decay, this one raises what every source pays.
        ClearReplicationGhost, // Unlinkguine: clear a Replication Ghost
                              // (PetModel::ghost_), the phantom process a defrag that
                              // failed on a Critical disk leaves behind. Magnitude-free
                              // — a ghost is a flag, not a quantity. An AV scan clears
                              // one too, so the pair is the same shape as the defrag
                              // variants: the scan is free but can fail and costs a care
                              // mistake when it does, this is an item that cannot. A
                              // no-op on a pet with no ghost, which is what keeps the
                              // snack an ordinary food the rest of the time.
    };
    Kind kind = Kind::None;
    int magnitude = 0;
};

// Rarity → the drop weight an item falls back to when its own row doesn't name one.
// Rarity says how much VALUE an item hands the player, so it makes a sane default
// ordering for scarcity — but only a default: a row that wants to be commoner or
// scarcer than its tier-mates sets ItemDef::dropWeight, and a POOL that wants this
// one item scarcer only THERE sets LootEntry::weight. Sibling to kModRarityWeight
// (tunables.h), which does the same job for the mod tables.
inline constexpr int kItemRarityDropWeight[4] = {50, 30, 15, 5};  // Common..Epic

// One row of a loot/reward pool: the item, and optionally the weight it draws at IN
// THIS POOL. `weight` 0 (the default every plain row carries) defers to the item's own
// ItemDef::dropWeight, which in turn defers to its rarity — so a pool is written as a
// bare list of ids until some entry actually needs to differ, and only the exception
// carries a number. Resolved in one place, lootEntryWeight() below.
//
// Consumers: Game::drawCacheItem (a container's payout) and Game::grantLootReward (the
// walk's loot-cache event), which share the same weighted walk over these rows.
struct LootEntry {
    const char* id;
    int weight = 0;   // 0 = use the item's own dropWeight
};

// A perishable item's decay: what it turns into, and how likely that is each time the
// pantry is disturbed. Spoilage has NO clock of its own — it rides a beat already
// happening (a bite of food), the way ItemEffect::Kind::HungerStacking rides the Hunger
// tick. Per-stack expiry would make an Inventory stack a list of timestamped objects
// rather than a count, which is the one shape this must not take.
//
// Only read when `into` is set. Game::rollPantrySpoilage (game_items.cpp) rolls once per
// held stack and converts a single unit, so a full larder decays at the same rate as one
// item and nothing turns while the player isn't playing. Give the `into` row its own
// SpoilDef only if the second stage should also rot.
struct SpoilDef {
    const char* into = nullptr;  // item id this becomes (nullptr = imperishable)
    int pct = 0;                 // percent chance per disturbance, per held stack
};

// A container item's reward draw — what Open pays out, and how the item is come by.
// Nested on the row (the same shape CrewDef's exploit uses) rather than kept in a
// parallel id-keyed table, so ADDING A CACHE IS ONE ITEM ROW: its purse, its pool, how
// often the walk drops it, and whether it also rolls a mod all sit next to the name.
// Only read when `use == ItemDef::Use::OpenContainer`; every other row leaves it inert.
//
// Consumers: Game::grantCacheReward (the payout), Game::resolveCacheEvent (the walk's
// weighted find roll over every row with findWeight > 0), and the native earn-path gate
// (an item reachable only as a cache draw is earnable because some pool lists it).
struct CacheDef {
    int bits = 0;                       // Bits paid on Open
    int draws = 0;                      // draws taken from `pool`
    int drawChancePct = 100;            // % chance EACH draw actually yields (100 = always)
    const LootEntry* pool = nullptr;    // the weighted item pool draws come from
    int poolSize = 0;
    // Relative weight in the walk's cache-find roll. 0 = never found on a walk: the
    // cache exists only where something hands it over (an achievement reward, a legacy
    // save), which is what keeps a granted prize off the ambient loot table.
    int findWeight = 0;
    // % chance an Open ALSO yields a permanent mod, rarity-weighted across every tier.
    // A second, non-boss mod source — 0 on the rows that aren't one.
    int modChancePct = 0;
};

struct ItemDef {
    enum class Type { Food, Buff, Quest };
    enum class Rarity { Common, Uncommon, Rare, Epic };
    // Where Use does something. Outside its context the detail screen disables
    // Use and states where the item applies.
    enum class Context { Anytime, LockoutOnly, PreEncounter };
    // Explore-use warp key: a consumable spent
    // DURING the walk (via the walk's B warp picker, not the ITEMS use path) to jump
    // straight to a target event — Shop = the nearest storefront (Access Token),
    // SafeRest = a safe rest event (Safe-Mode Key). None = not a warp key.
    enum class WalkWarp { None, Shop, SafeRest };
    // What B (Use) DOES in the ITEMS flow, when not gated. Collapses the old
    // openable/rollback/decryptEgg bool trio into one mutually-exclusive choice so an
    // item can never be two Use-actions at once. Consume = the normal feed/buff/quest
    // path (drives ItemEffect application); the others hand off to their own flow.
    enum class Use {
        Consume,        // feed / buff / lockout-resolve — the default path
        OpenContainer,  // Sealed Cache: decrypt for a reward draw (VAULT)
        Rollback,       // open the stat picker to shed a level
        DecryptEgg,     // cut a flat bite off a Boot-Sector egg's incubation (Boot Accelerator)
        PlayCryptogram, // cash in for a DECRYPTOGRAM board (VAULT, game_cryptogram.cpp)
    };
    // The ITEMS type-picker's axis (items_screen.h) — one notch finer than `type`,
    // which only knows Food/Buff/Quest. `Derive` (the default, and what every row
    // that doesn't say otherwise carries) resolves off the row itself: a Food row is
    // Food, a Buff row is Buffs, a Quest row is Tools. A Quest row that is really a
    // KEY or token names Keys explicitly — nothing else on ItemDef separates a
    // Decryption Key from a Defrag Tool. Read it through itemCategory(), never raw.
    enum class Category { Derive, Food, Buffs, Keys, Tools };

    const char* id;
    const char* displayName;
    Type type;
    Rarity rarity;
    // Concrete effect text, not a hint. A TEMPLATE over this row's own numbers:
    // `{hunger}` / `{heal}` / `{depth}` and friends are substituted from the fields
    // below, so retuning a magnitude retunes the sentence (core/content/effect_text.h,
    // which also derives the numbers-only stat line every screen draws under it).
    const char* effect;
    Context context;
    // --- On-Use pet effects: the complete set of levers this item pulls on the pet
    //     through the ITEMS Use flow. One place; magnitudes live here, not in tunables.
    //     Applied by Game::applyItemEffects. Unused slots stay Kind::None. ------------
    ItemEffect effects[kMaxItemEffects] = {};
    // --- Hand-offs to OTHER systems (applied by the named system, not applyItemEffects;
    //     "if it acts on something else, go to the something else"). All defaulted so a
    //     plain feed/buff row lists only its effects[] above. ---------------------------
    int combatHeal = 0;                  // combat Exploit picker → transient Health
                                         //   (game_combat/combat; >0 also = combat-usable)
    int preEncounterXp = 0;              // Sinkhole Trap bypass → XP lump (game_combat)
    // This item's own reference price: the 'Pedia's listed price, and the generic
    // "is this shop-sellable at all" signal (0 = not sold anywhere). The price a
    // player actually PAYS at a given storefront is that area's own
    // AreaShopDef::price/price2 (src/core/content/areas/) — deliberately separate,
    // so one shop can charge above/below this item's listed price.
    int bitsPrice = 0;
    WalkWarp walkWarp = WalkWarp::None;  // walk B-warp key (game_explore)
    Use use = Use::Consume;              // what B (Use) does instead of consume (game_items)
    Category category = Category::Derive;  // ITEMS type-picker tile (items_screen)
    // How often this item comes up against its pool-mates, wherever it is pooled.
    // 0 (the default) = derive from `rarity` via kItemRarityDropWeight, which is the
    // right answer for most rows; set it when THIS item should be commoner or scarcer
    // than the rest of its tier (a kitchen staple you trip over vs. a tier-mate you
    // don't). A single pool can still override it for itself — LootEntry::weight.
    int dropWeight = 0;
    CacheDef cache = {};                 // Use::OpenContainer rows only — the reward draw
    SpoilDef spoil = {};                 // perishable rows only — what it turns into, and how often
};

// Does this effect cap at ONE use per PET LIFETIME? These are the only per-pet gates the
// player cannot get back by any means: a new egg clears them and nothing else does. Named
// here rather than as a list of item ids so a new item carrying one of these effects is
// counted without a second table to update.
//
// The last six are the PERMANENT upgrades an Epic dish grants (core/model/pet_upgrades.h):
// once the pet has one, later helpings of the same dish are just the food.
//
// ForceTrojanDivert is deliberately NOT one of them, close as it looks: it is CONSUMED at
// the pet's next evolution rather than capped for the life, so a second Ambig-USB after
// that one has fired is a real use rather than a wasted item.
constexpr bool isOncePerPetLifetime(ItemEffect::Kind k) {
    return k == ItemEffect::Kind::RemoveCareMistakeOnce ||
           k == ItemEffect::Kind::ClearMistakeShieldOnce ||
           k == ItemEffect::Kind::BandwidthRegenBonusMin ||
           k == ItemEffect::Kind::StatPointPower ||
           k == ItemEffect::Kind::StatPointDefense ||
           k == ItemEffect::Kind::StatPointSpeed ||
           k == ItemEffect::Kind::StatPointHealth ||
           k == ItemEffect::Kind::XpRateBonusPct;
}

// Is this effect a USB one — something the pet's port HOLDS rather than something the pet
// eats? Every USB item steers the same boundary (the next evolution), which is why the
// soak pair can lock the port against the rest of the family: a stage cannot be both
// stretched and diverted by two devices at once. Named off the effect vocabulary rather
// than as a list of ids, so a future USB is one row in content_items.cpp and nothing else.
constexpr bool isUsbEffect(ItemEffect::Kind k) {
    return k == ItemEffect::Kind::ForceTrojanDivert ||
           k == ItemEffect::Kind::ForceEvolveBranchGood ||
           k == ItemEffect::Kind::ForceEvolveBranchBad ||
           k == ItemEffect::Kind::ArmEvolveSoak;
}

// Does this ROW carry any USB effect (see isUsbEffect)? The item-level question every
// gate actually asks.
constexpr bool itemIsUsb(const ItemDef& d) {
    for (const ItemEffect& e : d.effects)
        if (isUsbEffect(e.kind)) return true;
    return false;
}

// The magnitude of this row's evolve-soak, or 0 if it carries none — what the ITEMS gate
// reads to know a row is one of the soak USBs without re-walking its effects by hand.
constexpr int itemEvolveSoakFactor(const ItemDef& d) {
    for (const ItemEffect& e : d.effects)
        if (e.kind == ItemEffect::Kind::ArmEvolveSoak) return e.magnitude;
    return 0;
}

// The combat-stat index an off-level stat-point effect grants into (0 power · 1 defense ·
// 2 speed · 3 max-Health — game_internal.h's levelStatName names them), or -1 for any
// other kind. The one place the four Kinds map back onto the one array they all write, so
// no applier or readout hand-writes the correspondence.
constexpr int statPointEffectIndex(ItemEffect::Kind k) {
    switch (k) {
        case ItemEffect::Kind::StatPointPower:   return 0;
        case ItemEffect::Kind::StatPointDefense: return 1;
        case ItemEffect::Kind::StatPointSpeed:   return 2;
        case ItemEffect::Kind::StatPointHealth:  return 3;
        default: return -1;
    }
}

// The display WORD for a combat stat on that same axis. It lives here, at the bottom of
// the stack, because BOTH layers name these: an Epic dish's derived readout
// (core/content/effect_text.cpp) and the engine's own levelStatName (app/game_internal.h,
// which delegates to this). One place, so the Rollback picker, the level-up readout and
// an item's spec grid can never call the same stat two different things.
constexpr const char* levelStatWord(int i) {
    switch (i) {
        case 0: return "POWER";
        case 1: return "DEFENSE";
        case 2: return "SPEED";
        case 3: return "MAX-HP";
        default: return "?";
    }
}

// ...and the same question of a whole row: does using this item spend something this pet
// can never have again? The 'Pedia's pet page lists exactly these (pedia_state.cpp).
inline bool itemIsOncePerPetLifetime(const ItemDef& d) {
    for (const ItemEffect& e : d.effects)
        if (isOncePerPetLifetime(e.kind)) return true;
    return false;
}

// This item's drop weight: its own `dropWeight` if it names one, else its rarity's
// default. The one place the fallback is unfolded.
inline int itemDropWeight(const ItemDef& d) {
    if (d.dropWeight > 0) return d.dropWeight;
    const int r = static_cast<int>(d.rarity);
    return (r >= 0 && r < 4) ? kItemRarityDropWeight[r] : 1;
}

// A mod's structured combat effect: a data-driven kind + magnitude, applied
// data-driven in combat.cpp — the same shape MoveDef's effect fields use. Each kind is
// applied once, at fight start (makePlayerCombatant) or at the matching
// combat/post-battle hook. Line-agnostic by default (mods are the customization
// see LINE_MOVE_IDENTITIES.md) — a `line` affinity only ADDS a bonus, never locks.
enum class ModEffect : uint8_t {
    None = 0,        // legacy / inert (no structured effect)
    PowerPct,        // +magnitude% to the pet's attack-power lean (powerMultPct)
    DamageCutPct,    // +magnitude% incoming-damage cut (dmgReducePct, under the 85% clamp)
    MaxHealth,       // +magnitude flat max-Health (and starting Health)
    Speed,           // +magnitude initiative; magnitude2 = % attack-power COST (Overclock)
    PostBattleBits,  // +magnitude Bits after a won encounter (Packet Sniffer)
    RaidMirror,      // negate one fatal hit, then consumed (one-shot; RAID Mirror)
    FatigueFragCut,  // magnitude% of the battle-fatigue Frag tax (Heat Sink)
    Thorns,          // reflect magnitude damage to any attacker that hits you (Honeytoken)
    DeathBlast,      // on the pet's KO, deal magnitude to the enemy (Deadman Switch)
    MaxHitCapPct,    // ECC Memory — no single incoming hit may exceed magnitude% of the
                     // pet's max Health (the "no variance today → cap the burst" read of
                     // ECC: caps channel detonations / crits-in-spirit; magnitude is a %
                     // of max Health, resolved to a flat cap at fight start).
    LoadBalance,     // Load Balancer — a big hit (>= magnitude% of max Health) is SPLIT:
                     // magnitude2% of it is DEFERRED to the victim's next turn-start, the
                     // rest lands now. Unlike ECC it doesn't REDUCE total damage, only
                     // spreads it — buying a turn (heal / land a KO first). If the debt
                     // isn't outrun it lands next turn and can KO on its own.
    WatchdogClamp,   // Watchdog Timer — an incoming STUN (MoveDef::lockTurns) on the pet is
                     // CLAMPED to at most magnitude turns (the watchdog reboots a hung
                     // process). magnitude 1 = you never lose more than one turn to a lock.
    FaradayCut,      // Faraday Cage — incoming DoT (MoveDef::dot*) on the pet is cut by
                     // magnitude% (100 = immune — the cage shields against the corruption).
    ArmorPiercePct,  // DRM Stripper / Hull Auger / Rowhammer / DMA Breach — every hit this
                     // pet lands ignores magnitude% of the target's damage cut AND of its
                     // one-shot brace, exactly as MoveDef::armorPiercePct does, and blunted
                     // by the same defender-side pierceResistPct. It is the ANSWER to the
                     // thing that made raw attack power not worth a slot: power raises a
                     // number that a defensive wall then deletes, so the two rows are a
                     // PAIR — two of three slots spent to make elevated damage actually
                     // arrive. Composed multiplicatively with a move's own pierce (each
                     // cuts what the last one left), so no stack of pierces ever reaches
                     // "the defender has no defence".
    RegenPerTurn,    // Trickle Charger / Shadow Copy — restore magnitude Health at the start
                     // of each of the pet's own turns, clamped to max Health. The one
                     // RECOVERY axis in a combat that otherwise only ever subtracts, which
                     // is also what finally gives a raised max-Health ceiling somewhere to
                     // climb back into. It ticks AFTER the turn's Load Balancer debt and DoT
                     // rot and only if the pet survived them, on purpose: rot is pinned as
                     // the damage the mitigation stack cannot reach, and a heal that
                     // pre-empted the tick would quietly become a second Faraday Cage
                     // instead of a different thing. So it recovers from the fight, never
                     // from the tick that is currently killing you.

    // --- Niche-flavour build-around mods, one per idea --------------------------
    FirstStrikeRankMult,  // Prowlware — rank the pet's equipped Attack moves by power
                          // (ties share a rank, weakest = rank 1); the FIRST landed
                          // damaging hit this fight is multiplied by the used move's
                          // rank. Rewards a spread of differently-powered Attack moves
                          // and leading with the strongest.
    AttackCountPowerPct,  // Botnet Swarm — +magnitude% attack power PER equipped Attack
                          // move (a glass-cannon build-around: stack attack slots).
    DefendCountCutPct,    // Air-Gap Ward — +magnitude% incoming-damage cut PER equipped
                          // Defend move (the defensive mirror of Botnet Swarm).
    FirstHitCutPct,       // Canary Trap — the FIRST hit the pet takes each fight is cut
                          // an extra magnitude% (a decoy absorbs the opening probe).
    LowHealthPowerPct,    // Meltdown Core — while own Health <= magnitude% of max, attack
                          // power rises magnitude2% (a comeback surge, checked live).
    GambleBattlePowerPct, // Zero-Day Exploit — a magnitude% chance, rolled once at fight
                          // start, to raise attack power magnitude2% for the whole fight.
    ConditionalThorns,    // Tripwire — like Thorns, but only reflects magnitude damage
                          // while own Health <= magnitude2% of max (a last-ditch snare;
                          // kept separate from Thorns so it never mixes accumulation with
                          // an always-on Thorns mod equipped alongside it).
    StealAmplifyPct,      // Phishing Rod — amplifies the Obfuscation-bubble "Perfect
                          // Bite" bonus (Combat::applyEffect) by magnitude% (line-gated
                          // via ModDef::requiresLine); dormant without a live shieldHp
                          // pool to bite through, however aggressively it's equipped.

    // --- Line build-arounds for the two lines that had none -----------------------
    // Both amplify a LINE PASSIVE rather than a stat, which is why both are hard-gated
    // (ModDef::requiresLine) the way Phishing Rod is: the passive is the thing they
    // multiply, so off-line they would be inert rows rather than weak ones.
    ExecOverridePct,      // Ring-0 Shim — +magnitude percentage POINTS on the Trojan's
                          // Execution-Override hijack chance (Combat::execOverrideChance,
                          // which is kExecOverrideBasePct + the armed traps' bonuses).
                          // Adds to the same sum the traps do, so it stacks with a trap
                          // build instead of replacing the reason to hold one.
    ReplicaSpawnPct,      // Replication Bus — +magnitude percentage POINTS on every
                          // replica-spawn roll a Worm makes (Combat::rollWormSpawn reads
                          // MoveDef::replicaSpawnPct). Raises the RATE, never the CAP:
                          // kWormReplicaSlots still bounds the board, so this fills the
                          // slots sooner rather than making more of them.
    ExtortionLedger,      // Extortion Ledger — `magnitude`% incoming-damage cut standing,
                          // and `magnitude2`% ATTACK POWER on top while the pet is holding
                          // an unsettled ransom pool (Combatant::ransomPool).
                          //
                          // Both halves serve the same reading, which is the family's own:
                          // Ransomware is BRUTE FORCE with one gimmick, strong from the
                          // first turn rather than ramping into it. The cut is the brute
                          // half — it just keeps standing — and the pool is the gimmick,
                          // paying for as long as the pet is carrying damage it has not
                          // answered for. Gated on the POOL and not on a seizure: a
                          // seizure needs a full Cipher stack under a live window and is a
                          // payoff most fights never reach, so a bonus hung there averages
                          // to nothing however large the number is made.
    ReplicaWorthPct,      // Replication Bus — every copy a Worm puts on the board is worth
                          // `magnitude`% more, on whichever currency that copy deals in
                          // (an attacker's banked damage, a defender's Health). It raises
                          // what a copy IS rather than how often one arrives: the spawn
                          // roll is already certain on half the line's rows, and a full
                          // board does not roll at all, so rate was never the binding
                          // constraint — kWormReplicaSlots is, and this pays inside it.
    PolymorphEffectPct,   // Mutation Engine — an EFFECT KIND this fighter has not reached
                          // for yet (moveEffectMask, combat.h) pays `magnitude` of
                          // Polymorph's own stat points, in the casting move's currency
                          // (polymorphPay). An amplifier of the passive rather than a
                          // bonus beside it, and inert on a fighter not running Polymorph.
                          // A deliberately DIFFERENT axis from what it feeds: Polymorph
                          // counts distinct MOVES, so a kit of plain swings ramps the pet
                          // and pays this nothing. What it rewards is rolling into rows
                          // that DO something — a stun, a pierce, a pool, a trap — which
                          // is the reason to want the two lines a wildcard row draws from
                          // rather than the shared roster it mostly hits.
};

// Slot-based hardware equip (MODS). Mods are PERMANENT:
// consumed on equip, no unequip, overwrite discards the old one. THREE independent
// axes drive earn + power: `rarity` is the DROP WEIGHT within an area's
// loot table; `powerTier` (1..N) is the ladder DEPTH, which decides which area a mod
// drops in; `equipLevel` is the pet level the mod needs, authored per row.
// `effectKind`/`magnitude` are the structured combat effect (applied data-driven, not
// via a hardcoded per-mod `if`). `line`/`affinityBonus` give a few signature mods a
// bonus for a matching line (still equippable by anyone — line-agnostic).
struct ModDef {
    // Stable SAVE identity, and the reason it comes first: the owned-mod pool is stored
    // as a count per wire number, not as a list of ids (save.h), so this number is what
    // a blob means by "two Firewall Patches". Never reused, never renumbered — a row may
    // be reordered, regrouped by area or retired freely, but its number is spent
    // forever. Exactly the contract kAchievements has carried since v1; the native gate
    // enforces uniqueness the same way.
    int wire;
    const char* id;          // stable id, e.g. "firewall_patch"
    const char* displayName; // e.g. "Firewall Patch"
    const char* effectTag;   // short tag shown on the slot row, e.g. "+DEF" / "1-SHOT"
    const char* effect;      // concrete effect text for the picker (not a hint);
                             // a template over this row — {mag}/{mag2}/{bonus}/
                             // {magBonus} (effect_text.h)
    bool oneShot;            // consumed when it triggers in play (UI just flags it)
    // mods-into-combat pass ---
    ItemDef::Rarity rarity = ItemDef::Rarity::Common;  // drop WEIGHT in an area table
    int powerTier = 1;                 // 1..N ladder depth → which area's pool drops it
    // The pet level this mod needs before it can be EQUIPPED — authored here, one level
    // per mod, the same for every copy the player holds (the picker lists mods by TYPE
    // and has never had a way to offer one copy over another). Legal range is
    // [0, kModEquipLevelMax]. Deliberately NOT derived from `powerTier`: a rank is a
    // ladder DEPTH, and a ladder has only as many rungs as there are areas, which is too
    // few gates to spread mods across a level range that keeps growing. What does hold is
    // that the two agree in ORDER — a deeper tier never gates lower than a shallower one
    // (test_mod_equip_ladder_is_ordered_and_dense).
    int equipLevel = 0;
    ModEffect effectKind = ModEffect::None;  // structured combat effect (data-driven)
    int magnitude = 0;                 // effect-specific units (%, flat HP, Bits, ...)
    int magnitude2 = 0;                // secondary knob (Overclock's power cost, ...)
    const char* line = nullptr;        // optional line AFFINITY (bonus on match; not a lock)
    int affinityBonus = 0;             // +magnitude added when the pet's line == `line`
    // A HARD equip gate (niche-flavour pass), distinct from `line`/`affinityBonus` above:
    // when set, the mod can only be EQUIPPED by a pet whose CreatureDef::line matches this
    // exactly — checked at equip time (Game::commitModEquip / onModPicker), not a drop-time
    // filter, so it can still drop for anyone and sit in inventory until they raise the
    // matching line. nullptr = no hard gate (every existing/line-agnostic mod).
    const char* requiresLine = nullptr;
};

// The pet level `m` needs before it can be equipped — the mod's OWN gate, identical for
// every copy the player holds. A function rather than the raw field at each call site
// because every screen that shows a gate must show the same one, and this is the seam to
// change if the gate ever stops being a plain read of the row.
inline int modEquipLevel(const ModDef& m) { return m.equipLevel; }

// A combat move — a collectible roster like creatures/items/mods.
// The autonomous combat engine rolls a pet's equipped moves off its attack/defend
// lean; the player's agency is which moves they slot (MOVES) + the A+C
// override. Effects beyond power/kind/channel are a later content pass; this
// seeds the system. Per-move glyph is ICON_MOVE_<UPPER ID> (like ICON_MOD_*).
struct MoveDef {
    // Alias, not a nested enum: MoveKind (defs.h, above CreatureDef) is shared
    // with CreatureDef::slotKinds, which is declared earlier in this header and
    // can't forward-reference a type nested in MoveDef. Every existing
    // `MoveDef::Kind`/`MoveDef::Kind::Attack` call site keeps compiling unchanged.
    using Kind = MoveKind;

    const char* id;          // stable id, e.g. "packet_storm"
    const char* displayName; // e.g. "Packet Storm"
    Kind kind;               // attack or defend — drives the loadout lean
    int power;               // attack: damage dealt · defend: damage mitigated
    int channelTurns;        // 1 = single-turn; >1 = multi-turn wind-up
    const char* effect;      // concrete effect text for the picker (not a hint);
                             // a template over this row's own fields — {power},
                             // {pierce}, {evade}, {dot}, ... (effect_text.h)
    Stage minStage = Stage::BootSector;  // evolution stage a move UNLOCKS at.
                                         // A move may be OWNED earlier but can't be
                                         // equipped/used until the pet reaches minStage
                                         // (mirrors slotsForStage gating SLOTS — this
                                         // gates the MOVES). Default = always available.

    // Line + mechanical effects (see LINE_MOVE_IDENTITIES.md) ---
    // A move is either GENERIC (line == nullptr → any pet can learn it, the shared
    // pool) or LINE-specific (only that line's pets can learn/equip it). The effect
    // fields below are the move's structured combat mechanics: all are transient combat
    // state, wiped every fight.
    const char* line = nullptr;   // nullptr = generic; else exclusive line id
    int stackPowerPct = 0;        // Lockout track: +% to the CASTER's Power on landing
    int stackPowerCap = 0;        //   this hit, accumulating up to this total (0 = off)
    int stackDefensePct = 0;      // Cipher track: +% incoming-damage cut to the CASTER
    int stackDefenseCap = 0;      //   on casting this brace, up to this total (0 = off)
    int armorPiercePct = 0;       // this hit ignores this % of the target's guard +
                                  //   dmgReducePct (MBR Wipe — the wall doesn't matter)

    // --- Deferred-mod pass · the two THREATS the last two mods counter ------------
    // Both ride on a landed Attack (a rider on the hit, not a standalone move kind), so
    // they stay data-driven and any move/roster can carry them. Countered by MODS:
    // lockTurns by Watchdog Timer (clamps the lock), dot* by Faraday Cage (cuts the DoT).
    int lockTurns = 0;            // STUN: on a landed hit, freeze the target's next N turns
    int dotDamage = 0;            // DoT: on a landed hit, apply this much damage per turn...
    int dotTurns = 0;             //   ...for this many of the target's upcoming turn-starts

    // --- Steal track (generic — any line's Attack move may set these) -------------
    // On a landed Attack hit, each non-zero field below steals that % of the TARGET's
    // CURRENT stat — added to the caster, subtracted from the target, permanent for
    // the rest of the fight (Combat::applyEffect). A move can set more than one; every
    // non-zero field fires on the same hit (deterministic, no random pick). 0 = no
    // steal on that stat (every non-Phishing move today, the only line using the
    // track). stealPowerPct/stealDefensePct fire unconditionally; stealSpeedPct/
    // stealCurrentHpPct additionally require the caster's Obfuscation shield to be up
    // (shieldHp > 0) — see the "Perfect Bite" bonus in Combat::applyEffect and
    // content_passives.h's Phishing section.
    int stealPowerPct = 0;       // target's powerMultPct (floored, kStealPowerFloorPct)
    int stealDefensePct = 0;     // target's dmgReducePct (floored at 0)
    int stealSpeedPct = 0;       // target's CURRENT speed (floored, kStealSpeedFloor);
                                  // bubble-gated (see above)
    int stealCurrentHpPct = 0;   // target's CURRENT health, a lifesteal-style drain;
                                  // bubble-gated (see above)
    int stealMaxHpPct = 0;       // target's maxHealth ceiling, permanently for the
                                  // fight — the pool MOVES, so the caster gains the
                                  // ceiling AND the Health in it, and the target's
                                  // current Health clamps down to its new ceiling

    // --- Obfuscation shield-pool (Phishing defensive track) ----------------------
    // >0 marks a Defend move as a POOLABLE shield (a "second health bar") instead of
    // the one-shot `guard` brace: casting ADDS `power * defenseMultPct/100` to the
    // caster's shieldHp (stacks on recast), and incoming damage overflows to real
    // Health only once the pool pops (see Combat::applyEffect). 0 = ordinary brace.
    int shieldPool = 0;

    // --- Trojan trap track (Trojan defensive line) -------------------------------
    // A Trojan Defend move ARMS a trap instead of bracing: casting stacks it onto the
    // caster's trap pile (cap kTrojanTrapCap) where it stays until an incoming enemy
    // attack TRIGGERS one — which deletes trapEvasionPct% of that hit's final damage,
    // reflects trapReboundPct% of the damage the trap mitigated back through the
    // attacker's (now-rotting) defense, and strips trapArmorRot flat % Defense off the
    // attacker for the rest of the fight. Armed traps also feed the Execution-Override
    // passive: each contributes trapPassiveBonusPct to its trigger chance, so holding
    // all three traps makes the hijack likely (Combat::execOverrideChance). trapArm=1
    // marks the row a trap (like shieldPool=1 marks a pool); 0 = every non-Trojan Defend.
    int trapArm = 0;
    int trapEvasionPct = 0;       // % of the triggering hit's final damage deleted
    int trapReboundPct = 0;       // % of the mitigated damage reflected at the attacker
    int trapArmorRot = 0;         // flat % Defense stripped from the attacker (permanent)
    int trapPassiveBonusPct = 0;  // this armed trap's bump to the Execution-Override chance

    // --- Worm replication track (Worm line) --------------------------------------
    // A worm move that carries replicaSpawnPct may SPAWN a replica into one of the
    // caster's free replication slots (kWormReplicaSlots): an Attack row spawns an
    // ATTACKER, a Defend row a DEFENDER, so the move's own `kind` decides which and no
    // row ever has to say it twice. 0 = no replication (every non-Worm move, and any
    // Worm row that is purely a swing).
    //
    // The two magnitudes below are each read by ONE kind, and each is a percentage of
    // the PARENT, so a replica is always a fraction of the worm that made it:
    //   replicaPowerPct  — an ATTACKER's base damage, as % of this move's own power.
    //   replicaHealthPct — a DEFENDER's base Health, as % of the parent's maxHealth.
    // Their multipliers (the live count of the OTHER kind) are the passive's half and
    // live in content_passives.h, not here.
    int replicaSpawnPct = 0;
    int replicaPowerPct = 0;
    int replicaHealthPct = 0;

    // --- Chained moves -----------------------------------------------------------
    // A move that hands its slot to a FOLLOW-UP step: casting this row resolves it
    // normally and commits the caster's next turn to the step named here
    // (Combat::resolveTurn, via Combatant::chainFollow). So one slot alternates between
    // two rows — the pattern a wind-up wanted and could not express, because a wind-up
    // spends its first turn doing nothing at all while both halves of a chain are real
    // casts. Which is the whole point: the action-economy tax a channel pays is the
    // turn it wastes, and a chain wastes none.
    //
    // The step is resolved against the registry when the Combatant is BUILT (never
    // mid-fight, so Combat stays registry-free) and is looked up in the chain-step table
    // rather than the move roster — a follow-up is reached only by casting its entry, so
    // it is deliberately not something a pet can own, equip, be taught or be rolled.
    // nullptr = an ordinary move. See content_chain_steps.cpp.
    const char* chainNextId = nullptr;

    // --- Tempo refund (Defend rows) ----------------------------------------------
    // The % of one action's worth of speed gauge handed back to the caster the turn this
    // move resolves (kSpeedActionThreshold, Combat::resolveTurn). It exists because a
    // brace's real cost was never its magnitude: bracing spends a turn NOT swinging, and
    // measured against attacks competing for the same slot every pure brace in the roster
    // came out behind — while brace magnitude itself turned out to be worth almost nothing
    // per point, so no amount of bigger numbers could have closed it.
    //
    // A refund shortens the wait for the next action; it can never GRANT one outright. Per
    // ROW rather than a blanket rule on the kind, so it doubles as the lever that tells a
    // cheap quick block apart from a heavy wall: the small braces hand back most of the
    // tempo, the big ones stay a real commitment. 0 on every Attack, and on every Defend
    // that already does something besides brace (a pool, a trap, a spawn, a Cipher stack)
    // — those were never the rows paying this cost.
    int speedRefundPct = 0;

    // --- Poisoned data (Obfuscation pool rows) -----------------------------------
    // A pool row may also POISON whoever strikes it: an enemy attack landing on the live
    // bubble plants this DoT on the attacker (Combat::applyEffect). It is the Obfuscation
    // track's conversion from defence into damage, and it sits on the POOL rows on purpose —
    // the line's steals ride its ATTACK moves, which a defend-heavy pet with a single attack
    // slot almost never equips, so a conversion routed through those never reaches the pets
    // that need one. 0 on a pool row that only banks Health.
    //
    // Appended at the END of the struct, like every field before it: the rows are positional
    // initializers, so a field inserted mid-struct silently re-aims every magnitude after it.
    int poolRetaliateDot = 0;
    int poolRetaliateTurns = 0;

    // --- Metamorphic wildcard rows ------------------------------------------------
    // A row that does not cast ITSELF: it rolls one of the moves its pet could have been —
    // the generic roster of this row's own `kind`, plus the two lines named here — and
    // casts that instead (Combat::resolveTurn). Naming two of the four other lines rather
    // than all of them is what keeps a wildcard a CHOICE, since which pair a row reaches is
    // the only thing an operator tailors about it.
    //
    // Both null = an ordinary move, which is every row but the metamorphic track. The ids
    // resolve against the registry once, when the Combatant is built (buildWildPools),
    // never mid-fight — the rule `chainNextId` answers to as well. A wildcard row still
    // needs a real `power`, `kind` and `effect`: the kind decides which half of the roster
    // it draws from, and the rest is what the loadout and picker read off it.
    //
    // Appended at the END of the struct, like every field before it — the rows are
    // positional initializers, so a field inserted mid-struct re-aims every magnitude after it.
    const char* drawLineA = nullptr;
    const char* drawLineB = nullptr;
};

// Whether `m` rolls its cast out of a pool instead of being one (MoveDef::drawLineA).
inline bool moveIsWildcard(const MoveDef& m) { return m.drawLineA || m.drawLineB; }

// A PURE BRACE row: it mitigates and does nothing else, so the only fields that vary
// across the eleven of them are power, the stage they unlock at, and the tempo they hand
// back. Spelled out positionally each row would carry twenty-three zeros between minStage
// and speedRefundPct — a shape where the one number that matters is the hardest to see, and
// where a miscount silently assigns a magnitude to the wrong mechanic. The shape gets a
// name instead. Every other Defend row does something besides brace and stays positional,
// because for those the intermediate fields are the point.
constexpr MoveDef braceRow(const char* id, const char* displayName, int power,
                           const char* effect, Stage minStage, int speedRefundPct) {
    MoveDef m{};
    m.id = id;
    m.displayName = displayName;
    m.kind = MoveKind::Defend;
    m.power = power;
    m.channelTurns = 1;
    m.effect = effect;
    m.minStage = minStage;
    m.speedRefundPct = speedRefundPct;
    return m;
}

// An Obfuscation POOL row (Phishing): it banks a second Health bar and, from the second
// rung up, poisons whoever reads the decoy. Same reason braceRow exists — the fields that
// vary sit at opposite ends of a long positional tail, and a miscount there arms the wrong
// mechanic silently.
constexpr MoveDef poolRow(const char* id, const char* displayName, int power,
                          const char* effect, Stage minStage, int retaliateDot,
                          int retaliateTurns) {
    MoveDef m{};
    m.id = id;
    m.displayName = displayName;
    m.kind = MoveKind::Defend;
    m.power = power;
    m.channelTurns = 1;
    m.effect = effect;
    m.minStage = minStage;
    m.line = "phishing";
    m.shieldPool = 1;
    m.poolRetaliateDot = retaliateDot;
    m.poolRetaliateTurns = retaliateTurns;
    return m;
}

// A METAMORPHIC WILDCARD row: it casts nothing of its own, so the fields that would carry
// a mechanic are exactly the ones it leaves empty, and the two that matter (the lines it
// reaches) sit at the very end of the positional tail. Same reason braceRow and poolRow
// exist. `power` stays 0 on every one of them — a wildcard becomes whatever it rolls, so a
// number here would be a figure no cast ever uses.
constexpr MoveDef wildRow(const char* id, const char* displayName, MoveKind kind,
                          const char* effect, Stage minStage, const char* drawA,
                          const char* drawB) {
    MoveDef m{};
    m.id = id;
    m.displayName = displayName;
    m.kind = kind;
    m.power = 0;
    m.channelTurns = 1;
    m.effect = effect;
    m.minStage = minStage;
    m.line = "metamorphic";
    m.drawLineA = drawA;
    m.drawLineB = drawB;
    return m;
}

inline const char* moveKindTag(MoveDef::Kind k) {
    return k == MoveDef::Kind::Attack ? "ATK" : "DEF";
}

// Line-gating (LINE_MOVE_IDENTITIES.md): a move is learnable/equippable by a pet iff
// it is GENERIC (no line) or its line matches the pet's own line. This is the single
// predicate every learn/drop/equip site funnels through, so a Ransomware pet can
// never acquire a Phishing-line move (and vice-versa), while the generic pool stays
// open to all. A null petLine (a lineless creature) can still take generic moves.
inline bool moveAllowedForLine(const MoveDef& m, const char* petLine) {
    if (m.line == nullptr) return true;                       // generic — anyone
    return petLine != nullptr && std::strcmp(m.line, petLine) == 0;
}

// has the pet at `petStage` reached the stage a move unlocks at? Owning a
// move isn't enough — a locked move can't be equipped (picker) nor fielded (combat).
inline bool moveUnlockedAtStage(const MoveDef& m, Stage petStage) {
    return stageIndex(petStage) >= stageIndex(m.minStage);
}

// FOOD -> BUFFS -> QUEST: the inventory's fixed use-frequency order.
inline int itemTypeOrder(ItemDef::Type t) { return static_cast<int>(t); }

inline const char* itemTypeName(ItemDef::Type t) {
    switch (t) {
        case ItemDef::Type::Food: return "FOOD";
        case ItemDef::Type::Buff: return "BUFFS";
        case ItemDef::Type::Quest: return "QUEST";
    }
    return "?";
}

// An item's resolved type-picker category: its own `category` when the row names
// one, otherwise derived from its type (Food -> Food, Buff -> Buffs, Quest ->
// Tools). The one place ItemDef::Category::Derive is unfolded.
inline ItemDef::Category itemCategory(const ItemDef& d) {
    if (d.category != ItemDef::Category::Derive) return d.category;
    switch (d.type) {
        case ItemDef::Type::Food: return ItemDef::Category::Food;
        case ItemDef::Type::Buff: return ItemDef::Category::Buffs;
        case ItemDef::Type::Quest: return ItemDef::Category::Tools;
    }
    return ItemDef::Category::Tools;
}

inline const char* rarityName(ItemDef::Rarity r) {
    switch (r) {
        case ItemDef::Rarity::Common: return "COMMON";
        case ItemDef::Rarity::Uncommon: return "UNCOMMON";
        case ItemDef::Rarity::Rare: return "RARE";
        case ItemDef::Rarity::Epic: return "EPIC";
    }
    return "?";
}

} // namespace mal
