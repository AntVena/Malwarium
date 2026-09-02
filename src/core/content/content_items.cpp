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
const LootEntry kCachePoolCommon[]   = {{"dyno_nuggets"}, {"tortilla_chip"},
                                        {"null_noodles"}, {"decrypt_key"},
                                        {"backdoor_bell"},
                                        // The common half of the pantry.
                                        {"spam"}, {"breadcrumbs"}, {"c_salt"},
                                        {"grepsed_oil"}, {"cronstarch"},
                                        {"boolean_cubes"}, {"vanilla_extract"},
                                        {"polltatoes"}, {"regeggs"}, {"data_leek"},
                                        {"spoiled_macrol"},
                                        {"universal_cereal_box"},
                                        {"self_signed_flour"}, {"shellots"},
                                        {"linkguine"}, {"jailapeno"},
                                        {"churned_butter"},
                                        {"bytesteak_tomatoes"}, {"gherkins"},
                                        {"cruds"}, {"bootmeal"},
                                        {"garlic_escapes"}, {"grepefruit"},
                                        {"red_herring"},
                                        // The third shelf's common half.
                                        {"parsenips"}, {"romaine"}, {"bitroot"},
                                        {"swiss_chard"}, {"string_beans"},
                                        {"snap_peas"}, {"squash"},
                                        {"raidicchio"}, {"awkra"},
                                        {"kaliflower"}, {"archichoke"},
                                        {"flatpak_choi"}, {"capsicum"},
                                        {"peppermint"}, {"nixtamal"},
                                        {"pingapple"}, {"plaintain"},
                                        {"cloudberries"}, {"lintils"},
                                        {"perl_barley"}, {"basicmati_rice"},
                                        {"vpenne"}, {"unmonitored_oats"},
                                        {"yamls"}, {"chia_seeds"},
                                        {"cinnamon"}, {"mixins"}, {"nibbles"},
                                        {"humbugs"}, {"burp_sweets"},
                                        {"peer_drops"}};
const LootEntry kCachePoolUncommon[] = {{"dyno_nuggets"}, {"r007_b33r"},
                                        {"sinkhole_trap"}, {"pwnzu_sauce"},
                                        {"osi_dip"}, {"rootkit_bell"},
                                        {"decryptogram"},
                                        // The uncommon half of the pantry.
                                        {"java"}, {"kernel_oil"},
                                        {"syntactic_sugar"}, {"applets"},
                                        {"root_veg"}, {"fresh_macrol"},
                                        {"desalinated_c_salt"}, {"papaya"},
                                        {"mozillarella"}, {"imaple_syrup"},
                                        {"double_precision_cream"}, {"cocoa"},
                                        {"rubber_ducks"},
                                        // The third shelf's uncommon half.
                                        {"epoch_dates"}, {"dotfigs"},
                                        {"apiricot"}, {"raspberry_pis"},
                                        {"table_grapes"},
                                        {"minified_beef"},
                                        {"saasage"}, {"packed_sardines"},
                                        {"natto"},
                                        {"macadamia"}, {"cache_ews"},
                                        {"squid_ink"}, {"leaf_node_tea"}};
const LootEntry kCachePoolRare[]     = {{"backup_drive"}, {"sinkhole_trap"},
                                        {"rollback"}, {"deep_learning_module"},
                                        {"kernel_bell"}, {"decryptogram"},
                                        // THE SCARCE SHELF — the six staples the EPIC
                                        // dishes want, one apiece (content_recipes.cpp).
                                        // They are here rather than on the Uncommon pool
                                        // because they are the throttle on how many
                                        // permanent upgrades a player can cook: an Epic
                                        // dish is only as makeable as its scarcest
                                        // ingredient, and this pool plus a thinned walk
                                        // drop is the whole supply.
                                        {"honeypot_yogurt"}, {"lambda_chops"},
                                        {"file_mignon"}, {"paramesan"},
                                        {"silicon_wafers"},
                                        {"marshalled_mallows"}};
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
// ...and the share the six SCARCE staples draw at — the ones the Epic dishes are gated
// on (kCachePoolRare above). A quarter of an ordinary staple's, so a walk still turns
// one up now and then and an Epic dish stays cookable without a cache, just slowly.
// This is the one number that decides how fast a player can hand a pet a permanent
// upgrade, so it lives beside the weight it is a fraction of rather than in tunables.
constexpr int kRareStapleWalkWeight = 2;
}  // namespace

