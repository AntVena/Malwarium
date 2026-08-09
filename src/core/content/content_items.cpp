// content_items.cpp — the item table (ItemDef + its ItemEffect levers).
//
// One content table (see content_tables.h). Edit rows here; the registry picks
// them up unchanged via embedded_content.cpp.
#include "core/content/content_tables.h"

namespace mal {

// Each row lists its on-Use pet levers in `effects` (ItemEffect kind+magnitude — the
// complete, scannable set; magnitudes live here, never in tunables). Trailing hand-off
// fields (combatHeal / preEncounterXp / bitsPrice / walkWarp / use) point at the OTHER
// systems an item reaches — a plain feed/buff row leaves them defaulted. See ItemDef.
//
// Never type a magnitude into the description text: write `{hunger}` / `{heal}` /
// `{depth}` and effect_text.h substitutes the value from this same row, so retuning a
// number retunes the prose with it. The screens also draw a derived stat line under
// the prose (statLine()), which reports every magnitude here whether or not the
// sentence mentions it — so a description's job is what the item is FOR, not arithmetic.
// Reward pools ---------------------------------------------------------------
// The item sets a container draws from, and the one the walk's own loot-cache event
// pays out of (kLootPool, declared in content_tables.h for game_explore). Pools live
// HERE, beside the rows they name, so a cache's whole definition — purse, draw count,
// pool, find weight — reads off one place: its own row's CacheDef.
//
// Pools are rarity-graded: the common tiers hand over consumables, the higher ones add
// the scarce utility items. Every understood item needs a live earn path (the native
// gate asserts it), so an item that isn't shop-stocked, starting kit, or a walk find
// has to appear in one of these.
//
// A pool row is a LootEntry (defs.h): an id, plus a weight ONLY when this pool wants
// that item at a different frequency from everywhere else. A bare `{"id"}` draws at
// the item's own ItemDef::dropWeight, which in turn falls back to its rarity — so read
// the weights on the rows here as exceptions, and the item table as the rule.
namespace {
const LootEntry kCachePoolCommon[]   = {{"airgap_snack"}, {"tortilla_chip"},
                                        {"null_noodles"}, {"decrypt_key"},
                                        {"backdoor_bell"},
                                        // The common half of the pantry.
                                        {"spam"}, {"breadcrumbs"}, {"c_salt"},
                                        {"grepsed_oil"}, {"cronstarch"},
                                        {"boolean_cubes"}, {"vanilla_extract"},
                                        {"polltatoes"}, {"regeggs"}, {"data_leek"},
                                        {"spoiled_macrol"},
                                        {"universal_cereal_box"}};
const LootEntry kCachePoolUncommon[] = {{"airgap_snack"}, {"r007_b33r"},
                                        {"sinkhole_trap"}, {"pwnzu_sauce"},
                                        {"osi_dip"}, {"rootkit_bell"},
                                        // The uncommon half of the pantry.
                                        {"java"}, {"kernel_oil"},
                                        {"syntactic_sugar"}, {"applets"},
                                        {"root_veg"}, {"fresh_macrol"},
                                        {"desalinated_c_salt"}};
const LootEntry kCachePoolRare[]     = {{"backup_drive"}, {"sinkhole_trap"},
                                        {"rollback"}, {"deep_learning_module"},
                                        {"kernel_bell"}};
const LootEntry kCachePoolEpic[]     = {{"rollback"}, {"yubi_cookie"},
                                        {"backup_drive"}, {"restore_point"},
                                        {"deep_learning_core"}, {"zeroday_bell"}};
// The Commendation Cache's own pool: what an achievement pays out. Deliberately not
// the Epic pool — a commendation is earned by playing a whole ladder out, so it hands
// over the scarce per-lifetime shields and the deepest diving bells rather than
// re-rolling the same walk consumables a found cache already gives.
const LootEntry kCachePoolCommend[]  = {{"restore_point"}, {"yubi_cookie"},
                                        {"deep_learning_core"}, {"zeroday_bell"},
                                        {"kernel_bell"}, {"ambig_usb"}, {"rollback"}};
template <int N>
constexpr int poolN(const LootEntry (&)[N]) { return N; }
// The share of a staple's own dropWeight it draws at in kLootPool below. The pantry
// is meant to be the bulk of what a CACHE hands over, but the walk's loot event is the
// only source of the diving bells and the scarce utility items — at full weight the
// staples would crowd those out of the one place they come from, so every pantry row
// there carries this override instead.
constexpr int kStapleWalkWeight = 8;
}  // namespace

// The walk loot-cache event's own pool (Game::grantLootReward + the Wi-Fi sleeping-
// guardian / open-cache sub-outcomes), and the legacy `sealed_cache`'s draw. The
// DeepWeb Dive's depth items ride here too — earned from any area's walk loot, not
// gated to the dive itself (they're only USEFUL there).
const LootEntry kLootPool[] = {{"airgap_snack"}, {"tortilla_chip"},
    {"pwnzu_sauce"}, {"backup_drive"}, {"rollback"}, {"osi_dip"},
    {"deep_learning_module"}, {"deep_learning_core"}, {"backdoor_bell"},
    {"rootkit_bell"}, {"kernel_bell"}, {"zeroday_bell"},
    // The pantry, thinned to kStapleWalkWeight apiece — see the note on that constant.
    {"spam", kStapleWalkWeight}, {"breadcrumbs", kStapleWalkWeight},
    {"c_salt", kStapleWalkWeight}, {"grepsed_oil", kStapleWalkWeight},
    {"cronstarch", kStapleWalkWeight}, {"boolean_cubes", kStapleWalkWeight},
    {"vanilla_extract", kStapleWalkWeight}, {"polltatoes", kStapleWalkWeight},
    {"regeggs", kStapleWalkWeight}, {"data_leek", kStapleWalkWeight},
    {"spoiled_macrol", kStapleWalkWeight},
    {"universal_cereal_box", kStapleWalkWeight}, {"java", kStapleWalkWeight},
    {"kernel_oil", kStapleWalkWeight}, {"syntactic_sugar", kStapleWalkWeight},
    {"applets", kStapleWalkWeight}, {"root_veg", kStapleWalkWeight},
    {"fresh_macrol", kStapleWalkWeight},
    {"desalinated_c_salt", kStapleWalkWeight}};
const int kLootPoolCount = poolN(kLootPool);

using IE = ItemEffect;
const ItemDef kItems[] = {
    //
    // COMMON ITEMS --------------------------
    //
    {"decrypt_key", "Decryption Key", ItemDef::Type::Quest,
     ItemDef::Rarity::Common, "Satisfies the Lockout currency demand for free.",
     ItemDef::Context::LockoutOnly, /*effects=*/{}, /*combatHeal=*/0,
     /*preEncounterXp=*/0, /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None,
     /*use=*/ItemDef::Use::Consume, /*category=*/ItemDef::Category::Keys},
    
     // the Defrag Tool — a common consumable spent by a TOOL DEFRAG in MAINT
    // for a GUARANTEED clean (no fail roll). Never used from the ITEMS path (gated in
    // itemUsable); sold at the shop + drops from wild loot so it's obtainable. No new
    // glyph — reuses the MAINT defrag icon (itemIcon fallback).
    {"disk_scrubber", "Defrag Tool", ItemDef::Type::Quest,
     ItemDef::Rarity::Common, "Spend in MAINT for a guaranteed clean defrag.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0,
     /*preEncounterXp=*/0, /*bits=*/14},

    // Null Noodles: de-frags, makes the pet HUNGRIER, and pulls
    // Happiness toward 50%.
    {"null_noodles", "Null Noodles", ItemDef::Type::Food,
     ItemDef::Rarity::Common,
     "Tastes like... nothing. Sheds {|frag|} Fragmentation for {|hunger|} Hunger.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, -15}, {IE::Kind::Frag, -15}, {IE::Kind::HappyToward50, 20}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/5},

    // Decryptogram: the egg accelerator, granted into starting inventory on
    // every new egg. Use on a Boot-Sector egg takes a flat kDecryptogramCutMs off its
    // incubation, floored at the crackable window — every line's hatch minigame is
    // played at lay-time, so there is nothing left for an item to open. Quest-typed
    // (falls through the egg-phase ITEMS gate), no vitals, priceless, egg-only.
    {"decryptogram", "Decryptogram", ItemDef::Type::Quest,
     ItemDef::Rarity::Common,
     "Use on the egg to cut 10 minutes off its incubation.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::DecryptEgg,
     /*category=*/ItemDef::Category::Keys},

     // A random reward that doesn't have to come from the source zone's drop table.
     // The four rarity caches' findWeight values are the walk's cache-find distribution
     // and sum to 100, so a weight reads directly as a percentage.
     {"sealed_cache_common", "Common Cache", ItemDef::Type::Quest,
     ItemDef::Rarity::Common,
     "A common data cache. Open from the VAULT for a reward draw.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::OpenContainer,
     /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/0, /*cache=*/{/*bits=*/10, /*draws=*/1, /*drawChancePct=*/100,
                kCachePoolCommon, poolN(kCachePoolCommon), /*findWeight=*/50}},

    // Backdoor/Rootkit/Kernel/Zero-Day Bell: a diving bell lowers you straight to a
    // depth instead of swimming down — these arm the NEXT DeepWeb Dive to skip
    // straight to a given depth (SetDeepWebStartDepth(ToBest)), consumed the moment
    // that dive starts, so a pet re-earns its way back to a genuine struggle
    // without re-walking every shallow depth first.
    {"backdoor_bell", "Backdoor Bell", ItemDef::Type::Buff,
     ItemDef::Rarity::Common, "Starts the next DeepWeb Dive at depth {depth}.",
     ItemDef::Context::Anytime, {{IE::Kind::SetDeepWebStartDepth, 16}}},

    //
    // UNCOMMON ITEMS --------------------------
    //
    // The ghost cure. Air-gapping is what you do to a machine you can't trust to stay
    // connected to itself — which is exactly a Replication Ghost's problem — so this is
    // the one food whose flavour is also its mechanic. It stays an ordinary snack on a
    // pet with no ghost; the clear is a no-op then.
    {"airgap_snack", "Air-Gapped Snack", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Fills {hunger} Hunger. Patches {heal} Health. Cuts a Replication "
     "Ghost loose.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 40}, {IE::Kind::ClearReplicationGhost, 0}}, /*combatHeal=*/30},
    
    // Intended to be combined with Null Noodles to produce a rare food
    {"pwnzu_sauce", "Pwnzu Sauce", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "A bit intense all by itself. if only it could go on something with a truly neutral flavour.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 15}}, /*combatHeal=*/0},
    
    {"tortilla_chip", "Tor-Tilla Chip", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon, "A crunchy morsel of onion-routed corn.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, 10}}},
 
    {"osi_dip", "OSI Dip", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon, "Seven glorious layers. If only there was something good to eat it on. A sort of... eighth layer...",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, 10}}},

     // Sinkhole Trap: bypass the next wild encounter for a flat XP lump (preEncounterXp,
    // applied by game_combat::resolveSinkhole — a hand-off, not an on-Use pet effect).
    {"sinkhole_trap", "Sinkhole Trap", ItemDef::Type::Quest,
     ItemDef::Rarity::Uncommon, "Bypasses the next wild encounter for {xp} XP.",
     ItemDef::Context::PreEncounter, /*effects=*/{}, /*combatHeal=*/0,
     /*preEncounterXp=*/40},
    
     {"r007_b33r", "R007_B33R", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Junk food: {hunger} Hunger and {happy} Happiness, at {frag} Fragmentation.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 15}, {IE::Kind::Happy, 25}, {IE::Kind::Frag, 5}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/5},
    
    // Sealed Cache: a locked container picked up non-interrupting on
    // the walk Cache drop tables are in game.cpp.
    // The untiered original cache: no findWeight, so nothing drops it any more — it
    // exists so a save written before the rarity tiers still has an openable container,
    // and it pays out of the shared walk-loot pool at that event's own rate.
    {"sealed_cache", "Sealed Cache", ItemDef::Type::Quest,
     ItemDef::Rarity::Uncommon,
     "A locked data cache. Open from the VAULT for a reward draw.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
    /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::OpenContainer,
     /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/0, /*cache=*/{kLootBitsReward, /*draws=*/1, /*drawChancePct=*/kLootItemChancePct,
                kLootPool, poolN(kLootPool), /*findWeight=*/0}},

    {"sealed_cache_uncommon", "Uncommon Cache", ItemDef::Type::Quest,
     ItemDef::Rarity::Uncommon,
     "An uncommon data cache. Open from the VAULT for a reward draw.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::OpenContainer,
     /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/0, /*cache=*/{/*bits=*/18, /*draws=*/1, /*drawChancePct=*/100,
                kCachePoolUncommon, poolN(kCachePoolUncommon), /*findWeight=*/30}},

    // Rootkit Bell: the Backdoor Bell's deeper cousin — see its comment above.
    {"rootkit_bell", "Rootkit Bell", ItemDef::Type::Buff,
     ItemDef::Rarity::Uncommon, "Starts the next DeepWeb Dive at depth {depth}.",
     ItemDef::Context::Anytime, {{IE::Kind::SetDeepWebStartDepth, 32}}},
    //
    // STAPLE INGREDIENTS --------------------------
    // The pantry: the raw materials recipes are built out of. Grouped together rather
    // than filed under the rarity headings above because what makes one of these
    // findable is being a STAPLE, not its tier — the tier only says how much a pet
    // gets out of eating one raw, which is generally "not much, and sometimes less
    // than nothing". Most carry an explicit dropWeight so the pantry isn't a flat
    // shelf: the ones the flavour says you trip over (Spam, Breadcrumbs) turn up far
    // more often than the ones it says you don't (Root Veg, Fresh Macrol).
    //
    {"spam", "Spam", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Plentiful. Far too plentiful. Find something to do with it all.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 2}}, /*combatHeal=*/0,
     /*preEncounterXp=*/0, /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None,
     /*use=*/ItemDef::Use::Consume, /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/90},

