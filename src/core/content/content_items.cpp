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
// sentence mentions it — so a description is flavour, not explanation. Its voice and
// rules are CONTENT_STANDARD.md's *Description voice*.
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
const LootEntry kCachePoolEpic[]     = {{"rollback"}, {"repartition"}, {"yubi_cookie"},
                                        {"backup_drive"}, {"restore_point"},
                                        {"deep_learning_core"}, {"zeroday_bell"}};
// The share of the commendation pool one PALETTE CHIP draws at. Six of them against
// seven consumables whose own weights sum to ~55, so a first commendation is about as
// likely to pay a colour set as a prize and a device that has them all is back to the
// pool it started with. Lives here rather than in tunables.h because it is a fact about
// this one pool's balance, not a cross-cutting one.
constexpr int kChipDrawWeight = 6;

// The Commendation Cache's own pool: what an achievement pays out. Deliberately not
// the Epic pool — a commendation is earned by playing a whole ladder out, so it hands
// over the scarce per-lifetime shields and the deepest diving bells rather than
// re-rolling the same walk consumables a found cache already gives.
// ...plus the PALETTE CHIPS, the one thing in here that is not a consumable: each
// unlocks a colour set for good (content_themes.h), so a chip is worth a draw exactly
// once and then stops being loot at all — rollLootEntry drops one whose set is already
// unlocked to weight 0 rather than letting it cost a commendation a real prize. At
// kChipDrawWeight they are a bit under half the pool while any remain and none of it
// once they are all in, which is the shape a finite set of unlocks should have: a
// reason to open the next one, and never a tax on the twentieth.
const LootEntry kCachePoolCommend[]  = {{"restore_point"}, {"yubi_cookie"},
                                        {"deep_learning_core"}, {"zeroday_bell"},
                                        {"kernel_bell"}, {"ambig_usb"}, {"rollback"},
                                        {"repartition"},
                                        {"sunset_rom", kChipDrawWeight},
                                        {"phosphor_tube", kChipDrawWeight},
                                        {"amber_tube", kChipDrawWeight},
                                        {"daylight_filter", kChipDrawWeight},
                                        {"pocket_lcd", kChipDrawWeight},
                                        {"redshift_lens", kChipDrawWeight}};
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
    {"pwnzu_sauce"}, {"backup_drive"}, {"rollback"}, {"repartition"}, {"osi_dip"},
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
     ItemDef::Rarity::Common, "Turns a ransom note into a polite suggestion.",
     ItemDef::Context::LockoutOnly, /*effects=*/{}, /*combatHeal=*/0,
     /*preEncounterXp=*/0, /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None,
     /*use=*/ItemDef::Use::Consume, /*category=*/ItemDef::Category::Keys},
    
     // the Defrag Tool — a common consumable spent by a TOOL DEFRAG in MAINT
    // for a GUARANTEED clean (no fail roll). Never used from the ITEMS path (gated in
    // itemUsable); sold at the shop + drops from wild loot so it's obtainable. No new
    // glyph — reuses the MAINT defrag icon (itemIcon fallback).
    {"disk_scrubber", "Defrag Tool", ItemDef::Type::Quest,
     ItemDef::Rarity::Common, "Puts scattered pieces back right next to their neighbours.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0,
     /*preEncounterXp=*/0, /*bits=*/14, /*walkWarp=*/ItemDef::WalkWarp::None,
     /*use=*/ItemDef::Use::GuaranteeDefrag},

    // Null Noodles: de-frags, makes the pet HUNGRIER, and pulls
    // Happiness toward 50%.
    {"null_noodles", "Null Noodles", ItemDef::Type::Food,
     ItemDef::Rarity::Common,
     "It tastes like nothing.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, -15}, {IE::Kind::Frag, -15}, {IE::Kind::HappyToward50, 20}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/5},

    // Boot Accelerator: the egg accelerator, in the starting kit and stocked at
    // Pier-to-Peer. Use on a Boot-Sector egg takes its CutIncubationMin off the
    // incubation, floored at the crackable window — every line's hatch minigame is
    // played at lay-time, so there is nothing left for an item to open. Quest-typed
    // (falls through the egg-phase ITEMS gate), no vitals, egg-only.
    {"boot_accelerator", "Boot Accelerator", ItemDef::Type::Quest,
     ItemDef::Rarity::Common,
     "Skips the slow start-up so you can say hello sooner.",
     ItemDef::Context::Anytime, {{IE::Kind::CutIncubationMin, 10}},
     /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::DecryptEgg,
     /*category=*/ItemDef::Category::Keys},

    // Decryptogram: a found ticket to one DECRYPTOGRAM board (content_quotes.h). Cashed
    // in at the Hacker VAULT, never from ITEMS — the prize is a player-level account
    // unlock, so it is spent where the other things you cash in are, and itemUsable
    // gates the pet path with "CASH IN AT VAULT (A+C)" the way it does a sealed cache.
    // Priceless on purpose: no storefront sells one, so the pool only drains as fast as
    // the walk hands them over.
    {"decryptogram", "Decryptogram", ItemDef::Type::Quest,
     ItemDef::Rarity::Uncommon,
     "Each letter is standing in for a different one. Work out which.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None,
     /*use=*/ItemDef::Use::PlayCryptogram, /*category=*/ItemDef::Category::Keys},

     // A random reward that doesn't have to come from the source zone's drop table.
     // The four rarity caches' findWeight values are the walk's cache-find distribution
     // and sum to 100, so a weight reads directly as a percentage.
     {"sealed_cache_common", "Common Cache", ItemDef::Type::Quest,
     ItemDef::Rarity::Common,
     "Kept close at hand in case it's needed again.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::OpenContainer,
     /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/0, /*cache=*/{/*bits=*/10, /*draws=*/1, /*drawChancePct=*/100,
                kCachePoolCommon, poolN(kCachePoolCommon), /*findWeight=*/50}},

    // Backdoor/Rootkit/Kernel/Checkpoint Bell: a diving bell lowers you straight to a
    // depth instead of swimming down — these arm the NEXT DeepWeb Dive to skip
    // straight to a given depth (SetDeepWebStartDepth(ToBest)), consumed the moment
    // that dive starts, so a pet re-earns its way back to a genuine struggle
    // without re-walking every shallow depth first.
    {"backdoor_bell", "Backdoor Bell", ItemDef::Type::Tool,
     ItemDef::Rarity::Common, "A hidden way in that never bothers with the usual checks.",
     ItemDef::Context::Anytime, {{IE::Kind::SetDeepWebStartDepth, 16}, {IE::Kind::DiveStartBonusPct, 25}}},

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
     "Spun up fresh for dinner and gone again in seconds.",
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
     "Cuts the strands loose so they stop clinging to each other.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::ClearReplicationGhost, 0}}, /*combatHeal=*/20},

    // Intended to be combined with Null Noodles to produce a rare food
    {"pwnzu_sauce", "Pwnzu Sauce", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "One drop takes over the whole dish.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 15}}, /*combatHeal=*/0},
    
    {"tortilla_chip", "Tor-Tilla Chip", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon, "Crunchy and very hard to trace back to the bag it came from.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, 10}}},
 
    {"osi_dip", "OSI Dip", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon, "Seven layers deep and still missing a chip to scoop it with.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, 10}}},

     // Sinkhole Trap: bypass the next wild encounter for a flat XP lump (preEncounterXp,
    // applied by game_combat::resolveSinkhole — a hand-off, not an on-Use pet effect).
    {"sinkhole_trap", "Sinkhole Trap", ItemDef::Type::Quest,
     ItemDef::Rarity::Uncommon, "Trouble headed your way gets quietly sent nowhere.",
     ItemDef::Context::PreEncounter, /*effects=*/{}, /*combatHeal=*/0,
     /*preEncounterXp=*/40},
    
     {"r007_b33r", "R007_B33R", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Fizzy and sugary. A favourite of the up-past-midnight crowd.",
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
     "Stashed away so long ago it's probably gone stale.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
    /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::OpenContainer,
     /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/0, /*cache=*/{kLootBitsReward, /*draws=*/1, /*drawChancePct=*/kLootItemChancePct,
                kLootPool, poolN(kLootPool), /*findWeight=*/0}},

    {"sealed_cache_uncommon", "Uncommon Cache", ItemDef::Type::Quest,
     ItemDef::Rarity::Uncommon,
     "Still fresh. A good one to find.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::OpenContainer,
     /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/0, /*cache=*/{/*bits=*/18, /*draws=*/1, /*drawChancePct=*/100,
                kCachePoolUncommon, poolN(kCachePoolUncommon), /*findWeight=*/30}},

    // Rootkit Bell: the Backdoor Bell's deeper cousin — see its comment above.
    {"rootkit_bell", "Rootkit Bell", ItemDef::Type::Tool,
     ItemDef::Rarity::Uncommon, "Gets in below the level a normal check reaches.",
     ItemDef::Context::Anytime, {{IE::Kind::SetDeepWebStartDepth, 32}, {IE::Kind::DiveStartBonusPct, 50}}},
    //
    // STAPLE INGREDIENTS --------------------------
    // The pantry: the raw materials recipes are built out of. Grouped together rather
    // than filed under the rarity headings above because what makes one of these
    // findable is being a STAPLE, not its tier — the tier only says how much a pet
    // gets out of eating one raw, which is generally "not much, and sometimes less
    // than nothing". Most carry an explicit dropWeight so the pantry isn't a flat
    // shelf: what every kitchen is full of (Spam, Breadcrumbs) turns up far more often
    // than the scarce things (Root Veg, Fresh Macrol).
    //
    {"spam", "Spam", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Delivered in bulk whether you subscribed or not.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 2}}, /*combatHeal=*/0,
     /*preEncounterXp=*/0, /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None,
     /*use=*/ItemDef::Use::Consume, /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/90},

    {"breadcrumbs", "Breadcrumbs", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Drop a few as you go and you'll always find your way back.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 1}}, /*combatHeal=*/0,
     /*preEncounterXp=*/0, /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None,
     /*use=*/ItemDef::Use::Consume, /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/80},

    {"c_salt", "C-Salt", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Old as the hills and still in most of what you eat.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, -2}}, /*combatHeal=*/0,
     /*preEncounterXp=*/0, /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None,
     /*use=*/ItemDef::Use::Consume, /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/70},

    {"grepsed_oil", "Grep-sed Oil", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "One quick swipe through the pan finds the dry spots.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, -5}, {IE::Kind::Frag, -5}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/70},

    {"spoiled_macrol", "Spoiled Macrol", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Smells a bit phishy. Best not to open it.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, -15}}, /*combatHeal=*/0,
     /*preEncounterXp=*/0, /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None,
     /*use=*/ItemDef::Use::Consume, /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/60},

    {"cronstarch", "Cronstarch", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Thickens right on schedule.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 1}, {IE::Kind::Happy, -1}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/55},

    // Boolean Cubes take no dropWeight — Common's own default is exactly the middle
    // of this shelf, which is where they belong.
    {"boolean_cubes", "Boolean Cubes", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Either it's in the stock or it isn't.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, -5}}},

    {"vanilla_extract", "Vanilla Extract", ItemDef::Type::Food,
     ItemDef::Rarity::Common,
     "The flavour you start with before getting fancy.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 1}, {IE::Kind::Happy, 1}, {IE::Kind::Frag, 1}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/45},

    // Polltatoes: the one stacking food (ItemEffect::Kind::HungerStacking). Eaten
    // alone it is a Breadcrumb; eaten in a run it climbs, and the pet's next passive
    // Hunger-decay tick ends the run.
    {"polltatoes", "Polltatoes", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Keep checking the pot. They'll be done eventually.",
     ItemDef::Context::Anytime, {{IE::Kind::HungerStacking, 1}}, /*combatHeal=*/0,
     /*preEncounterXp=*/0, /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None,
     /*use=*/ItemDef::Use::Consume, /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/45},

    {"regeggs", "RegEggs", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Whip them into the pattern you want.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 8}}, /*combatHeal=*/0,
     /*preEncounterXp=*/0, /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None,
     /*use=*/ItemDef::Use::Consume, /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/40},

    {"data_leek", "Data Leek", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Your neighbours seem to have a bunch of these lately.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, 3}, {IE::Kind::Frag, -3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/35},

    {"universal_cereal_box", "Universal Cereal Box", ItemDef::Type::Food,
     ItemDef::Rarity::Common,
     "Fits the bowls in your house and your nan's too.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 10}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/30},

    {"java", "Java", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Takes a while to get going in the morning but then it runs all day.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, 5}, {IE::Kind::Frag, -2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/35},

    // Kernel Oil takes Uncommon's own default weight — the middle of its shelf.
    {"kernel_oil", "Kernel Oil", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Works its way right down to the bottom of the pan.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, -5}, {IE::Kind::Frag, -5}}},

    {"syntactic_sugar", "Syntactic Sugar", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Makes the medicine easier to swallow without changing it.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 1}, {IE::Kind::Happy, 15}, {IE::Kind::Frag, 3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/25},

    {"applets", "Applets", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "They used to be in school lunchboxes. You hardly see them now.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, 3}, {IE::Kind::Frag, -5}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/20},

    {"root_veg", "Root Veg", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "You need special access before you're allowed to dig these up.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 6}, {IE::Kind::Frag, -6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/15},

    {"fresh_macrol", "Fresh Macrol", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Lovely straight off the boat. Just don't leave it lying around.",
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
     "The same old seasoning with all the risky bits taken out.",
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
     "Its only reference is its own say-so. Trust it if you like.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 2}, {IE::Kind::Happy, -3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/65},

    {"shellots", "Shellots", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Peel back one layer and there's always another underneath.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 2}, {IE::Kind::Happy, -4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/60},

    {"linkguine", "Linkguine", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Best shared. Send it to a friend.",
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
     "Hot enough to keep you locked in your seat.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, 4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/55},

    {"churned_butter", "Churned Butter", ItemDef::Type::Food,
     ItemDef::Rarity::Common,
     "Worked over and over until it comes out exactly the same.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Frag, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/50},

    {"bytesteak_tomatoes", "Bytesteak Tomatoes", ItemDef::Type::Food,
     ItemDef::Rarity::Common,
     "They come in bunches of eight. Always eight.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/50},

    {"gherkins", "Gherkins", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Given a jar, when you open it, then you get pickles.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, -3}, {IE::Kind::Frag, -2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/45},

    {"cruds", "CRUDs", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Make them, check on them, tweak them and then mostly throw them out.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, -2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/40},

    // Bootmeal cooks into Portridge, which is the same bowl at the same tier for the
    // same numbers — the one recipe in the kitchen that changes nothing but where it
    // runs. The pair only reads as a joke because these effects and this rarity are
    // literally the ones on that dish's row.
    {"bootmeal", "Bootmeal", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Breakfast comes first. The rest of the day can wait till it's done.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 10}, {IE::Kind::Happy, -5}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/40},

    {"garlic_escapes", "Garlic Escapes", ItemDef::Type::Food,
     ItemDef::Rarity::Common,
     "Handle them carefully or the whole dish falls apart.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, -6}, {IE::Kind::Frag, -4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/35},

    {"grepefruit", "Grepefruit", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Only the segments that match are worth eating.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, -4},
                                 {IE::Kind::Frag, -4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/30},

    {"red_herring", "Red Herring", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Looks important. Isn't.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, -5}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/25},

    {"papaya", "PAPaya", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Hands over its secrets the moment you ask.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, 6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/30},

    {"mozillarella", "Mozillarella", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Stretches as far as you need and you're free to make your own.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 6}, {IE::Kind::Happy, 4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/25},

    {"imaple_syrup", "IMAPle Syrup", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Pour as much as you like and there's still plenty left in the bottle.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 2}, {IE::Kind::Happy, 12},
                                 {IE::Kind::Frag, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/20},

    {"double_precision_cream", "Double-Precision Cream", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Twice as thick and accurate to the last drop.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 7}, {IE::Kind::Frag, 3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/18},

    {"cocoa", "Cocoa", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "A rich old favourite that only really grows in one company's orchard.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, 10}, {IE::Kind::Frag, -3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/15},

    {"rubber_ducks", "Rubber Ducks", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Explain your recipe to one before you start. They're great listeners.",
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
     "Left out on purpose to see who helps themselves.",
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
     "You have to work through them bit by bit before they make sense.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, -3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/62},

    {"romaine", "ROMaine", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Look all you like but you can't change a thing about it.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 2}, {IE::Kind::Frag, -3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/58},

    {"bitroot", "Bitroot", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Leave it in storage too long and it slowly goes bad on its own.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, -4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/56},

    {"swiss_chard", "Swiss Chard", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Split it into pieces and spread them around the plate.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Frag, -3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/54},

    {"string_beans", "String Beans", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "A long line of them where the last one is always empty.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/52},

    {"snap_peas", "Snap Peas", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "A pod packs its own seeds and grows wherever you plant it.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, 3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/50},

    {"squash", "Squash", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "A whole day's serve of vegetables in a single veggie patch.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 7}, {IE::Kind::Happy, -2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/48},

    {"raidicchio", "RAIDicchio", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Lose a leaf and there's a spare ready to take its place.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, -5},
                                 {IE::Kind::Frag, -3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/46},

    {"awkra", "AWKra", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Slices neatly into columns.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, -4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/44},

    {"kaliflower", "Kaliflower", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Comes with a full kit of tools already in the box. Mostly sharp ones.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Frag, -2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/42},

    {"archichoke", "Archichoke", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Takes forever to get to the good bit but people swear it's worth it.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 2}, {IE::Kind::Happy, -6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/38},

    {"flatpak_choi", "Flatpak Choi", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Brings its own seasoning so it works in your kitchen or mine.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/36},

    {"capsicum", "CAPsicum", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Sweet, crunchy, cheap. Pick two.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, 4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/34},

    {"peppermint", "Peppermint", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Light and fresh. Runs well even on an old stomach.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, 6}, {IE::Kind::Frag, -2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/32},

    {"nixtamal", "Nixtamal", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Made exactly the same way each time down to the last kernel.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 6}, {IE::Kind::Happy, -3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/30},

    {"pingapple", "Pingapple", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Toss one over and it always comes back.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, 6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/28},

    {"plaintain", "Plaintain", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Sitting right out in the open for the whole street to see.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 7}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/26},

    {"cloudberries", "Cloudberries", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Grown on another farm and shipped in when you need them.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 2}, {IE::Kind::Happy, 7}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/24},

    {"lintils", "Lintils", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "A handful of small reminders that you could be doing better.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 6}, {IE::Kind::Happy, -4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/44},

    {"perl_barley", "Perl Barley", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Easy to cook up. Impossible to read the recipe afterwards.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 6}, {IE::Kind::Frag, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/40},

    {"basicmati_rice", "BASICmati Rice", ItemDef::Type::Food,
     ItemDef::Rarity::Common,
     "Simple enough for a beginner who follows it line by line.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 8}, {IE::Kind::Happy, -2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/48},

    {"vpenne", "VPenne", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Little tunnels that keep their filling private.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Frag, -2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/46},

    {"unmonitored_oats", "Unmonitored Oats", ItemDef::Type::Food,
     ItemDef::Rarity::Common,
     "Leave them overnight and hope it goes well.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 6}, {IE::Kind::Happy, -6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/38},

    {"yamls", "YAMLs", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Cut them at the wrong depth and the dish turns into a different meal.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 7}, {IE::Kind::Happy, -5}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/36},

    {"epoch_dates", "Epoch Dates", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "All dated from the same day back in 1970.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 8}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/28},

    {"dotfigs", "Dotfigs", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Your friends arrange theirs a little differently and swear theirs is "
     "best.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, 9}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/26},

    {"apiricot", "APIricot", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Ask nicely and you'll get some. Ask too often and you'll have to wait.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, 7}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/24},

    {"raspberry_pis", "Raspberry Pis", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "You'll buy a punnet for a project and forget about most of them.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, 8}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/22},

    {"table_grapes", "Table Grapes", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Neatly arranged in rows. Easy to look up the one you want.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, 6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/20},

    {"lambda_chops", "Lambda Chops", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Small cuts that do one job and then they're gone.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 10}, {IE::Kind::Happy, 3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/0},

    {"file_mignon", "File Mignon", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Small and precious. Make a copy before you cook it.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 9}, {IE::Kind::Happy, 6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/0},

    {"minified_beef", "Minified Beef", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Trimmed of fat until it's barely recognisable.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 9}, {IE::Kind::Frag, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/20},

    {"saasage", "SaaSage", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "You don't buy it outright. You pay a little every month forever.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 8}, {IE::Kind::Happy, 4},
                                 {IE::Kind::Frag, 3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/22},

    {"packed_sardines", "Packed Sardines", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Squeezed in with no room to spare between them.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 9}, {IE::Kind::Happy, -3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/24},

    {"natto", "NATto", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Sticky enough to cling to the bowl and still slip through the gaps.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 7}, {IE::Kind::Happy, -8},
                                 {IE::Kind::Frag, -5}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/16},

    {"paramesan", "Paramesan", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Sprinkle on as much as you like. Leave it off and it's still fine.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, 7}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/0},

    {"macadamia", "MACadamia", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "A unique mark on each shell. Easy enough to fake though.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 7}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/18},

    {"cache_ews", "Cache-ews", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Keep a bowl close by and you'll never have to go to the shop.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/20},

    {"chia_seeds", "Chia Seeds", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Takes up a lot of room in the cupboard and gives you very little back.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 2}, {IE::Kind::Frag, -2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/34},

    {"cinnamon", "Cinnamon", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Warm and friendly. Makes your kitchen look nicer just by being there.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, 7}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/32},

    {"squid_ink", "Squid Ink", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Stains the pasta it passes through.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, -6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/16},

    {"leaf_node_tea", "Leaf-Node Tea", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Picked from the very tips of the branches.",
     ItemDef::Context::Anytime, {{IE::Kind::Happy, 9}, {IE::Kind::Frag, -6}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/22},

    {"mixins", "Mixins", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Stir a little into your drink and it picks up the flavour.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 1}, {IE::Kind::Happy, 5}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/42},

    {"silicon_wafers", "Silicon Wafers", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Thin and crisp and worth more than they look.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 3}, {IE::Kind::Happy, 8}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/0},

    {"nibbles", "Nibbles", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Half a bite each. Two make a byte.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 4}, {IE::Kind::Happy, 4}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/40},

    {"marshalled_mallows", "Marshalled Mallows", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Flattened down so they travel well. Puff them back up when they "
     "arrive.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 2}, {IE::Kind::Happy, 11},
                                 {IE::Kind::Frag, 3}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/0},

    {"humbugs", "Humbugs", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "There's always one more in the bag than you thought.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 1}, {IE::Kind::Happy, 8},
                                 {IE::Kind::Frag, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/30},

    {"burp_sweets", "Burp Sweets", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Catches your food on the way down so you can take a look.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 1}, {IE::Kind::Happy, 9},
                                 {IE::Kind::Frag, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/28},

    {"peer_drops", "Peer Drops", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Pass the bag round and your friends hand them on to theirs.",
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
     "Fixed all the problems in one go. Grandma Yubi would be proud.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 100}, {IE::Kind::Frag, -100}, {IE::Kind::Happy, 100}}, /*combatHeal=*/0},

    // Created when Tor-Tilla Chip and OSI Dip are combined (Hacker MERGE HUB) — the
    // eighth layer OSI Dip was missing. Fills every stat at once.
    {"fully_stacked_nachos", "Fully-Stacked Nachos", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Chip to topping and you made each layer yourself.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 100}, {IE::Kind::Frag, -100}, {IE::Kind::Happy, 100}}, /*combatHeal=*/0},

    // Hashed Browns + Salted&Hashed Browns: the pantry's first cooked dish and its
    // second pass through the pan. Both are MERGE HUB outputs (game_internal.h's
    // kMergeRecipes) AND stocked at Moor-to-Moor — buying one is how a player MEETS
    // the dish, which is what a Decryptogram asks for before it will teach either
    // recipe (game_internal.h's MergeRecipe::requiresItems). They are also the only
    // cooked dishes a storefront carries: every other one is the recipe or nothing.
    {"hashed_browns", "Hashed Browns", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon, "Shredded so fine you could never put the potato back together.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 20}, {IE::Kind::Happy, 10}, {IE::Kind::Frag, -5}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/512},

    {"salted_hashed_browns", "Salted&Hashed Browns", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "A pinch of salt makes them much harder to crack.",
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
     "Keep trying and eventually you'll get the outside just right.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 60}, {IE::Kind::Happy, 5}, {IE::Kind::Frag, 5}}},

    {"hackshuka", "Hackshuka", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Sometimes it's fun to just grab some reg-eggs, whatever's in the "
     "fridge, and make it work.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Happy, 15}}, /*combatHeal=*/40},

    {"applet_turnover", "Applet Turnover", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "A little pastry that runs happily inside a bigger meal.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 20}, {IE::Kind::Happy, 35}, {IE::Kind::Frag, -10}}},

    {"serial_bar", "Serial Bar", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Eat it one bite at a time in order. That's the only way it goes.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::Happy, 10}, {IE::Kind::Frag, -5}}},

    {"macrol_fry_up", "Macrol Fry-Up", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Make it once while you're watching and you can repeat it forever.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 45}, {IE::Kind::Happy, 20}}, /*combatHeal=*/30},

    {"vanilla_java_roast", "Vanilla Java Roast", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Plain with no extras. Take it to work or the beach and it runs the "
     "same.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 45}, {IE::Kind::Frag, -30}}},

    // The pantry's second service. Three of these are the ingredient's own joke
    // finished: RISCotto is cooked in Boolean Cubes, which is what a risotto's stock
    // is; LANsagne is built on OSI Dip, whose seven layers are the dish; RAMen wants
    // exactly the noodles, egg and leek a bowl of it is made of.
    {"riscotto", "RISCotto", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Fewer ingredients done well. It even cooks faster.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 50}, {IE::Kind::Frag, -40}}},

    {"lansagne", "LANsagne", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Best shared with the people closest to you.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 60}, {IE::Kind::Happy, 20}}, /*combatHeal=*/40},

    {"ramen", "RAMen", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Fills you up fast but it's gone the moment you switch off.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 45}, {IE::Kind::Happy, 15}, {IE::Kind::Frag, -10}}},

    // EPIC — the upgrade that is not a stat: a permanently shorter Bandwidth regen.
    // First helping only, like every grant in the tier; after that the pet already has
    // root and this is simply a very good pudding that tops the pool up, which is what
    // its second effect is for.
    {"tiramisudo", "Tiramisudo", ItemDef::Type::Food,
     ItemDef::Rarity::Epic,
     "Guaranteed lasting pep in your step. Whether it\'s on the menu depends "
     "who\'s asking.",
     ItemDef::Context::Anytime,
     {{IE::Kind::BandwidthRegenBonusMin, 1}, {IE::Kind::Bandwidth, 1},
      {IE::Kind::Happy, 50}, {IE::Kind::Frag, -15}}},

    {"core_dumplings", "Core Dumplings", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Leftovers from a crash wrapped up neatly so you can look through them "
     "later.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 55}, {IE::Kind::Happy, 5}}},

    {"forkaccia", "Forkaccia", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Tear off a piece and it keeps growing on its own.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 40}, {IE::Kind::Happy, 10}}},

    // The pantry's first SECOND-ORDER dish: its lead ingredient is another cooked
    // dish, not a staple. Which is what a casserole is — yesterday's cooking, kept.
    {"cacherole", "Cacherole", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Made ahead and kept close so it's ready the second you want it.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 65}, {IE::Kind::Happy, 15}, {IE::Kind::Frag, -10}},
     /*combatHeal=*/25},

    {"gnulash", "GNUlash", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Free to share and each cook adds their own spin to the recipe.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 70}, {IE::Kind::Frag, -25}}, /*combatHeal=*/60},

    // The third service. Two of these are shaped by the kitchen rather than by a
    // pantry shelf: Portridge is Bootmeal's own row cooked, down to the tier and the
    // magnitudes, and Chrootons want a loaf that was itself a merge.
    {"portridge", "Portridge", ItemDef::Type::Food, ItemDef::Rarity::Common,
     "Same breakfast. Now it runs on gas or electric.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 10}, {IE::Kind::Happy, -5}}},

    {"halloumi_world", "Halloumi, World", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "The first dish you ever learn to cook.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 25}, {IE::Kind::Happy, 15}}},

    {"nan_bread", "NaN Bread", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Delicious. Just don't try comparing it to itself.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Frag, -5}}},

    {"chrootons", "Chrootons", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Toss them through the salad to share or keep a bowl to yourself.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 20}, {IE::Kind::Happy, 15}, {IE::Kind::Frag, -10}}},

    {"gzipacho", "Gzipacho", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "All the summer vegetables squeezed into a much smaller bowl.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 55}, {IE::Kind::Frag, -20}}, /*combatHeal=*/30},

    // A lassi cooked without its yogurt (content_recipes.cpp): the missing ingredient is
    // on the shelf and in other dishes' ingredient lists, so the gap reads as loss
    // rather than as a missing item, which is what the name promises.
    {"lossy_lassi", "Lossy Lassi", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "A little goes missing on the way to the glass. Still delicious.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 20}, {IE::Kind::Happy, 30}, {IE::Kind::Frag, -5}}},

    {"cod_review", "Cod Review", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Quality control on this dish is rigorous. If the fish is too fishy, "
     "the shipment gets cancelled.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 40}, {IE::Kind::Happy, 10}}, /*combatHeal=*/25},

    {"recursive_turducken", "Recursive Turducken", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "It's ducks all the way down.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 70}, {IE::Kind::Happy, 10}}, /*combatHeal=*/40},

    {"peking_duck_typing", "Peking Duck Typing", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "If it looks like dinner and smells like dinner, it's dinner.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 60}, {IE::Kind::Happy, 25}, {IE::Kind::Frag, -10}}},

    {"semaphreddo", "Semaphreddo", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "One spoon in the dish at a time. Wait your turn.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 25}, {IE::Kind::Happy, 45}, {IE::Kind::Frag, -20}}},

    // The one dish that ADDS Fragmentation. It fills a pet up and leaves it in a
    // state nobody can follow, which is what the name promises.
    {"spaghetti_code", "Spaghetti Code", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Usually delicious but the recipe is notoriously hard to modify.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 55}, {IE::Kind::Frag, 15}}},

    {"emacsaroni", "Emacsaroni", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Mac and cheese with so many extras it could replace the whole kitchen.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 45}, {IE::Kind::Happy, 20}}},

    {"bisectuits", "Bisectuits", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Can't find the burnt one? Split the tin in half and check again.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 25}, {IE::Kind::Happy, 20}}},

    {"quicksortbet", "Quicksortbet", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Dished up fast and it always comes out in order.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 20}, {IE::Kind::Happy, 40}, {IE::Kind::Frag, -15}}},

    // The fourth service — the rest of the kitchen. Filed by what they are rather than
    // by tier, like every dish above: bread and bakery, then the pans, then the plates,
    // then the puddings, then what you drink with them.
    {"buguette", "Buguette", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Crusty and fresh. Give it a good look over before you take a bite.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Happy, -5}}},

    {"chapati", "CHAPati", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Comes with a secret handshake. Get it right and you can have seconds.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::Happy, 10}}},

    {"corrumpets", "Corrumpets", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Toast them a little too long and they're never quite the same again.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::Happy, 10}, {IE::Kind::Frag, 5}}},

    {"packettone", "Packettone", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Shipped over in pieces that never arrive in quite the right order.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 60}, {IE::Kind::Happy, 25}}},

    {"hot_swapped_buns", "Hot-Swapped Buns", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Out with the old bun and in with the new without pausing dinner.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Happy, 15}}},

    {"current_buns", "Current Buns", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Fresh from the oven and positively buzzing.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::Happy, 18}}},

    {"config_rolls", "Config Rolls", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Your gran rolls them tight and your uncle rolls them loose. Both swear "
     "by it.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 25}, {IE::Kind::Happy, 20}}},

    {"brusshetta", "BruSSHetta", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "A crisp crust that keeps the toppings safe and sound.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 20}, {IE::Kind::Happy, 20}, {IE::Kind::Frag, -5}}},

    {"payloaf", "Payloaf", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Looks like an ordinary loaf. The good stuff is all in the middle.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 65}, {IE::Kind::Frag, 10}}, /*combatHeal=*/35},

    {"firewaffle", "Firewaffle", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "A grid of tidy squares. The syrup still sneaks through.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Happy, 20}, {IE::Kind::Frag, -8}}},

    // The pans -------------------------------------------------------------
    {"chownder", "Chownder", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Hold the bowl and it's yours.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 60}, {IE::Kind::Happy, 15}}, /*combatHeal=*/35},

    {"cronsomme", "Cronsomme", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Served on the dot every quarter hour whether you're hungry or not.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::Frag, -18}}},

    {"wanton_soup", "WANton Soup", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Wrapped up tight so they travel well over long distances.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 40}, {IE::Kind::Happy, 12}}, /*combatHeal=*/20},

    {"piperogi", "Piperogi", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Line them up and pass each one straight along to the next person.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 45}, {IE::Kind::Happy, 8}}},

    {"queuesadilla", "Queuesadilla", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "First come, first served. That's the rule here.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 40}, {IE::Kind::Happy, 15}}},

    {"ravioli_code", "Ravioli Code", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Each little parcel keeps its filling to itself.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 50}, {IE::Kind::Frag, -12}}},

    {"idleys", "Idleys", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Light and plain. Perfect for a lazy afternoon on the couch.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::Frag, -15}}},

    {"ms_dosa", "MS-Dosa", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "An old family recipe that still works. Just don't ask it to multitask.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 55}, {IE::Kind::Happy, 15}}},

    {"arpas", "ARPas", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Goes round the table asking who's got the salt.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Happy, 10}}},

    {"kafkofta", "Kafkofta", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Your order gets written down so you can have the same plate again "
     "later.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 60}, {IE::Kind::Happy, 10}}, /*combatHeal=*/30},

    {"kernel_panini", "Kernel Panini", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Press it too hard and the whole kitchen grinds to a halt.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 55}, {IE::Kind::Happy, 20}}, /*combatHeal=*/30},

    // EPIC — a race is decided by who gets there first, which is what a Speed point buys.
    {"racelette", "Racelette", ItemDef::Type::Food, ItemDef::Rarity::Epic,
     "First to the pan wins. Ties get messy.",
     ItemDef::Context::Anytime,
     {{IE::Kind::StatPointSpeed, 1}, {IE::Kind::Hunger, 50}, {IE::Kind::Happy, 25}}},

    {"scrambled_regeggs", "Scrambled RegEggs", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "They look like a mess but match exactly what you were craving.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Happy, 10}}},

    {"char_grilled_array", "Char-Grilled Array", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Lined up on one skewer with each piece in its own spot.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 45}, {IE::Kind::Happy, 8}}},

    {"tarballs", "Tarballs", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "The whole counter bundled into one tidy ball.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 50}, {IE::Kind::Happy, 5}}},

    {"bashed_potatoes", "Bashed Potatoes", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "A few good whacks with the masher and they do exactly what you tell "
     "them.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 45}, {IE::Kind::Happy, 10}}},

    {"onion_rings", "Onion Rings", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Golden layers wrapped around layers. Good luck tracing it back to the "
     "onion.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::Happy, 20}}},

    {"flash_fried_chips", "Flash-Fried Chips", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "In and out of the oil so fast the pan barely noticed.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Happy, 18}}},

    {"twisted_pairetzels", "Twisted Pairetzels", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Two strands wound together keep each other steady.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 25}, {IE::Kind::Happy, 18}}},

    {"jitter_fritters", "Jitter Fritters", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "They come out of the pan whenever they feel like it.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Happy, 12}}},

    {"shashimi", "SHAshimi", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Cut the same way each time so a fake slice stands out a mile.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 45}, {IE::Kind::Happy, 25}, {IE::Kind::Frag, -15}}},

    // EPIC — a spare in the array is the whole idea of a Defence point: one more thing
    // that has to fail before anything is actually lost.
    {"spare_ribs", "Spare RIBs", ItemDef::Type::Food, ItemDef::Rarity::Epic,
     "Always cook a few more than you need. You'll be glad you did.",
     ItemDef::Context::Anytime,
     {{IE::Kind::StatPointDefense, 1}, {IE::Kind::Hunger, 65}, {IE::Kind::Happy, 15}},
     /*combatHeal=*/30},

    {"rested_steak", "RESTed Steak", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Let it sit and each bite stands on its own.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 60}, {IE::Kind::Happy, 20}}, /*combatHeal=*/35},

    // EPIC — the pet keeps a Power point for life. The pun is the mechanic: an escalation
    // is not a thing you do twice, so the first plate roots it and later ones are veal.
    {"privilege_escalope", "Privilege Escalope", ItemDef::Type::Food,
     ItemDef::Rarity::Epic,
     "Start with a humble cutlet and work your way up to the head of the "
     "table.",
     ItemDef::Context::Anytime,
     {{IE::Kind::StatPointPower, 1}, {IE::Kind::Hunger, 55}, {IE::Kind::Happy, 30}}},

    {"force_pulled_pork", "Force-Pulled Pork", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Yanked off the bone before it was ready. Turns out it was fine.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 70}, {IE::Kind::Frag, 10}}, /*combatHeal=*/40},

    {"rubber_duck_confit", "Rubber Duck Confit", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "A bit chewy but that's the point. Chew it over long enough and the "
     "answer comes to you.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 55}, {IE::Kind::Happy, 30}, {IE::Kind::Frag, -20}}},

    {"vacuum_sealed_leftovers", "Vacuum-Sealed Leftovers", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Shrink-wrapped tight. Suddenly there's room in the fridge again.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 60}, {IE::Kind::Frag, -25}}},

    {"disk_platter", "Disk Platter", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "A big spinning platter. Your favourite comes round again soon.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 65}, {IE::Kind::Happy, 20}}},

    {"serverless_platter", "Serverless Platter", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Just appears when you're hungry. No waiter required.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 55}, {IE::Kind::Happy, 25}}, /*combatHeal=*/25},

    {"pickle_jar", "Pickle Jar", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Pack it away today and open it up exactly the same next year.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 20}, {IE::Kind::Happy, -5}, {IE::Kind::Frag, -12}}},

    // The condiments — small effects, but they are what the big plates are built on.
    {"ai_oli", "AI-oli", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Whipped up with total confidence. Taste it before you trust it.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 10}, {IE::Kind::Happy, 15}}},

    {"vinaigrette", "Vi-naigrette", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Easy to open. Good luck ever getting the lid back on.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 12}, {IE::Kind::Frag, -8}}},

    {"malwarmalade", "Malwarmalade", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Spreads from slice to slice so keep the lid on.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 12}, {IE::Kind::Happy, 18}, {IE::Kind::Frag, 5}}},

    {"signal_jam", "Signal Jam", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Spread it thick and it blocks out the toast completely.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 12}, {IE::Kind::Happy, 16}}},

    // The puddings --------------------------------------------------------
    {"pop3sicle", "POP3sicle", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Take it out of the freezer and it's gone from there for good.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 15}, {IE::Kind::Happy, 30}}},

    {"mergingue", "Mergingue", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Fold two batches together carefully and there's no conflict at all.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 15}, {IE::Kind::Happy, 32}}},

    {"declair", "Declair", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Bringing a box back from holiday? Customs will want to hear about it "
     "first.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 20}, {IE::Kind::Happy, 30}}},

    // EPIC — the one Epic dish that grants no stat at all. A profiler doesn't make the
    // pet stronger, it makes every hour it spends teach it more, which is an XP rate.
    {"profilerole", "Profilerole", ItemDef::Type::Food, ItemDef::Rarity::Epic,
     "One bite and you know exactly where your afternoon went.",
     ItemDef::Context::Anytime,
     {{IE::Kind::XpRateBonusPct, 25}, {IE::Kind::Hunger, 25}, {IE::Kind::Happy, 38},
      {IE::Kind::Frag, -10}}},

    {"coboler", "COBOLer", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "An ancient recipe that still runs half the bakeries in town.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 50}, {IE::Kind::Happy, 25}}},

    {"clustard", "Clustard", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Poured from several jugs at once so one spill never ruins dessert.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 25}, {IE::Kind::Happy, 22}}},

    {"bashlava", "Bashlava", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Layer after layer after layer. Once you've made one the rest just "
     "loop.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 40}, {IE::Kind::Happy, 40}, {IE::Kind::Frag, 8}}},

    {"deflated_souffle", "Deflated Souffle", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "All the air squeezed out and it's still the same pudding.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::Happy, 12}}},

    {"fork_bombe", "Fork Bombe", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Cut it in half and you get two. Cut those and you get four. Don't keep "
     "cutting.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 45}, {IE::Kind::Happy, 35}, {IE::Kind::Frag, 12}}},

    {"optical_mousse", "Optical Mousse", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Light as air. It glides right across the plate.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 25}, {IE::Kind::Happy, 40}, {IE::Kind::Frag, -12}}},

    {"cherry_picked_tart", "Cherry-Picked Tart", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Only the best fruit off the branch makes it in.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 30}, {IE::Kind::Happy, 42}}},

    {"raspberry_pie", "Raspberry Pie", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Cheap and small and surprisingly handy.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 35}, {IE::Kind::Happy, 25}}},

    {"rainbow_tablet", "Rainbow Tablet", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "All the flavours worked out in advance so cracking one open is easy.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 20}, {IE::Kind::Happy, 45}, {IE::Kind::Frag, 10}}},

    {"mint_choc_chip", "Mint Choc Chip", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "A fresh and friendly flavour that's easy to switch to.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 25}, {IE::Kind::Happy, 38}, {IE::Kind::Frag, -8}}},

    {"candied_yamls", "Candied YAMLs", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Arrange them carefully. One out of line and the whole tray is wrong.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 40}, {IE::Kind::Happy, 20}}},

    // What you drink with them --------------------------------------------
    {"flat_white", "Flat White", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "No layers and no structure. Just one smooth pour.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 5}, {IE::Kind::Happy, 28}, {IE::Kind::Frag, -12}}},

    {"mockachino", "Mockachino", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Not really coffee but it stands in for one while you're testing.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 8}, {IE::Kind::Happy, 26}, {IE::Kind::Frag, -8}}},

    {"blockchai", "Blockchai", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "Brewed from the last cup and you can't take a sip back.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 10}, {IE::Kind::Happy, 35}, {IE::Kind::Frag, -18}}},

    {"syn_ack_shake", "SYN-ACK Shake", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "You offer. They accept. Then you both drink.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 25}, {IE::Kind::Happy, 25}}},

    // EPIC — the pet's max Health IS its buffer, and this is the drink that writes past
    // the end of it. The Fragmentation it adds is the cost of taking the extra room.
    {"buffer_overfloat", "Buffer Overfloat", ItemDef::Type::Food,
     ItemDef::Rarity::Epic,
     "Pour until it spills into the next glass over.",
     ItemDef::Context::Anytime,
     {{IE::Kind::StatPointHealth, 1}, {IE::Kind::Hunger, 30}, {IE::Kind::Happy, 40},
      {IE::Kind::Frag, 15}}},

    {"hard_cidr", "Hard CIDR", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Sold by the block in sizes that never quite match what you wanted.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 10}, {IE::Kind::Happy, 28}, {IE::Kind::Frag, 8}}},

    {"port_80", "Port 80", ItemDef::Type::Food, ItemDef::Rarity::Rare,
     "A fortified wine that's open to all comers.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 12}, {IE::Kind::Happy, 42}, {IE::Kind::Frag, 10}}},

    {"fizzbuzz", "FizzBuzz", ItemDef::Type::Food, ItemDef::Rarity::Uncommon,
     "Every third sip fizzes and every fifth one buzzes. Easy to make but "
     "hard to get right.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 8}, {IE::Kind::Happy, 24}}},

    {"punchcard_punch", "Punchcard Punch", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "The steps go in a strict order. Don't drop the recipe cards.",
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
     "For the days you need a friend in your corner.",
     ItemDef::Context::Anytime, {{IE::Kind::ArmCombatShieldBuff, 60}}},
    
    // Rare Cache: Open in the VAULT for a Rare Reward not locked to any area in particular
     {"sealed_cache_rare", "Rare Cache", ItemDef::Type::Quest,
     ItemDef::Rarity::Rare,
     "A hit worth getting excited about.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::OpenContainer,
     /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/0, /*cache=*/{/*bits=*/30, /*draws=*/1, /*drawChancePct=*/100,
                kCachePoolRare, poolN(kCachePoolRare), /*findWeight=*/15}},

    // Kernel Bell: the Backdoor Bell's deeper cousin still — see its comment above.
    {"kernel_bell", "Kernel Bell", ItemDef::Type::Tool,
     ItemDef::Rarity::Rare, "Pops you straight down to the core.",
     ItemDef::Context::Anytime, {{IE::Kind::SetDeepWebStartDepth, 64}, {IE::Kind::DiveStartBonusPct, 100}}},

    // Deep-Learning Module: arms the dive's depth-per-win multiplier
    // (ArmDeepWebDepthMultiplier) — the pet "learns" the shallow depths fast so it
    // skips ahead on every win instead of one step at a time. Overwrites, doesn't
    // stack — a fresh Module/Core just replaces whichever multiplier is currently
    // armed. Lets a blitzing endgame pet catch back up to a real fight faster.
    {"deep_learning_module", "Deep-Learning Module", ItemDef::Type::Tool,
     ItemDef::Rarity::Rare, "It's seen enough dives to know which bits to skip.",
     ItemDef::Context::Anytime, {{IE::Kind::ArmDeepWebDepthMultiplier, 2}, {IE::Kind::DiveStepBonusPct, 50}}},
    //
    // EPIC ITEMS --------------------------
    //
     // The Yubi-Cookie is a COOKIE: it is eaten, it fills the pet up, and the pet is
    // happier for it every single time. What it does once in a life is forget a care
    // mistake — the same shape the Epic dishes take, a very good meal with one
    // permanent thing folded into it, which is why it is filed with the food rather
    // than with the buffs it arms none of.
    {"yubi_cookie", "Yubi-Cookie", ItemDef::Type::Food,
     ItemDef::Rarity::Epic, "It's impossible to hold a grudge when the secret ingredient is love.",
     ItemDef::Context::Anytime,
     {{IE::Kind::RemoveCareMistakeOnce, 1}, {IE::Kind::Hunger, 20},
      {IE::Kind::Happy, 40}}},
    
     // Restore Point: a System-Restore shield — Use on a Process/Script pet to
    // arm protection against the NEXT care mistake, once per lifetime. The shield is
    // per-pet, consumed on the next positive mistake.
    {"restore_point", "Restore Point", ItemDef::Type::Buff,
     ItemDef::Rarity::Epic, "For the day you wish you could undo yesterday.",
     ItemDef::Context::Anytime, {{IE::Kind::ClearMistakeShieldOnce, 1}}},
    
    // Epic Cache: Open in the VAULT for an Epic Reward not locked to any area in particular
    // The Epic cache is also a second, non-boss MOD source: `modChancePct` is the
    // chance an Open ALSO yields a permanent mod, rolled globally rarity-weighted (even
    // a tier-4 mod can surface — its rolled equip-level gate holds it until the pet is
    // deep enough). The yield reveal shows the items/Bits; the mod lands in MODS.
    {"sealed_cache_epic", "Epic Cache", ItemDef::Type::Quest,
     ItemDef::Rarity::Epic,
     "Warm and full and ready to go.",
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
     "A thank-you for sticking with it. You earned this.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::OpenContainer,
     /*category=*/ItemDef::Category::Derive,
     /*dropWeight=*/0, /*cache=*/{/*bits=*/90, /*draws=*/2, /*drawChancePct=*/100,
                kCachePoolCommend, poolN(kCachePoolCommend), /*findWeight=*/0,
                /*modChancePct=*/60}},

    // The PALETTE CHIPS. Six objects that do the same thing — hold one and the colour
    // set it carries is unlocked in CFG for good (content_themes.h) — so they are one
    // family and written as one block. Each is a Quest/Keys row that is INERT in the bag:
    // itemUsable answers "SET IN CFG > THEME", because the chip is not spent to apply a
    // theme and applying one is not a thing done to the pet. Nothing sells them and
    // nothing but a commendation drops them.
    //
    // Rare or Epic by what the set costs to give up on the device it is imitating: the
    // two that are somebody else's hardware — a sunset arcade cabinet, a pocket LCD —
    // are Epic; the four that are a filter over the tube you already own are Rare.
    {"sunset_rom", "Sunset ROM", ItemDef::Type::Quest,
     ItemDef::Rarity::Epic,
     "Neon skies and chrome burned in for good.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Keys},

    {"pocket_lcd", "Pocket LCD", ItemDef::Type::Quest,
     ItemDef::Rarity::Epic,
     "Four shades of green and a whole childhood.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Keys},

    {"phosphor_tube", "Phosphor Tube", ItemDef::Type::Quest,
     ItemDef::Rarity::Rare,
     "Green glow on black glass and a quiet hum.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Keys},

    {"amber_tube", "Amber Tube", ItemDef::Type::Quest,
     ItemDef::Rarity::Rare,
     "Warm orange glow on black glass. Easy on tired eyes.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Keys},

    {"daylight_filter", "Daylight Filter", ItemDef::Type::Quest,
     ItemDef::Rarity::Rare,
     "Crisp and clear even out in the midday sun.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Keys},

    {"redshift_lens", "Redshift Lens", ItemDef::Type::Quest,
     ItemDef::Rarity::Rare,
     "Warms the colours as the sun goes down so your eyes can rest.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Keys},

    // Key warp items: consumables used DURING the
    // walk (the B warp picker) to jump straight to a target event, not eaten/buffed.
    // Using one from ITEMS is inert ("USE ON THE WALK", itemUsable).
    {"access_token", "Access Token", ItemDef::Type::Quest,
     ItemDef::Rarity::Uncommon,
     "Proves you're allowed in. No questions asked.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::Shop, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Keys},
    
    // Warp to a safe zone and rest off some fragmentation. The rest is the whole
    // point of the key, so how deep it cleans is a magnitude on THIS row (a negative
    // Frag effect — rest de-frags), applied by resolveSafeRestEvent through the same
    // applyItemEffects every other item goes through.
    {"safe_mode_key", "Safe-Mode Key", ItemDef::Type::Quest,
     ItemDef::Rarity::Uncommon,
     "Boots you somewhere quiet with only the essentials running.",
     ItemDef::Context::Anytime,
     /*effects=*/{{ItemEffect::Kind::Frag, -20}}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::SafeRest, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Keys},
    
    // Rollback: a TOOL, and the row the band is shaped around. Use opens a stat picker
    // (use=Rollback) and hands the operator a lever over the stat TABLE — the pet is the
    // same creature either side of it, which is what puts it here rather than in Buffs
    // (the rule is on ItemDef::Type). Sheds one earned combat-stat point (-1 that stat, -1 level)
    // so the pet re-grinds that level and re-rolls a fresh +1. A reward-pool drop; inert
    // at level 0 (nothing to shed), and it can never reach an off-level point an Epic
    // dish granted (core/model/pet_upgrades.h).
    {"rollback", "Rollback", ItemDef::Type::Tool,
     ItemDef::Rarity::Rare,
     "Undo the last thing you were sure about.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Rollback},

    // Repartition: Rollback's sibling, and the OTHER way to argue with the stat table.
    // Where a Rollback hands a point back to the dice and charges a level's grind for
    // the re-roll, this one moves a point the pet already has onto a stat the operator
    // names — same level, same total, a different shape. So it is a lever over the same
    // system (a TOOL, like its sibling), and its whole cost is the item: there is no
    // grind to pay because nothing was un-earned, which is what puts it an entire tier
    // above the Rollback. Use opens the two-step picker (use=Repartition): a FROM stat
    // with a point to spare, then a TO stat that isn't it. Inert at level 0, for the
    // same reason a Rollback is — nothing earned, nothing to move — and it cannot reach
    // an off-level point an Epic dish granted (core/model/pet_upgrades.h), because those
    // are not on the earned table at all.
    {"repartition", "Repartition", ItemDef::Type::Tool,
     ItemDef::Rarity::Epic,
     "Same space. Just divided up the way you wanted it.",
     ItemDef::Context::Anytime, /*effects=*/{}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Repartition},
    
    // Ambig-USB: Use on a Process pet to guarantee its Trojan divert instead of leaving it to the
    // kTrojanDivertPct roll. Stocked item at Moor-to-Moor (Napstorrent Moors).
    {"ambig_usb", "Ambig-USB", ItemDef::Type::Tool,
     ItemDef::Rarity::Epic,
     "Found it in the car park. Surely it's fine to plug in.",
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
    {"bad_usb", "Bad-USB", ItemDef::Type::Tool,
     ItemDef::Rarity::Epic,
     "Looks like an ordinary stick but it types faster than you do.",
     ItemDef::Context::Anytime, {{IE::Kind::ForceEvolveBranchBad, 1}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/1},

    // Signed-USB: the same device with a vendor signature on its firmware, and the exact
    // inverse item — the GOOD successor whatever the record says, which is the half that
    // rescues a badly-raised pet rather than the half that ruins a well-raised one. Same
    // one slot as the Bad-USB: plugging either in replaces the other.
    {"signed_usb", "Signed-USB", ItemDef::Type::Tool,
     ItemDef::Rarity::Epic,
     "Comes with a manufacturer's seal and a clean conscience.",
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
    {"sandbox_usb", "Sandbox-USB", ItemDef::Type::Tool,
     ItemDef::Rarity::Rare,
     "Lets it play somewhere safe for a while before it grows up.",
     ItemDef::Context::Anytime, {{IE::Kind::ArmEvolveSoak, 2}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/3},

    // Hypervisor-USB: the Sandbox-USB's Epic upgrade — four times as deep AND it reaches a
    // stage the Sandbox cannot. On a Script it still pays x{soak} XP but the clock costs
    // DOUBLE that, because a Script's boundary is the branch the whole raise was aimed at:
    // stretching the last stage before an ending is worth more than stretching a middle
    // one, so it is priced to match.
    // The only USB anyone sells (Moor-to-Moor, Napstorrent Moors), and it is priced in
    // its own family: four Sandbox-USBs plus Bits, so the deep end of the ladder is
    // reached by diving for the rare one four times over rather than by having a wallet.
    //
    // WHY THE SOAK IS THIS DEEP. What the arm actually costs is a STAGE, and a stage is
    // the biggest single thing a pet owns: kStagePowerScalePct alone runs 100 -> 230
    // across the two boundaries this holds shut, on top of a move slot per stage and the
    // Script-gated half of the move roster. A pet held at Process to farm is giving up
    // all of that for as long as it is armed, and the four Sandbox-USBs it is built from
    // are DeepWeb Dive drops — a zone that scales to the pet's own level from depth 0, so
    // the ingredients are gated behind the one place the hold makes hardest to farm. The
    // XP has to be worth a stage to be worth arming at all.
    {"hypervisor_usb", "Hypervisor-USB", ItemDef::Type::Tool,
     ItemDef::Rarity::Epic,
     "Runs a whole world inside the one it's plugged into.",
     ItemDef::Context::Anytime, {{IE::Kind::ArmEvolveSoakLate, 8}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/2048},

    // Halt-USB: the pet stops evolving. Not a stretch of the clock — a refusal to reach
    // the boundary at all, and the only device in the family that is never consumed,
    // because no boundary arrives to consume it. It comes out when an Eject-USB pulls it,
    // when the pet goes back on the ARCH rack, or never. What it is FOR is parking a pet
    // at a stage you want it at: the roster has thirty-five species and only sixteen of
    // them are endings, so keeping one of each means keeping the middle of the chains.
    {"halt_usb", "Halt-USB", ItemDef::Type::Tool,
     ItemDef::Rarity::Rare,
     "Freezes the moment right where it is.",
     ItemDef::Context::Anytime, {{IE::Kind::ArmEvolveHold, 0}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/6},

    // Eject-USB: pull whatever is in the port and drop its effect, whichever device it
    // was. The family's undo, and the one thing that goes in while a soak or a hold is
    // already there — a port that could only be emptied by the boundary it was refusing
    // to reach would be a trap rather than a decision. Drawn commoner than the devices it
    // undoes (dropWeight), for the same reason.
    {"eject_usb", "Eject-USB", ItemDef::Type::Tool,
     ItemDef::Rarity::Rare,
     "Pull it out safely and life goes back to normal.",
     ItemDef::Context::Anytime, {{IE::Kind::ClearUsbPort, 0}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/8},

    // Checkpoint Bell: the Backdoor Bell's ultimate cousin — instead of a fixed depth,
    // it warps the next DeepWeb Dive straight to THIS PET's own best-ever depth
    // (SetDeepWebStartDepthToBest reads bestDeepWebDepth_ at dive-start), never any
    // other pet's or the device's frontier.
    {"zeroday_bell", "Checkpoint Bell", ItemDef::Type::Tool,
     ItemDef::Rarity::Epic,
     "Saved at your finest moment and ready to load.",
     ItemDef::Context::Anytime, {{IE::Kind::SetDeepWebStartDepthToBest, 0}, {IE::Kind::DiveStartBonusPct, 100}}},

    // Deep-Learning Core: Deep-Learning Module's Epic upgrade — see its comment above.
    {"deep_learning_core", "Deep-Learning Core", ItemDef::Type::Tool,
     ItemDef::Rarity::Epic, "Trained on more dives than you'll ever take.",
     ItemDef::Context::Anytime, {{IE::Kind::ArmDeepWebDepthMultiplier, 4}, {IE::Kind::DiveStepBonusPct, 100}}},
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
