// The everyday kit: the Common and Uncommon items that are not pantry staples or
// cooked dishes: the keys, the first diving bells, the low-tier caches, and the six
// snacks outside the pantry (Dyno Nuggets, Unlinkguine, and the four halves of the first
// two merges).
//
// One of the item families listed in item_families.h; ITEM_CONTENT_STANDARD.md says how
// a family is laid out and how a row reads.
#pragma once

#include "core/content/items/pools.h"
#include "core/content/defs.h"

namespace mal::item_table {

using IE = ItemEffect;

inline constexpr ItemDef kKitItems[] = {
    //
    // COMMON ITEMS --------------------------
    //
    {"decrypt_key", "Decryption Key", ItemDef::Type::Quest,
     ItemDef::Rarity::Common, "Turns a ransom note into a polite suggestion.",
     ItemDef::Context::LockoutOnly, /*effects=*/{}, /*combatHeal=*/0,
     /*preEncounterXp=*/0, /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::None,
     /*use=*/ItemDef::Use::Consume, /*category=*/ItemDef::Category::Keys},
    
     // the Defrag Tool: a common consumable spent by a TOOL DEFRAG in MAINT
    // for a GUARANTEED clean (no fail roll). Never used from the ITEMS path (gated in
    // itemUsable); sold at the shop + drops from wild loot so it's obtainable. No new
    // glyph: reuses the MAINT defrag icon (itemIcon fallback).
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
    // incubation, floored at the crackable window: every line's hatch minigame is
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
    // in at the Hacker VAULT, never from ITEMS: the prize is a player-level account
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
    // depth instead of swimming down. These arm the NEXT DeepWeb Dive to skip
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
    // in its shape are what the pantry feeds a process, which is the whole joke, and the
    // reason this is the one food that turns up everywhere rather than anywhere special.
    // It fills and patches and does nothing else; the ghost cure is Unlinkguine's job.
    {"dyno_nuggets", "Dyno Nuggets", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Spun up fresh for dinner and gone again in seconds.",
     ItemDef::Context::Anytime,
     {{IE::Kind::Hunger, 40}}, /*combatHeal=*/30},

    // The ghost cure, and the counterpart to Linkguine ("every strand joined to the
    // last one"): unlink() severs the reference to a copy, which is exactly a
    // Replication Ghost's problem. The phantom process a failed defrag leaves behind
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
    // applied by game_combat::resolveSinkhole. A hand-off, not an on-Use pet effect).
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
    
    // The untiered Sealed Cache: no findWeight, so the walk's cache roll
    // (game_explore.cpp) never picks it. It
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

    // Rootkit Bell: the Backdoor Bell's deeper cousin. See its comment above.
    {"rootkit_bell", "Rootkit Bell", ItemDef::Type::Tool,
     ItemDef::Rarity::Uncommon, "Gets in below the level a normal check reaches.",
     ItemDef::Context::Anytime, {{IE::Kind::SetDeepWebStartDepth, 32}, {IE::Kind::DiveStartBonusPct, 50}}},
};
inline constexpr int kKitItemsCount = sizeof(kKitItems) / sizeof(kKitItems[0]);

}  // namespace mal::item_table