    {"breadcrumbs", "Breadcrumbs", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "There's a reason enough of these to make a trail were left uneaten.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 1}}, /*combatHeal=*/0,
     /*preEncounterXp=*/0, /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None,
     /*use=*/ItemDef::Use::Consume, /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/80},

    {"c_salt", "C-Salt", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Tastes salty.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, -2}}, /*combatHeal=*/0,
     /*preEncounterXp=*/0, /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None,
     /*use=*/ItemDef::Use::Consume, /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/70},

    {"grepsed_oil", "Grep-sed Oil", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "This humble staple turns raw ingredients into cuisine. You'll find it in any "
     "cupboard.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, -5}, {IE::Kind::Frag, -5}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/70},

    {"spoiled_macrol", "Spoiled Macrol", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Smells a bit too phishy to eat. Someone down at the pier could make better use "
     "of it.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, -15}}, /*combatHeal=*/0,
     /*preEncounterXp=*/0, /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None,
     /*use=*/ItemDef::Use::Consume, /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/60},

    {"cronstarch", "Cronstarch", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "There's a time to use this, and it is NOT as a snack.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 1}, {IE::Kind::Happy, -1}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/55},

    // Boolean Cubes take no dropWeight — Common's own default is exactly the middle
    // of this shelf, which is where they belong.
    {"boolean_cubes", "Boolean Cubes", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "You either love them or you don't.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, -5}}},

    {"vanilla_extract", "Vanilla Extract", ItemDef::Type::Food,
     ItemDef::Rarity::Common,
     "There's a reason this is a default flavour: nobody's messed with the recipe in "
     "years.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 1}, {IE::Kind::Happy, 1}, {IE::Kind::Frag, 1}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/45},

    // Polltatoes: the one stacking food (ItemEffect::Kind::HungerStacking). Eaten
    // alone it is a Breadcrumb; eaten in a run it climbs, and the pet's next passive
    // Hunger-decay tick ends the run.
    {"polltatoes", "Polltatoes", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Bland on its own. Every Polltatoes eaten since the pet last lost a Hunger point "
     "adds another {hungerStack} to what the next one fills.",
     ItemDef::Context::Anytime, {{IE::Kind::HungerStacking, 1}}, /*combatHeal=*/0,
     /*preEncounterXp=*/0, /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None,
     /*use=*/ItemDef::Use::Consume, /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/45},

    {"regeggs", "RegEggs", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "You must be yoking.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 8}}, /*combatHeal=*/0,
     /*preEncounterXp=*/0, /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None,
     /*use=*/ItemDef::Use::Consume, /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/40},

    {"data_leek", "Data Leek", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "These used to be exclusive. Now everyone has a copy.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, 3}, {IE::Kind::Frag, -3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/35},

    {"universal_cereal_box", "Universal Cereal Box", ItemDef::Type::Food,
     ItemDef::Rarity::Common,
     "One box, every distribution. Nine hundred serving suggestions and no ingredients "
     "list.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 10}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/30},

    {"java", "Java", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Can't start your morning without it.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, 5}, {IE::Kind::Frag, -2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/35},

    // Kernel Oil takes Uncommon's own default weight — the middle of its shelf.
    {"kernel_oil", "Kernel Oil", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Why would you drink plain oil?",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, -5}, {IE::Kind::Frag, -5}}},

    {"syntactic_sugar", "Syntactic Sugar", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "It's very clearly labelled: 'Not particularly filling or good for you. Makes you "
     "happy.'",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 1}, {IE::Kind::Happy, 15}, {IE::Kind::Frag, 3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/25},

    {"applets", "Applets", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Enough of these and you'll forget what a diagnostic report even looks like.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, 3}, {IE::Kind::Frag, -5}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/20},

    {"root_veg", "Root Veg", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "You need special permissions to order a box of these.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 6}, {IE::Kind::Frag, -6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/15},

    {"fresh_macrol", "Fresh Macrol", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Tastes great, but it goes bad quickly: {spoil}% odds per feeding.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 5}, {IE::Kind::Frag, -3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/12, /*cache=*/{},
     // The one perishable. Low enough that a stack usually survives a feeding session,
     // high enough that hoarding it is a losing plan — which is the whole point of the
     // trade against Spoiled Macrol, the Pier-to-Peer currency it decays into.
     /*spoil=*/{"spoiled_macrol", 5}},

    // Desalinated C-Salt is mostly a TRADE good: Pier-to-Peer takes a Spoiled Macrol
    // for one (pirate_bayou/area.cpp), which is a far better rate than finding it.
    {"desalinated_c_salt", "Desalinated C-Salt", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "The shaker isn't exactly empty; there's 'nothing' in it.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, -2}, {IE::Kind::Frag, -1}, {IE::Kind::HappyToward50, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/8},

    //
    // RARE ITEMS --------------------------
    //
    // Created when Null_Noodles and Pwn-zu Sauce are combined (Hacker MERGE HUB).
    {"pwnzu_patched_noodles", "Pwnzu-Patched Noodles", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "It tastes like Grandma Yubi's Cooking.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 100}, {IE::Kind::Frag, -100}, {IE::Kind::Happy, 100}}, /*combatHeal=*/0},

    // Created when Tor-Tilla Chip and OSI Dip are combined (Hacker MERGE HUB) — the
    // eighth layer OSI Dip was missing. Fills every stat at once.
    {"fully_stacked_nachos", "Fully-Stacked Nachos", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "We tried every combination. In the end it turns out the secret eighth dippy layer... was always you.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 100}, {IE::Kind::Frag, -100}, {IE::Kind::Happy, 100}}, /*combatHeal=*/0},

    // Hashed Browns + Salted&Hashed Browns: the pantry's first cooked dish and its
    // second pass through the pan. Both are MERGE HUB outputs (game_internal.h's
    // kMergeRecipes) AND stocked at Moor-to-Moor — buying one is how a player meets
    // the dish before they can cook it, which is exactly what the Rig Shop asks for
    // before it will sell either recipe (game_rig_shop.h's requiresItems).
    {"hashed_browns", "Hashed Browns", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon, "Crispy!",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 20}, {IE::Kind::Happy, 10}, {IE::Kind::Frag, -5}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/512},

    {"salted_hashed_browns", "Salted&Hashed Browns", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Extra-salted. Bad for the heart, good for the soul.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 20}, {IE::Kind::Happy, 20}, {IE::Kind::Frag, -5}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/512},

    // Backup Drive: a combat buff, not a Lockout item. Use arms a 1-hour DEATH-SAVE
    // (ItemEffect::ArmCombatShieldBuff, save v30). Every hit lands in full; the drive is
    // read once, at the moment the pet would be overwhelmed (Combat::checkOutcome), and
    // hands back half of MAX Health from wherever the pet ended up. So it usually saves
    // a life and sometimes doesn't — a blow that buried the pet deeper than half its max
    // is past restoring, which is the honest version of what a backup can do. Consumed
    // either way, win or lose. Deliberately NOT the RAID Mirror mod's job: the mod
    // spends itself negating the first hit of any size, whereas the drive ignores hits
    // entirely and only ever answers the question "is this pet gone?". If the hour runs
    // out unused it just lapses; another Backup Drive re-arms it.
    {"backup_drive", "Backup Drive", ItemDef::Type::Buff,
     ItemDef::Rarity::Rare,
     "For {shieldMins} minutes, a pet that goes down is restored with half its max Health. One save.",
     ItemDef::Context::Anytime, {{IE::Kind::ArmCombatShieldBuff, 60}}},
    
    // Rare Cache: Open in the VAULT for a Rare Reward not locked to any area in particular
     {"sealed_cache_rare", "Rare Cache", ItemDef::Type::Quest,
     ItemDef::Rarity::Rare,
     "A rare data cache. Open from the VAULT for a richer reward draw.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::OpenContainer,
     /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/0, /*cache=*/{/*bits=*/30, /*draws=*/1, /*drawChancePct=*/100,
                kCachePoolRare, poolN(kCachePoolRare), /*findWeight=*/15}},

    // Kernel Bell: the Backdoor Bell's deeper cousin still — see its comment above.
    {"kernel_bell", "Kernel Bell", ItemDef::Type::Buff,
     ItemDef::Rarity::Rare, "Starts the next DeepWeb Dive at depth {depth}.",
     ItemDef::Context::Anytime, {{IE::Kind::SetDeepWebStartDepth, 64}}},

    // Deep-Learning Module: arms the dive's depth-per-win multiplier
    // (ArmDeepWebDepthMultiplier) — the pet "learns" the shallow depths fast so it
    // skips ahead on every win instead of one step at a time. Overwrites, doesn't
    // stack — a fresh Module/Core just replaces whichever multiplier is currently
    // armed. Lets a blitzing endgame pet catch back up to a real fight faster.
    {"deep_learning_module", "Deep-Learning Module", ItemDef::Type::Buff,
     ItemDef::Rarity::Rare, "Each DeepWeb Dive win advances the depth by {depthStep} instead of 1.",
     ItemDef::Context::Anytime, {{IE::Kind::ArmDeepWebDepthMultiplier, 2}}},
    //
    // EPIC ITEMS --------------------------
    //
     {"yubi_cookie", "Yubi-Cookie", ItemDef::Type::Buff,
     ItemDef::Rarity::Epic, "So delicious it could make the pet forget {mistakes} care mistake. Max 1 per lifecycle.",
     ItemDef::Context::Anytime, {{IE::Kind::RemoveCareMistakeOnce, 1}}},
    
     // Restore Point: a System-Restore shield — Use on a Process/Script pet to
    // arm protection against the NEXT care mistake, once per lifetime. The shield is
    // per-pet, consumed on the next positive mistake.
    {"restore_point", "Restore Point", ItemDef::Type::Buff,
     ItemDef::Rarity::Epic, "Shields against the next care mistake. Max 1 per lifecycle.",
     ItemDef::Context::Anytime, {{IE::Kind::ClearMistakeShieldOnce, 1}}},
    
    // Epic Cache: Open in the VAULT for an Epic Reward not locked to any area in particular
    // The Epic cache is also a second, non-boss MOD source: `modChancePct` is the
    // chance an Open ALSO yields a permanent mod, rolled globally rarity-weighted (even
    // a tier-4 mod can surface — its rolled equip-level gate holds it until the pet is
    // deep enough). The yield reveal shows the items/Bits; the mod lands in MODS.
    {"sealed_cache_epic", "Epic Cache", ItemDef::Type::Quest,
     ItemDef::Rarity::Epic,
     "An epic data cache. Open from the VAULT for the best reward draw.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::OpenContainer,
     /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/0, /*cache=*/{/*bits=*/55, /*draws=*/2, /*drawChancePct=*/100,
                kCachePoolEpic, poolN(kCachePoolEpic), /*findWeight=*/5,
                /*modChancePct=*/50}},

    // The Commendation Cache: the standing reward for an ACHIEVEMENT
    // (content_achievements.cpp rows list it in `rewards`). findWeight 0 — it is never
    // found on a walk, only earned, which is the whole point of it being a different
    // container from the four the 'net drops. Its purse and mod chance sit above Epic's
    // because a ladder takes far longer to finish than a cache takes to find.
    {"commend_cache", "Commendation Cache", ItemDef::Type::Quest,
     ItemDef::Rarity::Epic,
     "Earned, never found. Open from the VAULT for a commendation draw.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::OpenContainer,
     /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/0, /*cache=*/{/*bits=*/90, /*draws=*/2, /*drawChancePct=*/100,
                kCachePoolCommend, poolN(kCachePoolCommend), /*findWeight=*/0,
                /*modChancePct=*/60}},

    // Key warp items: consumables used DURING the
    // walk (the B warp picker) to jump straight to a target event, not eaten/buffed.
    // Using one from ITEMS is inert ("USE ON THE WALK", itemUsable).
    {"access_token", "Access Token", ItemDef::Type::Quest,
     ItemDef::Rarity::Uncommon,
     "Explore-use: warp straight to the nearest shop.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::Shop, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Keys},
    
    // Warp to a safe zone and rest off some fragmentation. The rest is the whole
    // point of the key, so how deep it cleans is a magnitude on THIS row (a negative
    // Frag effect — rest de-frags), applied by resolveSafeRestEvent through the same
    // applyItemEffects every other item goes through.
    {"safe_mode_key", "Safe-Mode Key", ItemDef::Type::Quest,
     ItemDef::Rarity::Uncommon,
     "Explore-use: warp straight to a safe rest.",
     ItemDef::Context::Anytime,
     /*effects=*/{{ItemEffect::Kind::Frag, -20}}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::SafeRest, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Keys},
    
    // Rollback: a level re-roll BUFF, not a quest item — it doesn't respond to a
    // strange event, it's a lever the player pulls at will on the stat RNG. Use opens
    // a stat picker (use=Rollback) to shed one earned combat-stat point (−1 that stat,
    // −1 level) so the pet re-grinds that level and re-rolls a fresh +1. A reward-pool
    // drop; inert at level 0 (nothing to shed).
    {"rollback", "Rollback", ItemDef::Type::Buff,
     ItemDef::Rarity::Rare,
     "Shed one earned stat point (-1 level) to re-roll it.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Rollback},
    
    // Ambig-USB: Use on a Process pet to guarantee its Trojan divert instead of leaving it to the
    // kTrojanDivertPct roll. Stocked item at Moor-to-Moor (Napstorrent Moors).
    {"ambig_usb", "Ambig-USB", ItemDef::Type::Buff,
     ItemDef::Rarity::Epic,
     "Guarantees the pet's next evolution diverts into a Trojan.",
     ItemDef::Context::Anytime, {{IE::Kind::ForceTrojanDivert, 1}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/1024},

    // Zero-Day Bell: the Backdoor Bell's ultimate cousin — instead of a fixed depth,
    // it warps the next DeepWeb Dive straight to THIS PET's own best-ever depth
    // (SetDeepWebStartDepthToBest reads bestDeepWebDepth_ at dive-start), never any
    // other pet's or the device's frontier.
    {"zeroday_bell", "Zero-Day Bell", ItemDef::Type::Buff,
     ItemDef::Rarity::Epic,
     "Starts the next DeepWeb Dive at this pet's own deepest depth reached.",
     ItemDef::Context::Anytime, {{IE::Kind::SetDeepWebStartDepthToBest, 0}}},

    // Deep-Learning Core: Deep-Learning Module's Epic upgrade — see its comment above.
    {"deep_learning_core", "Deep-Learning Core", ItemDef::Type::Buff,
     ItemDef::Rarity::Epic, "Each DeepWeb Dive win advances the depth by {depthStep} instead of 1.",
     ItemDef::Context::Anytime, {{IE::Kind::ArmDeepWebDepthMultiplier, 4}}},
};
const int kItemsCount = sizeof(kItems) / sizeof(kItems[0]);

// The Defrag Tool's id, exposed so MAINT (game_care.cpp) can gate/spend it
// without string-comparing against a bare literal.
const char* const kDefragToolId = "disk_scrubber";
// The Backup Drive's id, exposed so the Auto Backup / Continuous Auto-Backup Rig
// Shop upgrades (game_explore.cpp) can arm its shield programmatically via
// applyItemEffects without a bare literal.
const char* const kBackupDriveId = "backup_drive";

}  // namespace mal
