// pools.h: the reward pools the item table's caches and the walk's loot event draw from.
//
// The item sets a container draws from, and the one the walk's own loot-cache event
// pays out of (kLootPool, read by game_explore). Pools live beside the rows they name,
// so a cache's whole definition (purse, draw count, pool, find weight) reads off one
// place: its own row's CacheDef.
//
// Pools are rarity-graded: the common tiers hand over consumables, the higher ones add
// the scarce utility items. Every understood item needs a live earn path (the native
// gate asserts it), so an item that isn't shop-stocked, starting kit, or a walk find
// has to appear in one of these.
//
// A pool row is a LootEntry (defs.h): an id, plus a weight ONLY when this pool wants
// that item at a different frequency from everywhere else. A bare `{"id"}` draws at
// the item's own ItemDef::dropWeight, which in turn falls back to its rarity, so read
// the weights on the rows here as exceptions, and the item table as the rule.
#pragma once

#include "core/content/defs.h"

namespace mal::item_table {

inline constexpr LootEntry kCachePoolCommon[]   = {{"dyno_nuggets"}, {"tortilla_chip"},
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
inline constexpr LootEntry kCachePoolUncommon[] = {{"dyno_nuggets"}, {"r007_b33r"},
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
inline constexpr LootEntry kCachePoolRare[]     = {{"backup_drive"}, {"sinkhole_trap"},
                                                   {"rollback"}, {"deep_learning_module"},
                                                   {"kernel_bell"}, {"decryptogram"},
                                                   // THE SCARCE SHELF: the six staples the EPIC
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
inline constexpr LootEntry kCachePoolEpic[]     = {{"rollback"}, {"repartition"}, {"yubi_cookie"},
                                                   {"backup_drive"}, {"restore_point"},
                                                   {"deep_learning_core"}, {"zeroday_bell"}};
// The share of the commendation pool one PALETTE CHIP draws at. Six of them against
// seven consumables whose own weights sum to ~55, so a first commendation is about as
// likely to pay a colour set as a prize and a device that has them all is back to the
// pool it started with. Lives here rather than in tunables.h because it is a fact about
// this one pool's balance, not a cross-cutting one.
inline constexpr int kChipDrawWeight = 6;

// The Commendation Cache's own pool: what an achievement pays out. Deliberately not
// the Epic pool: a commendation is earned by playing a whole ladder out, so it hands
// over the scarce per-lifetime shields and the deepest diving bells rather than
// re-rolling the same walk consumables a found cache already gives.
// ...plus the PALETTE CHIPS, the one thing in here that is not a consumable: each
// unlocks a colour set for good (content_themes.h), so a chip is worth a draw exactly
// once and then stops being loot at all; rollLootEntry drops one whose set is already
// unlocked to weight 0 rather than letting it cost a commendation a real prize. At
// kChipDrawWeight they are a bit under half the pool while any remain and none of it
// once they are all in, which is the shape a finite set of unlocks should have: a
// reason to open the next one, and never a tax on the twentieth.
inline constexpr LootEntry kCachePoolCommend[]  = {{"restore_point"}, {"yubi_cookie"},
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
// only source of the diving bells and the scarce utility items: at full weight the
// staples would crowd those out of the one place they come from, so every pantry row
// there carries this override instead.
inline constexpr int kStapleWalkWeight = 8;
// ...and the share the six SCARCE staples draw at: the ones the Epic dishes are gated
// on (kCachePoolRare above). A quarter of an ordinary staple's, so a walk still turns
// one up now and then and an Epic dish stays cookable without a cache, just slowly.
// This is the one number that decides how fast a player can hand a pet a permanent
// upgrade, so it lives beside the weight it is a fraction of rather than in tunables.
inline constexpr int kRareStapleWalkWeight = 2;

// The walk loot-cache event's own pool (Game::grantLootReward + the Wi-Fi sleeping-
// guardian / open-cache sub-outcomes), and the legacy `sealed_cache`'s draw. The
// DeepWeb Dive's depth items ride here too: earned from any area's walk loot, not
// gated to the dive itself (they're only USEFUL there).
inline constexpr LootEntry kLootPool[] = {{"dyno_nuggets"}, {"tortilla_chip"},
    {"pwnzu_sauce"}, {"backup_drive"}, {"rollback"}, {"repartition"}, {"osi_dip"},
    {"deep_learning_module"}, {"deep_learning_core"}, {"backdoor_bell"},
    {"rootkit_bell"}, {"kernel_bell"}, {"zeroday_bell"}, {"decryptogram"},
    // The pantry, thinned to kStapleWalkWeight apiece: see the note on that constant.
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
inline constexpr int kLootPoolCount = poolN(kLootPool);

}  // namespace mal::item_table

namespace mal {
using item_table::kLootPool;
using item_table::kLootPoolCount;
}  // namespace mal
