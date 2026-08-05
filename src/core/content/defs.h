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

// Attack/Defend move typing (move-slot rework #12). Free-standing rather than
// nested in MoveDef (below) because CreatureDef, declared next, needs it for its
// per-slot layout — MoveDef itself aliases this as MoveDef::Kind so every
// existing `MoveDef::Kind::Attack`-style call site keeps compiling unchanged.
enum class MoveKind { Attack, Defend };

// How a creature gets around under its own power, which is what the idle habitat's
// resting motion reads (core/model/idle_wander.h): a walker keeps its feet on the
// shelf, a flier holds an altitude above it, a swimmer ignores the shelf entirely.
// MOVEMENT only — which sprite row a mover animates on is the sheet's business.
enum class Locomotion : uint8_t { Walk, Fly, Swim };

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
    // Per-slot Attack/Defend typing (move-slot rework #12). Each of the
    // kMaxMoveSlots move slots on THIS creature is permanently locked to one
    // MoveKind. Game::stampSlotKinds() reads slotKinds[i] the INSTANT slot i first
    // unlocks for a pet and never rewrites it afterward — so the creature actually
    // occupying the pet's evolution path at the moment each slot unlocks decides
    // that slot's kind for the rest of the raise (evolving Good vs Bad at the
    // Daemon hop can therefore leave slot 3 typed differently per branch). The
    // type-agnostic default move (Quick Jab) ignores this — it fills ANY empty
    // slot regardless of kind, and is never itself equippable.
    // PLACEHOLDER layout (balance TBD, not a design-reviewed roster pass) — seeded
    // only so the mechanism is live: Process pets lean slot0/1 Attack (Paypup varies
    // at slot1 for flavour), Script adds a Defend at slot2,
    // and the Daemon branch splits at slot3 to mirror the power/Frag lean —
    // Good (durable) -> Defend, Bad (glass cannon) -> Attack.
    MoveKind slotKinds[kMaxMoveSlots] = {MoveKind::Attack, MoveKind::Attack,
                                         MoveKind::Attack, MoveKind::Attack};
    // Cross-line Trojan infiltration hook. When a Process pet with this set evolves,
    // a kTrojanDivertPct chance (Game::fireEvolution) diverts it into this Trojan
    // creature instead of its normal successor — the pet keeps its old line's colour
    // but is now a Trojan (losing its line moves for the Trojan kit; see
    // Game::completeEvolution). The first divert unlocks the family. nullptr = no
    // divert path (every non-infiltratable creature).
    const char* evolvesToTrojanId = nullptr;
    // How this creature moves around its habitat at rest. EVERY row states it, even
    // the walkers: how a creature moves is a fact about that creature, and it belongs
    // where the rest of it is read rather than being inferred from an absence. It
    // also changes WITHIN a line — a Tadpoll swims, the Croaken it becomes does not —
    // so there is no line-level answer to inherit. The default is a floor for a row
    // that predates a new mover kind, not a licence to leave it off.
    Locomotion locomotion = Locomotion::Walk;
};

// Which minigame an egg line's hatch runs. One entry per interaction shape; the
// applier is Game::startHatchGame (game_lifecycle.cpp), which is the map of every
// route out of layEgg(). A line that wants a new shape adds a kind here plus one
// case there — never an `if (lineId == "...")` at a call site.
enum class HatchGame : uint8_t {
    Decrypt,   // Decryption Hatch: the incubating egg is brute-forced by presses
               // in the second half of its clock (Game::openDecryptMinigame).
    Clutch,    // Clutch Pick: the egg is laid among identical decoys and found by
               // halving the clutch, right away (Game::startEggPick).
};

struct EggLineDef {
    const char* id;            // e.g. "ransomware"
    const char* displayName;   // e.g. "Ransomware"
    const char* eggCreatureId; // the Boot-Sector creature, e.g. "cryptoshell"
    HatchGame hatchGame = HatchGame::Decrypt;
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
        ClearReplicationGhost, // Air-Gapped Snack: clear a Replication Ghost
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
// pantry is disturbed. Spoilage deliberately has NO clock of its own — it rides a beat
// that is already happening (a bite of food), the way ItemEffect::Kind::HungerStacking
// rides the Hunger tick. Per-stack expiry would mean an Inventory stack stopped being a
// count and became a list of objects each carrying its own timestamp, which is the one
// shape this must not take.
//
// Only read when `into` is set; every other row leaves it inert. Consumer:
// Game::rollPantrySpoilage (game_items.cpp), which rolls once per held stack and
// converts a single unit — so a full larder decays at the same rate as a single item,
// and nothing turns while the player isn't playing.
//
// Chains are allowed but pointless: give the `into` row its own SpoilDef only if the
// second stage should also rot.
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
        DecryptEgg,     // ready a Boot-Sector egg's decrypt NOW (Decryptogram)
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
};

// Slot-based hardware equip (MODS). Mods are PERMANENT (D3):
// consumed on equip, no unequip, overwrite discards the old one. Two independent
// axes drive earn + power: `rarity` is the DROP WEIGHT within an area's
// loot table; `powerTier` (1..N) is the sliding EFFECTIVENESS rank that decides which
// area a mod drops in AND the nominal equip-LEVEL band (a dropped instance rolls its
// own required pet-level within ±50% of the band — see kModEquipLevel* in tunables).
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
    int powerTier = 1;                 // 1..N effectiveness rank → area home + equip band
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

// The pet level `m` needs before it can be equipped — the mod's OWN gate, derived from
// its power tier, identical for every copy the player holds. One function rather than a
// field so the gate can't drift from the tier that sets the mod's place in the ladder,
// and one call site's worth of arithmetic so no screen has to remember the formula.
inline int modEquipLevel(const ModDef& m) { return modEquipLevelFloor(m.powerTier); }

// A combat move — a collectible roster like creatures/items/mods.
// The autonomous combat engine rolls a pet's equipped moves off its attack/defend
// lean; the player's agency is which moves they slot (TRAIN) + the A+C
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
                                  // fight (clamps target's current health down with it)

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
};

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