// The walk loot-cache event's own pool (Game::grantLootReward + the Wi-Fi sleeping-
// guardian / open-cache sub-outcomes), and the legacy `sealed_cache`'s draw. The
// DeepWeb Dive's depth items ride here too — earned from any area's walk loot, not
// gated to the dive itself (they're only USEFUL there).
const LootEntry kLootPool[] = {{"dyno_nuggets"}, {"tortilla_chip"},
    {"pwnzu_sauce"}, {"backup_drive"}, {"rollback"}, {"osi_dip"},
    {"deep_learning_module"}, {"deep_learning_core"}, {"backdoor_bell"},
    {"rootkit_bell"}, {"kernel_bell"}, {"zeroday_bell"}, {"decryptogram"},
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
    {"desalinated_c_salt", kStapleWalkWeight},
    {"self_signed_flour", kStapleWalkWeight}, {"shellots", kStapleWalkWeight},
    {"linkguine", kStapleWalkWeight}, {"jailapeno", kStapleWalkWeight},
    {"churned_butter", kStapleWalkWeight},
    {"bytesteak_tomatoes", kStapleWalkWeight}, {"gherkins", kStapleWalkWeight},
    {"cruds", kStapleWalkWeight}, {"bootmeal", kStapleWalkWeight},
    {"garlic_escapes", kStapleWalkWeight}, {"grepefruit", kStapleWalkWeight},
    {"red_herring", kStapleWalkWeight}, {"papaya", kStapleWalkWeight},
    {"mozillarella", kStapleWalkWeight}, {"imaple_syrup", kStapleWalkWeight},
    {"double_precision_cream", kStapleWalkWeight}, {"cocoa", kStapleWalkWeight},
    {"rubber_ducks", kStapleWalkWeight},
    {"honeypot_yogurt", kRareStapleWalkWeight},
    {"parsenips", kStapleWalkWeight}, {"romaine", kStapleWalkWeight},
    {"bitroot", kStapleWalkWeight}, {"swiss_chard", kStapleWalkWeight},
    {"string_beans", kStapleWalkWeight}, {"snap_peas", kStapleWalkWeight},
    {"squash", kStapleWalkWeight}, {"raidicchio", kStapleWalkWeight},
    {"awkra", kStapleWalkWeight}, {"kaliflower", kStapleWalkWeight},
    {"archichoke", kStapleWalkWeight}, {"flatpak_choi", kStapleWalkWeight},
    {"capsicum", kStapleWalkWeight}, {"peppermint", kStapleWalkWeight},
    {"nixtamal", kStapleWalkWeight}, {"pingapple", kStapleWalkWeight},
    {"plaintain", kStapleWalkWeight}, {"cloudberries", kStapleWalkWeight},
    {"lintils", kStapleWalkWeight}, {"perl_barley", kStapleWalkWeight},
    {"basicmati_rice", kStapleWalkWeight}, {"vpenne", kStapleWalkWeight},
    {"unmonitored_oats", kStapleWalkWeight}, {"yamls", kStapleWalkWeight},
    {"chia_seeds", kStapleWalkWeight}, {"cinnamon", kStapleWalkWeight},
    {"mixins", kStapleWalkWeight}, {"nibbles", kStapleWalkWeight},
    {"humbugs", kStapleWalkWeight}, {"burp_sweets", kStapleWalkWeight},
    {"peer_drops", kStapleWalkWeight}, {"epoch_dates", kStapleWalkWeight},
    {"dotfigs", kStapleWalkWeight}, {"apiricot", kStapleWalkWeight},
    {"raspberry_pis", kStapleWalkWeight}, {"table_grapes", kStapleWalkWeight},
    {"lambda_chops", kRareStapleWalkWeight}, {"file_mignon", kRareStapleWalkWeight},
    {"minified_beef", kStapleWalkWeight}, {"saasage", kStapleWalkWeight},
    {"packed_sardines", kStapleWalkWeight}, {"natto", kStapleWalkWeight},
    {"paramesan", kRareStapleWalkWeight}, {"macadamia", kStapleWalkWeight},
    {"cache_ews", kStapleWalkWeight}, {"squid_ink", kStapleWalkWeight},
    {"leaf_node_tea", kStapleWalkWeight}, {"silicon_wafers", kRareStapleWalkWeight},
    {"marshalled_mallows", kRareStapleWalkWeight}};
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

    // Boot Accelerator: the egg accelerator, granted into starting inventory on every
    // new egg. Use on a Boot-Sector egg takes a flat kBootAcceleratorCutMs off its
    // incubation, floored at the crackable window — every line's hatch minigame is
    // played at lay-time, so there is nothing left for an item to open. Quest-typed
    // (falls through the egg-phase ITEMS gate), no vitals, egg-only.
    {"boot_accelerator", "Boot Accelerator", ItemDef::Type::Quest,
     ItemDef::Rarity::Common,
     "Use on the egg to cut 10 minutes off its incubation.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::DecryptEgg,
     /*category=*/ItemDef::Category::Keys},

    // Decryptogram: a found ticket to one DECRYPTOGRAM board (content_quotes.h). Cashed
    // in at the Hacker VAULT, never from ITEMS — the prize is a player-level account
    // unlock, so it is spent where the other things you cash in are, and itemUsable
    // gates the pet path with "CASH IN AT VAULT" the way it does a sealed cache.
    // Priceless on purpose: no storefront sells one, so the pool only drains as fast as
    // the walk hands them over.
    {"decryptogram", "Decryptogram", ItemDef::Type::Quest,
     ItemDef::Rarity::Uncommon,
     "Cash in at the VAULT to crack a quote for Bits and an upgrade.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None,
     /*use=*/ItemDef::Use::PlayCryptogram, /*category=*/ItemDef::Category::Keys},

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
    // The everyday ration, and the widest-spread food in the game: the starting shelf,
    // both cache pools, the walk's loot pool, every area's drop table and one storefront
    // all hand these over. A dyno is the container a process runs inside, so nuggets cut
    // in its shape are what the pantry feeds a process — which is the whole joke, and the
    // reason this is the one food that turns up everywhere rather than anywhere special.
    // It fills and patches and does nothing else; the ghost cure is Unlinkguine's job.
    {"dyno_nuggets", "Dyno Nuggets", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Fills {hunger} Hunger. Patches {heal} Health.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 40}}, /*combatHeal=*/30},

    // The ghost cure, and the counterpart to Linkguine ("every strand joined to the
    // last one"): unlink() severs the reference to a copy, which is exactly a
    // Replication Ghost's problem — the phantom process a failed defrag leaves behind
    // on a Critical disk (Worm-line only, game_care.cpp's resolveMaint). Cooked rather
    // than found: the Merge Hub row folds a Jailapeño into Linkguine, so curing a ghost
    // is something you learn to make instead of something you happen to hold.
    // A no-op on a pet with no ghost (any non-Worm pet, always).
    {"unlinkguine", "Unlinkguine", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Fills {hunger} Hunger. Patches {heal} Health. Cuts a Replication "
     "Ghost loose.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::ClearReplicationGhost, 0}}, /*combatHeal=*/20},

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

    // The second shelf of the pantry: the raw materials the dishes below the staples
    // reach for. Same rule as the first shelf — a tier says how little a pet gets out
    // of eating one raw, and the dropWeight ladder says how often you trip over it.
    {"self_signed_flour", "Self-Signed Flour", ItemDef::Type::Food,
     ItemDef::Rarity::Common,
     "It raised itself. Nobody else vouched for it.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 2}, {IE::Kind::Happy, -3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/65},

    {"shellots", "Shellots", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Peel a layer off and there is another prompt underneath.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 2}, {IE::Kind::Happy, -4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/60},

    {"linkguine", "Linkguine", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Long, thin, and every strand joined to the last one.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/55},

    // A jail is the isolation primitive a process gets put in when it can't be trusted
    // loose, which is what makes this the thing you fold into Linkguine to sever it.
    // Eaten raw it is a staple like any other; its reason to exist is the recipe.
    // ASCII only: the font is 32..126 (font_glyphs.cpp), so an "ñ" would draw as a
    // blank cell and mis-measure textWidth, which counts bytes.
    {"jailapeno", "Jailapeno", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Hot enough to keep a process where you put it.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, 4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/55},

    {"churned_butter", "Churned Butter", ItemDef::Type::Food,
     ItemDef::Rarity::Common,
     "Twelve thousand revisions. Same butter.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Frag, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/50},

    {"bytesteak_tomatoes", "Bytesteak Tomatoes", ItemDef::Type::Food,
     ItemDef::Rarity::Common,
     "They grow in eights. There has never been a ninth.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/50},

    {"gherkins", "Gherkins", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Given a jar, when you open it, then.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, -3}, {IE::Kind::Frag, -2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/45},

    {"cruds", "CRUDs", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Create one, read it, update it. Mostly delete it.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, -2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/40},

    // Bootmeal cooks into Portridge, which is the same bowl at the same tier for the
    // same numbers — the one recipe in the kitchen that changes nothing but where it
    // runs. The pair only reads as a joke because these effects and this rarity are
    // literally the ones on that dish's row.
    {"bootmeal", "Bootmeal", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Nothing else in the morning starts until this has.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 10}, {IE::Kind::Happy, -5}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/40},

    {"garlic_escapes", "Garlic Escapes", ItemDef::Type::Food,
     ItemDef::Rarity::Common,
     "Escape them properly or the whole line breaks.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, -6}, {IE::Kind::Frag, -4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/35},

    {"grepefruit", "Grepefruit", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Bitter. Only the segments that match are worth eating.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, -4},
                                 {IE::Kind::Frag, -4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/30},

    {"red_herring", "Red Herring", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "It is a decoy. Eat it anyway.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, -5}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/25},

    {"papaya", "PAPaya", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Hands over everything the moment anyone asks.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, 6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/30},

    {"mozillarella", "Mozillarella", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Free to copy, and it stretches further than anything else on the shelf.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 6}, {IE::Kind::Happy, 4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/25},

    {"imaple_syrup", "IMAPle Syrup", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Take as much as you like. It all stays on the tree.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 2}, {IE::Kind::Happy, 12},
                                 {IE::Kind::Frag, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/20},

    {"double_precision_cream", "Double-Precision Cream", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Twice the storage of the single. Pours exactly the same.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 7}, {IE::Kind::Frag, 3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/18},

    {"cocoa", "Cocoa", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Deprecated for years. Still ships in everything.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, 10}, {IE::Kind::Frag, -3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/15},

    {"rubber_ducks", "Rubber Ducks", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Explain the recipe to one before you start. It never argues.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 8}, {IE::Kind::Happy, 5}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/12},

    // Honeypot Yogurt is stocked so that its ABSENCE reads: Lossy Lassi is the one
    // drink it belongs in and the one recipe that never lists it, and a pot sitting on
    // the shelf is what makes that a joke rather than an oversight. Quicksortbet is
    // where it actually gets cooked.
    {"honeypot_yogurt", "Honeypot Yogurt", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Left out where anyone could take it. That is the point of it.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 8},
                                 {IE::Kind::Frag, -4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/0},

    // The pantry's third shelf: the produce, dry goods, protein and sweets the rest of
    // the kitchen reaches for. Same rule as the shelves above — the tier says how little
    // a pet gets out of eating one raw, and every row here is an ingredient in at least
    // one recipe, so no shelf is only ever chewed on.
    {"parsenips", "Parsenips", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "You have to parse them before they are any use.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, -3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/62},

    {"romaine", "ROMaine", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Read only. Nothing you do to it takes.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 2}, {IE::Kind::Frag, -3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/58},

    {"bitroot", "Bitroot", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Left in storage a season too long, and it shows.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, -4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/56},

    {"swiss_chard", "Swiss Chard", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Fixed width, every leaf.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Frag, -3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/54},

    {"string_beans", "String Beans", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "An array of them, and the last one is always empty.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/52},

    {"snap_peas", "Snap Peas", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Each pod ships with everything it needs and nothing it shares.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, 3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/50},

    {"squash", "Squash", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Seven of them went in. One came out.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 7}, {IE::Kind::Happy, -2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/48},

    {"raidicchio", "RAIDicchio", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Lose a leaf and lose nothing. There is another copy of it.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, -5},
                                 {IE::Kind::Frag, -3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/46},

    {"awkra", "AWKra", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Splits into fields the moment you cut it.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, -4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/44},

    {"kaliflower", "Kaliflower", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Comes with every tool already installed. Most of them sharp.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Frag, -2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/42},

    {"archichoke", "Archichoke", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Takes all afternoon to reach the part you can eat.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 2}, {IE::Kind::Happy, -6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/38},

    {"flatpak_choi", "Flatpak Choi", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Bundles its own everything. Twice the leaf you needed.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/36},

    {"capsicum", "CAPsicum", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Sweet, crisp, cheap. Pick two.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, 4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/34},

    {"peppermint", "Peppermint", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "The one seasoning you are meant to be able to see.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, 6}, {IE::Kind::Frag, -2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/32},

    {"nixtamal", "Nixtamal", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Reproducible. The same corn, every single time.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 6}, {IE::Kind::Happy, -3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/30},

    {"pingapple", "Pingapple", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Sweet, and it always comes back.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, 6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/28},

    {"plaintain", "Plaintain", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Unencrypted, in the open, right there in the bowl.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 7}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/26},

    {"cloudberries", "Cloudberries", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Somebody else's berries, on somebody else's bush.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 2}, {IE::Kind::Happy, 7}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/24},

    {"lintils", "Lintils", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Thousands of tiny complaints, and every one of them fair.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 6}, {IE::Kind::Happy, -4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/44},

    {"perl_barley", "Perl Barley", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Write the pot once. Nobody will read it again.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 6}, {IE::Kind::Frag, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/40},

    {"basicmati_rice", "BASICmati Rice", ItemDef::Type::Food,
     ItemDef::Rarity::Common,
     "Ten lines in the pot, and the last one sends you back to the first.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 8}, {IE::Kind::Happy, -2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/48},

    {"vpenne", "VPenne", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "A tube nobody else can see down.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Frag, -2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/46},

    {"unmonitored_oats", "Unmonitored Oats", ItemDef::Type::Food,
     ItemDef::Rarity::Common,
     "Left them overnight. Nobody was watching, and nobody was paged.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 6}, {IE::Kind::Happy, -6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/38},

    {"yamls", "YAMLs", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "How deep you cut them changes what they mean.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 7}, {IE::Kind::Happy, -5}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/36},

    {"epoch_dates", "Epoch Dates", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Picked the first of January, nineteen seventy. All of them.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 8}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/28},

    {"dotfigs", "Dotfigs", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Everybody's are different and everybody's are correct.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, 9}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/26},

    {"apiricot", "APIricot", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Documented, versioned, and rate-limited to two.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, 7}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/24},

    {"raspberry_pis", "Raspberry Pis", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Forty in the drawer. Three of them doing anything.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, 8}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/22},

    {"table_grapes", "Table Grapes", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Indexed, one row to a bunch.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, 6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/20},

    {"lambda_chops", "Lambda Chops", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Anonymous. Nobody can say which sheep they came off.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 10}, {IE::Kind::Happy, 3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/0},

    {"file_mignon", "File Mignon", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Small, tender, and somebody deleted the backup.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 9}, {IE::Kind::Happy, 6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/0},

    {"minified_beef", "Minified Beef", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Every scrap of whitespace stripped out of it.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 9}, {IE::Kind::Frag, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/20},

    {"saasage", "SaaSage", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "You never own one. You simply keep paying for it.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 8}, {IE::Kind::Happy, 4},
                                 {IE::Kind::Frag, 3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/22},

    {"packed_sardines", "Packed Sardines", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "No padding between them anywhere in the tin.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 9}, {IE::Kind::Happy, -3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/24},

    {"natto", "NATto", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "It gets through. Whatever is in the way, it gets through.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 7}, {IE::Kind::Happy, -8},
                                 {IE::Kind::Frag, -5}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/16},

    {"paramesan", "Paramesan", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Grated over the top. Optional, with a sensible default.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, 7}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/0},

    {"macadamia", "MACadamia", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Hard shell, unique address, and you can forge it.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 7}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/18},

    {"cache_ews", "Cache-ews", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Kept close, because reaching for them again is the slow part.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/20},

    {"chia_seeds", "Chia Seeds", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "They farm all night and hand over almost nothing.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 2}, {IE::Kind::Frag, -2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/34},

    {"cinnamon", "Cinnamon", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Warm, brown, and it themes everything it touches.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, 7}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/32},

    {"squid_ink", "Squid Ink", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Everything goes through it, and it keeps a note of all of it.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, -6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/16},

    {"leaf_node_tea", "Leaf-Node Tea", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Nothing hangs below it. This is the bottom of the tree.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, 9}, {IE::Kind::Frag, -6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/22},

    {"mixins", "Mixins", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Whatever you pour it into inherits the fizz.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 1}, {IE::Kind::Happy, 5}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/42},

    {"silicon_wafers", "Silicon Wafers", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Thin, flat, and worth more than the tin they came in.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, 8}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/0},

    {"nibbles", "Nibbles", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Four bits to a go, and two goes make a proper mouthful.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, 4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/40},

    {"marshalled_mallows", "Marshalled Mallows", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Flattened into a shape that travels, ready to be sent.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 2}, {IE::Kind::Happy, 11},
                                 {IE::Kind::Frag, 3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/0},

    {"humbugs", "Humbugs", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "There is one in every batch, and it is always striped.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 1}, {IE::Kind::Happy, 8},
                                 {IE::Kind::Frag, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/30},

    {"burp_sweets", "Burp Sweets", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Intercepts everything on the way down and lets you edit it.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 1}, {IE::Kind::Happy, 9},
                                 {IE::Kind::Frag, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/28},

    {"peer_drops", "Peer Drops", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Everyone has a bag. Not all of them arrive.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 1}, {IE::Kind::Happy, 10}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/26},

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
    // kMergeRecipes) AND stocked at Moor-to-Moor — buying one is how a player MEETS
    // the dish, which is what a Decryptogram asks for before it will teach either
    // recipe (game_internal.h's MergeRecipe::requiresItems). They are also the only
    // cooked dishes a storefront carries: every other one is the recipe or nothing.
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

    //
    // COOKED DISHES --------------------------
    // What the pantry is FOR. Every row below is a MERGE HUB output and nothing else —
    // no shop stocks one, no pool drops one, so the only way to hold a plate is to own
    // the recipe (game_rig_shop.h) and have the raw staples in the bag. They are filed
    // together rather than under their rarity headings for the same reason the staples
    // are: what defines one is being COOKED, and the tier only says how much of a step
    // up from its own ingredients it is.
    //
    // Each one earns its keep by doing something its ingredients can't. A staple eaten
    // raw moves one number by a handful; a dish moves the numbers a pet actually cares
    // about, and three of them heal in combat, which no staple does at all.
    //
    // At the top of the tier ladder sit the six EPIC dishes, and they are the reason to
    // cook at all: each grants the pet eating it something PERMANENT, once in that pet's
    // life (core/model/pet_upgrades.h). Tiramisudo shaves its Bandwidth regen; Privilege
    // Escalope, Spare RIBs, Racelette and Buffer Overfloat each hand it an off-level
    // point of Power / Defence / Speed / max-Health that no Rollback can take back; and
    // Profilerole raises what every XP source pays it. Six, not sixty: the whole weight
    // of the mechanic is that a pet can only ever be handed these once, so a second
    // helping of any of them is simply a very good meal.
    {"cracquettes", "Cracquettes", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Everything the filter caught, fried into one patty. Not classy, but the "
     "folder is finally empty.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 60}, {IE::Kind::Happy, 5}, {IE::Kind::Frag, 5}}},

    {"hackshuka", "Hackshuka", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Everything that was in the bag, in one pan. Now nobody can tell what leaked.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Happy, 15}}, /*combatHeal=*/40},

    {"applet_turnover", "Applet Turnover", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Sandboxed in pastry. It still runs.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 20}, {IE::Kind::Happy, 35}, {IE::Kind::Frag, -10}}},

    {"serial_bar", "Serial Bar", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Nine hundred serving suggestions, pressed into one you can carry.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::Happy, 10}, {IE::Kind::Frag, -5}}},

    {"macrol_fry_up", "Macrol Fry-Up", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Cooked through, so it stops going off. Whatever the pier says, this is the "
     "right end of a Fresh Macrol's short life.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 45}, {IE::Kind::Happy, 20}}, /*combatHeal=*/30},

    {"vanilla_java_roast", "Vanilla Java Roast", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Roasted dark, sweetened hard, and nobody's framework anywhere near it.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 45}, {IE::Kind::Frag, -30}}},

    // The pantry's second service. Three of these are the ingredient's own joke
    // finished: RISCotto is cooked in Boolean Cubes, which is what a risotto's stock
    // is; LANsagne is built on OSI Dip, whose seven layers are the dish; RAMen wants
    // exactly the noodles, egg and leek a bowl of it is made of.
    {"riscotto", "RISCotto", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Cooked down slowly until there is nothing left in it that didn't need to be. "
     "Fewer instructions, better executed.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 50}, {IE::Kind::Frag, -40}}},

    {"lansagne", "LANsagne", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Seven layers, and every one of them talks only to the layer above it.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 60}, {IE::Kind::Happy, 20}}, /*combatHeal=*/40},

    {"ramen", "RAMen", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Hot, fast and gone the moment the power is. Eat it while it's live.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 45}, {IE::Kind::Happy, 15}, {IE::Kind::Frag, -10}}},

    // EPIC — the upgrade that is not a stat: a permanently shorter Bandwidth regen.
    // First helping only, like every grant in the tier; after that the pet already has
    // root and this is simply a very good pudding that tops the pool up, which is what
    // its second effect is for.
    {"tiramisudo", "Tiramisudo", ItemDef::Type::Food,
     ItemDef::Rarity::Epic,
     "Ask the rig nicely and it says no. Ask it again like this and it says of course.",
     ItemDef::Context::Anytime,
     {{IE::Kind::BandwidthRegenBonusMin, 1}, {IE::Kind::Bandwidth, 1},
      {IE::Kind::Happy, 50}, {IE::Kind::Frag, -15}}},

    {"core_dumplings", "Core Dumplings", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Everything that was in memory when it went down, wrapped and steamed.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 55}, {IE::Kind::Happy, 5}}},

    {"forkaccia", "Forkaccia", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Tear off a piece and it carries on rising on its own.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 40}, {IE::Kind::Happy, 10}}},

    // The pantry's first SECOND-ORDER dish: its lead ingredient is another cooked
    // dish, not a staple. Which is what a casserole is — yesterday's cooking, kept.
    {"cacherole", "Cacherole", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Whatever was still warm from the last cook, held for later and served again "
     "faster than it was made.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 65}, {IE::Kind::Happy, 15}, {IE::Kind::Frag, -10}},
     /*combatHeal=*/25},

    {"gnulash", "GNUlash", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Everything in the pot at once, at a rolling boil, no plan whatsoever. Free to "
     "copy, and somehow it comes out right.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 70}, {IE::Kind::Frag, -25}}, /*combatHeal=*/60},

    // The third service. Two of these are shaped by the kitchen rather than by a
    // pantry shelf: Portridge is Bootmeal's own row cooked, down to the tier and the
    // magnitudes, and Chrootons want a loaf that was itself a merge.
    {"portridge", "Portridge", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Cooked, plated, and identical to what went in. Runs anywhere.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 10}, {IE::Kind::Happy, -5}}},

    {"halloumi_world", "Halloumi, World", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "The first thing anybody cooks. It squeaks, and it works.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 25}, {IE::Kind::Happy, 15}}},

    {"nan_bread", "NaN Bread", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "No two loaves are equal. Not even to themselves.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Frag, -5}}},

    {"chrootons", "Chrootons", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Cubed, fried, and unable to reach the rest of the bowl.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 20}, {IE::Kind::Happy, 15}, {IE::Kind::Frag, -10}}},

    {"gzipacho", "Gzipacho", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Everything that was in the pot, in a quarter of the space. Served cold.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 55}, {IE::Kind::Frag, -20}}, /*combatHeal=*/30},

    // The description IS the recipe, minus one line. What it leaves out is on the
    // shelf and in the MERGE HUB's own ingredient list for other dishes, so the gap
    // reads as loss rather than as a missing item.
    {"lossy_lassi", "Lossy Lassi", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Papaya, sugar, a pinch of salt. Whatever else was in it did not arrive.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 20}, {IE::Kind::Happy, 30}, {IE::Kind::Frag, -5}}},

    {"cod_review", "Cod Review", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Somebody else looked at it before it shipped. They said it was fine.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 40}, {IE::Kind::Happy, 10}}, /*combatHeal=*/25},

    {"recursive_turducken", "Recursive Turducken", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "A duck, in a duck, in a duck. The innermost one is only a duck.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 70}, {IE::Kind::Happy, 10}}, /*combatHeal=*/40},

    {"peking_duck_typing", "Peking Duck Typing", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "If it looks like dinner and quacks like dinner, serve it.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 60}, {IE::Kind::Happy, 25}, {IE::Kind::Frag, -10}}},

    {"semaphreddo", "Semaphreddo", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "One spoon at a time. Everybody else waits their turn.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 25}, {IE::Kind::Happy, 45}, {IE::Kind::Frag, -20}}},

    // The one dish that ADDS Fragmentation. It fills a pet up and leaves it in a
    // state nobody can follow, which is what the name promises.
    {"spaghetti_code", "Spaghetti Code", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "It came out as one piece. Nowhere in it does a strand start.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 55}, {IE::Kind::Frag, 15}}},

    {"emacsaroni", "Emacsaroni", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Comes with a mail client, a calendar, and a cheese sauce.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 45}, {IE::Kind::Happy, 20}}},

    {"bisectuits", "Bisectuits", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Halve the tin, taste, halve again. The bad one is in there somewhere.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 25}, {IE::Kind::Happy, 20}}},

    {"quicksortbet", "Quicksortbet", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Pick one, split the rest around it, repeat. Served in order.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 20}, {IE::Kind::Happy, 40}, {IE::Kind::Frag, -15}}},

    // The fourth service — the rest of the kitchen. Filed by what they are rather than
    // by tier, like every dish above: bread and bakery, then the pans, then the plates,
    // then the puddings, then what you drink with them.
    {"buguette", "Buguette", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Long, crusty, and riddled with them from end to end.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Happy, -5}}},

    {"chapati", "CHAPati", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "It asks you something before it lets you have any.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::Happy, 10}}},

    {"corrumpets", "Corrumpets", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Holes all the way through, and butter goes straight into them.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::Happy, 10}, {IE::Kind::Frag, 5}}},

    {"packettone", "Packettone", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Enormous, sweet, and it arrives in no particular order.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 60}, {IE::Kind::Happy, 25}}},

    {"hot_swapped_buns", "Hot-Swapped Buns", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Pulled out and replaced without anybody at the table noticing.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Happy, 15}}},

    {"current_buns", "Current Buns", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Draws rather more than the recipe said it would.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::Happy, 18}}},

    {"config_rolls", "Config Rolls", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Two of them on the plate and they disagree about the filling.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 25}, {IE::Kind::Happy, 20}}},

    {"crostini", "Crostini", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "A whole little system running on one slice of somebody else's bread.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 20}, {IE::Kind::Happy, 20}, {IE::Kind::Frag, -5}}},

    {"payloaf", "Payloaf", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Something is baked into the middle of it. It is not raisins.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 65}, {IE::Kind::Frag, 10}}, /*combatHeal=*/35},

    {"firewaffle", "Firewaffle", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "A grid of little closed squares. Syrup gets through anyway.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Happy, 20}, {IE::Kind::Frag, -8}}},

    // The pans -------------------------------------------------------------
    {"chownder", "Chownder", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Thick, hot, and it belongs to whoever is holding the bowl.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 60}, {IE::Kind::Happy, 15}}, /*combatHeal=*/35},

    {"cronsomme", "Cronsomme", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Perfectly clear, and served on the quarter hour whether you came or not.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::Frag, -18}}},

    {"wanton_soup", "WANton Soup", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Each one wrapped for a long journey, and most of them survive it.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 40}, {IE::Kind::Happy, 12}}, /*combatHeal=*/20},

    {"piperogi", "Piperogi", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Each one feeds straight into the next one along the plate.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 45}, {IE::Kind::Happy, 8}}},

    {"queuesadilla", "Queuesadilla", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "First one in is the first one out, and it has gone cold waiting.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 40}, {IE::Kind::Happy, 15}}},

    {"ravioli_code", "Ravioli Code", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Every piece sealed, self-contained, and impossible to tell apart.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 50}, {IE::Kind::Frag, -12}}},

    {"idleys", "Idleys", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Doing nothing at all, and using nothing to do it.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::Frag, -15}}},

    {"ms_dosa", "MS-Dosa", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Thin, crisp, and older than everybody at the table.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 55}, {IE::Kind::Happy, 15}}},

    {"arpas", "ARPas", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "It goes round the table asking who has what.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Happy, 10}}},

    {"kafkofta", "Kafkofta", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "They arrive in order, and you can have the whole lot again.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 60}, {IE::Kind::Happy, 10}}, /*combatHeal=*/30},

    {"kernel_panini", "Kernel Panini", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Everything stopped the moment it was pressed. Nothing since.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 55}, {IE::Kind::Happy, 20}}, /*combatHeal=*/30},

    // EPIC — a race is decided by who gets there first, which is what a Speed point buys.
    {"racelette", "Racelette", ItemDef::Type::Food, ItemDef::Rarity::Epic,
     "Two of you scraping at the same pan. Whoever gets there first. "
     "Once per pet: +{speed} SPEED for life.",
     ItemDef::Context::Anytime,
     {{IE::Kind::StatPointSpeed, 1}, {IE::Kind::Hunger, 50}, {IE::Kind::Happy, 25}}},

    {"scrambled_regeggs", "Scrambled RegEggs", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Thoroughly scrambled. Nobody is unscrambling that.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Happy, 10}}},

    {"char_grilled_array", "Char-Grilled Array", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "A skewer of fixed width. The last one on it is always empty.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 45}, {IE::Kind::Happy, 8}}},

    {"tarballs", "Tarballs", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Everything that was on the counter, compressed into one.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 50}, {IE::Kind::Happy, 5}}},

    {"bashed_potatoes", "Bashed Potatoes", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Hit until they did what they were told.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 45}, {IE::Kind::Happy, 10}}},

    {"onion_rings", "Onion Rings", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Three layers, and not one of them knows who placed the order.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::Happy, 20}}},

    {"flash_fried_chips", "Flash-Fried Chips", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "In and out of the oil so fast the pan never noticed.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Happy, 18}}},

    {"twisted_pairetzels", "Twisted Pairetzels", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Wound around each other so tightly that neither picks up the other's noise.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 25}, {IE::Kind::Happy, 18}}},

    {"jitter_fritters", "Jitter Fritters", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "They arrive. Just never quite when you expected them to.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Happy, 12}}},

    {"shashimi", "SHAshimi", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Sliced thin, one direction only. Nothing is going back.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 45}, {IE::Kind::Happy, 25}, {IE::Kind::Frag, -15}}},

    // EPIC — a spare in the array is the whole idea of a Defence point: one more thing
    // that has to fail before anything is actually lost.
    {"spare_ribs", "Spare RIBs", ItemDef::Type::Food, ItemDef::Rarity::Epic,
     "Keep a copy. You will want to know how you got here. "
     "Once per pet: +{defense} DEFENSE for life.",
     ItemDef::Context::Anytime,
     {{IE::Kind::StatPointDefense, 1}, {IE::Kind::Hunger, 65}, {IE::Kind::Happy, 15}},
     /*combatHeal=*/30},

    {"rested_steak", "RESTed Steak", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Stateless. Every bite stands entirely on its own.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 60}, {IE::Kind::Happy, 20}}, /*combatHeal=*/35},

    // EPIC — the pet keeps a Power point for life. The pun is the mechanic: an escalation
    // is not a thing you do twice, so the first plate roots it and later ones are veal.
    {"privilege_escalope", "Privilege Escalope", ItemDef::Type::Food,
     ItemDef::Rarity::Epic,
     "Ordered the veal. Came back with the run of the kitchen. "
     "Once per pet: +{power} POWER for life.",
     ItemDef::Context::Anytime,
     {{IE::Kind::StatPointPower, 1}, {IE::Kind::Hunger, 55}, {IE::Kind::Happy, 30}}},

    {"force_pulled_pork", "Force-Pulled Pork", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "It was not ready. It came apart anyway.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 70}, {IE::Kind::Frag, 10}}, /*combatHeal=*/40},

    {"rubber_duck_confit", "Rubber Duck Confit", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "You explained the whole problem to it and it said nothing at all.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 55}, {IE::Kind::Happy, 30}, {IE::Kind::Frag, -20}}},

    {"vacuum_sealed_leftovers", "Vacuum-Sealed Leftovers", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Sealed up and the space nobody was using handed straight back.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 60}, {IE::Kind::Frag, -25}}},

    {"disk_platter", "Disk Platter", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "It spins, everybody reaches in, and it is somehow always your turn.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 65}, {IE::Kind::Happy, 20}}},

    {"serverless_platter", "Serverless Platter", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Nobody brought it out. It is simply there when you want it.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 55}, {IE::Kind::Happy, 25}}, /*combatHeal=*/25},

    {"pickle_jar", "Pickle Jar", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Whatever went in is what comes out, and you had better hope you packed it.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 20}, {IE::Kind::Happy, -5}, {IE::Kind::Frag, -12}}},

    // The condiments — small effects, but they are what the big plates are built on.
    {"ai_oli", "AI-oli", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "It came up with the recipe itself. Mostly garlic. Confidently.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 10}, {IE::Kind::Happy, 15}}},

    {"vinaigrette", "Vi-naigrette", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Nobody at this table can work out how to put the lid back on.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 12}, {IE::Kind::Frag, -8}}},

    {"malwarmalade", "Malwarmalade", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Spreads. Keep it away from the other jars.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 12}, {IE::Kind::Happy, 18}, {IE::Kind::Frag, 5}}},

    {"signal_jam", "Signal Jam", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Thick enough that nothing gets through it.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 12}, {IE::Kind::Happy, 16}}},

    // The puddings --------------------------------------------------------
    {"pop3sicle", "POP3sicle", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "You get it once. It is not on the tray any more.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 15}, {IE::Kind::Happy, 30}}},

    {"mergingue", "Mergingue", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Two of them folded together with no seam left anywhere.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 15}, {IE::Kind::Happy, 32}}},

    {"declair", "Declair", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Has to be announced before anyone is allowed to use it.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 20}, {IE::Kind::Happy, 30}}},

    // EPIC — the one Epic dish that grants no stat at all. A profiler doesn't make the
    // pet stronger, it makes every hour it spends teach it more, which is an XP rate.
    {"profilerole", "Profilerole", ItemDef::Type::Food, ItemDef::Rarity::Epic,
     "Small, rich, and afterwards you know exactly where the time went. "
     "Once per pet: +{xpRate}% XP for life.",
     ItemDef::Context::Anytime,
     {{IE::Kind::XpRateBonusPct, 25}, {IE::Kind::Hunger, 25}, {IE::Kind::Happy, 38},
      {IE::Kind::Frag, -10}}},

    {"coboler", "COBOLer", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Nobody has touched the recipe in fifty years. It still comes out.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 50}, {IE::Kind::Happy, 25}}},

    {"clustard", "Clustard", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Three jugs of it. One goes over and nobody at the table notices.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 25}, {IE::Kind::Happy, 22}}},

    {"bashlava", "Bashlava", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Layer calls layer calls layer, all the way down to the syrup.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 40}, {IE::Kind::Happy, 40}, {IE::Kind::Frag, 8}}},

    {"deflated_souffle", "Deflated Souffle", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "It was twice this size before all the air came out of it.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::Happy, 12}}},

    {"fork_bombe", "Fork Bombe", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Cut it and there are two. Cut those and there are four.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 45}, {IE::Kind::Happy, 35}, {IE::Kind::Frag, 12}}},

    {"optical_mousse", "Optical Mousse", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "No wires anywhere in it. Stops dead on a shiny plate.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 25}, {IE::Kind::Happy, 40}, {IE::Kind::Frag, -12}}},

    {"cherry_picked_tart", "Cherry-Picked Tart", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "You took the one you wanted and left the rest of the branch.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::Happy, 42}}},

    {"raspberry_pie", "Raspberry Pie", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Cheap, tiny, endlessly useful, and there are four more in the drawer.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Happy, 25}}},

    {"rainbow_tablet", "Rainbow Tablet", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Every answer worked out in advance, set hard, and cut into squares.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 20}, {IE::Kind::Happy, 45}, {IE::Kind::Frag, 10}}},

    {"mint_choc_chip", "Mint Choc Chip", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "A perfectly good distribution, with silicon through it.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 25}, {IE::Kind::Happy, 38}, {IE::Kind::Frag, -8}}},

    {"candied_yamls", "Candied YAMLs", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Delicious, and one wrong space in the tray ruins the tray.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 40}, {IE::Kind::Happy, 20}}},

    // What you drink with them --------------------------------------------
    {"flat_white", "Flat White", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "No structure, no schema, one long pour.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 28}, {IE::Kind::Frag, -12}}},

    {"mockachino", "Mockachino", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "It only pretends to be coffee, and it is convincing enough.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 8}, {IE::Kind::Happy, 26}, {IE::Kind::Frag, -8}}},

    {"blockchai", "Blockchai", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Every cup depends on the one before it, and nobody can pour it again.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 10}, {IE::Kind::Happy, 35}, {IE::Kind::Frag, -18}}},

    {"syn_ack_shake", "SYN-ACK Shake", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "You offer, it offers back, you agree. Then you drink it.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 25}, {IE::Kind::Happy, 25}}},

    // EPIC — the pet's max Health IS its buffer, and this is the drink that writes past
    // the end of it. The Fragmentation it adds is the cost of taking the extra room.
    {"buffer_overfloat", "Buffer Overfloat", ItemDef::Type::Food,
     ItemDef::Rarity::Epic,
     "They kept pouring after the glass was full. It went everywhere. "
     "Once per pet: +{maxhp} MAX-HP for life.",
     ItemDef::Context::Anytime,
     {{IE::Kind::StatPointHealth, 1}, {IE::Kind::Hunger, 30}, {IE::Kind::Happy, 40},
      {IE::Kind::Frag, 15}}},

    {"hard_cidr", "Hard CIDR", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Comes by the block. You do not get to choose how big a block.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 10}, {IE::Kind::Happy, 28}, {IE::Kind::Frag, 8}}},

    {"port_80", "Port 80", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Fortified, and served open to absolutely anyone who asks.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 12}, {IE::Kind::Happy, 42}, {IE::Kind::Frag, 10}}},

    {"fizzbuzz", "FizzBuzz", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Every third sip fizzes, every fifth buzzes. Nobody orders it twice.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 8}, {IE::Kind::Happy, 24}}},

    {"punchcard_punch", "Punchcard Punch", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Drop the tray and you are starting the evening again.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 15}, {IE::Kind::Happy, 26}}},

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
     // The Yubi-Cookie is a COOKIE: it is eaten, it fills the pet up, and the pet is
    // happier for it every single time. What it does once in a life is forget a care
    // mistake — the same shape the Epic dishes take, a very good meal with one
    // permanent thing folded into it, which is why it is filed with the food rather
    // than with the buffs it arms none of.
    {"yubi_cookie", "Yubi-Cookie", ItemDef::Type::Food,
     ItemDef::Rarity::Epic, "So delicious it could make the pet forget {mistakes} care mistake. Max 1 per lifecycle.",
     ItemDef::Context::Anytime,
     {{IE::Kind::RemoveCareMistakeOnce, 1}, {IE::Kind::Hunger, 20},
      {IE::Kind::Happy, 40}}},
    
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
    
    // Rollback: a TOOL. It arms nothing and waits for nothing — Use opens a stat picker
    // (use=Rollback) and the shed lands the moment it is confirmed, which is the line
    // between the two tabs: a Buff is a thing the pet is now CARRYING, a Tool is a thing
    // the operator just DID. Sheds one earned combat-stat point (-1 that stat, -1 level)
    // so the pet re-grinds that level and re-rolls a fresh +1. A reward-pool drop; inert
    // at level 0 (nothing to shed), and it can never reach an off-level point an Epic
    // dish granted (core/model/pet_upgrades.h).
    {"rollback", "Rollback", ItemDef::Type::Quest,
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

    // --- The rest of the USB family ------------------------------------------------
    // Four devices that all steer the SAME boundary the Ambig-USB does — the pet's next
    // evolution — which is what makes them one family and why the soak pair below can
    // lock the port against the other three (defs.h's isUsbEffect). All but the
    // Hypervisor are DeepWeb Dive drops (areas/deepweb_dive/area.cpp): no counter sells
    // the ability to overrule how a pet was raised, so the only way to hold one is to go
    // down and take it.

    // Bad-USB: the firmware attack the name comes from, and the item that does to a pet
    // what it does to a host — the branch is decided by the DEVICE, not by the record.
    // Forces the BAD successor at the next branching evolution however clean the care
    // budget was. Deliberately scarcer than its Epic tier-mates (dropWeight): a run's
    // ending is the one thing the care loop is FOR, so buying your way past it should
    // cost a real trip down.
    {"bad_usb", "Bad-USB", ItemDef::Type::Buff,
     ItemDef::Rarity::Epic,
     "Forces the next branching evolution down the BAD line, whatever the care record.",
     ItemDef::Context::Anytime, {{IE::Kind::ForceEvolveBranchBad, 1}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/1},

    // Signed-USB: the same device with a vendor signature on its firmware, and the exact
    // inverse item — the GOOD successor whatever the record says, which is the half that
    // rescues a badly-raised pet rather than the half that ruins a well-raised one. Same
    // one slot as the Bad-USB: plugging either in replaces the other.
    {"signed_usb", "Signed-USB", ItemDef::Type::Buff,
     ItemDef::Rarity::Epic,
     "Forces the next branching evolution down the GOOD line, whatever the care record.",
     ItemDef::Context::Anytime, {{IE::Kind::ForceEvolveBranchGood, 1}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/1},

    // Sandbox-USB: hold the process under observation instead of letting it run to term.
    // A Process-stage-only plug that stretches THIS stage's evolution dwell by {soak} and
    // pays {soak} times the XP for everything the pet does while it is stretched — the
    // same pet, arriving later and further along, which is the whole trade. It holds the
    // port shut while it runs (defs.h's isUsbEffect), so a soak is a decision about the
    // stage rather than one buff among several: no divert, no branch override, not even a
    // second soak, until this one is spent at the boundary it stretched.
    {"sandbox_usb", "Sandbox-USB", ItemDef::Type::Buff,
     ItemDef::Rarity::Rare,
     "Process-use: stretches this stage's evolve clock x{soak} and pays x{soak} XP.",
     ItemDef::Context::Anytime, {{IE::Kind::ArmEvolveSoak, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/3},

    // Hypervisor-USB: the Sandbox-USB's Epic upgrade — one layer further down, so the
    // soak is twice as deep AND it reaches a stage the Sandbox cannot. On a Script it
    // still pays x{soak} XP but the clock costs DOUBLE that, because a Script's boundary
    // is the branch the whole raise was aimed at: stretching the last stage before an
    // ending is worth more than stretching a middle one, so it is priced to match.
    // The only USB anyone sells (Moor-to-Moor, Napstorrent Moors), and it is priced in
    // its own family: four Sandbox-USBs plus Bits, so the deep end of the ladder is
    // reached by diving for the rare one four times over rather than by having a wallet.
    {"hypervisor_usb", "Hypervisor-USB", ItemDef::Type::Buff,
     ItemDef::Rarity::Epic,
     "Process/Script: x{soak} XP for x{soak} the evolve clock, x2 that on a Script.",
     ItemDef::Context::Anytime, {{IE::Kind::ArmEvolveSoakLate, 4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/2048},

    // Halt-USB: the pet stops evolving. Not a stretch of the clock — a refusal to reach
    // the boundary at all, and the only device in the family that is never consumed,
    // because no boundary arrives to consume it. It comes out when an Eject-USB pulls it,
    // when the pet goes back on the ARCH rack, or never. What it is FOR is parking a pet
    // at a stage you want it at: the roster has thirty-five species and only sixteen of
    // them are endings, so keeping one of each means keeping the middle of the chains.
    {"halt_usb", "Halt-USB", ItemDef::Type::Buff,
     ItemDef::Rarity::Rare,
     "Stops the pet evolving at all, until an Eject-USB pulls it.",
     ItemDef::Context::Anytime, {{IE::Kind::ArmEvolveHold, 0}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/6},

    // Eject-USB: pull whatever is in the port and drop its effect, whichever device it
    // was. The family's undo, and the one thing that goes in while a soak or a hold is
    // already there — a port that could only be emptied by the boundary it was refusing
    // to reach would be a trap rather than a decision. Drawn commoner than the devices it
    // undoes (dropWeight), for the same reason.
    {"eject_usb", "Eject-USB", ItemDef::Type::Buff,
     ItemDef::Rarity::Rare,
     "Pulls whatever USB is armed and drops its effect.",
     ItemDef::Context::Anytime, {{IE::Kind::ClearUsbPort, 0}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/8},

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
