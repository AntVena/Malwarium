// The rare kit: the Rare and Epic items that are not pantry staples or cooked dishes.
// The death-save and the per-lifetime shields, the deeper bells and dive modules, the
// high-tier and commendation caches, the palette chips, the walk warp keys and the USBs.
//
// One of the item families listed in item_families.h; ITEM_CONTENT_STANDARD.md says how
// a family is laid out and how a row reads.
#pragma once

#include "core/content/items/pools.h"
#include "core/content/defs.h"

namespace mal::item_table {

using IE = ItemEffect;

inline constexpr ItemDef kRareKitItems[] = {
    //
    // RARE ITEMS --------------------------
    //
    // Backup Drive: a combat buff, not a Lockout item. Use arms a 1-hour DEATH-SAVE
    // (ItemEffect::ArmCombatShieldBuff, save v30). Every hit lands in full; the drive is
    // read once, at the moment the pet would be overwhelmed (Combat::checkOutcome), and
    // hands back half of MAX Health from wherever the pet ended up. So it usually saves
    // a life and sometimes doesn't: a blow that buried the pet deeper than half its max
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

    // Kernel Bell: the Backdoor Bell's deeper cousin still. See its comment above.
    {"kernel_bell", "Kernel Bell", ItemDef::Type::Tool,
     ItemDef::Rarity::Rare, "Pops you straight down to the core.",
     ItemDef::Context::Anytime, {{IE::Kind::SetDeepWebStartDepth, 64}, {IE::Kind::DiveStartBonusPct, 100}}},

    // Deep-Learning Module: arms the dive's depth-per-win multiplier
    // (ArmDeepWebDepthMultiplier). The pet "learns" the shallow depths fast so it
    // skips ahead on every win instead of one step at a time. Overwrites, doesn't
    // stack: a fresh Module/Core just replaces whichever multiplier is currently
    // armed. Lets a blitzing endgame pet catch back up to a real fight faster.
    {"deep_learning_module", "Deep-Learning Module", ItemDef::Type::Tool,
     ItemDef::Rarity::Rare, "It's seen enough dives to know which bits to skip.",
     ItemDef::Context::Anytime, {{IE::Kind::ArmDeepWebDepthMultiplier, 2}, {IE::Kind::DiveStepBonusPct, 50}}},
    //
    // EPIC ITEMS --------------------------
    //
     // The Yubi-Cookie is a COOKIE: it is eaten, it fills the pet up, and the pet is
    // happier for it every single time. What it does once in a life is forget a care
    // mistake: the same shape the Epic dishes take, a very good meal with one
    // permanent thing folded into it, which is why it is filed with the food rather
    // than with the buffs it arms none of.
    {"yubi_cookie", "Yubi-Cookie", ItemDef::Type::Food,
     ItemDef::Rarity::Epic, "It's impossible to hold a grudge when the secret ingredient is love.",
     ItemDef::Context::Anytime,
     {{IE::Kind::RemoveCareMistakeOnce, 1}, {IE::Kind::Hunger, 20},
      {IE::Kind::Happy, 40}}},
    
     // Restore Point: a System-Restore shield. Use on a Process/Script pet to
    // arm protection against the NEXT care mistake, once per lifetime. The shield is
    // per-pet, consumed on the next positive mistake.
    {"restore_point", "Restore Point", ItemDef::Type::Buff,
     ItemDef::Rarity::Epic, "For the day you wish you could undo yesterday.",
     ItemDef::Context::Anytime, {{IE::Kind::ClearMistakeShieldOnce, 1}}},
    
    // Epic Cache: Open in the VAULT for an Epic Reward not locked to any area in particular
    // The Epic cache is also a second, non-boss MOD source: `modChancePct` is the
    // chance an Open ALSO yields a permanent mod, rolled globally rarity-weighted (even
    // a tier-4 mod can surface. Its rolled equip-level gate holds it until the pet is
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
    // (content_achievements.cpp rows list it in `rewards`). findWeight 0: it is never
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

    // The PALETTE CHIPS. Six objects that do the same thing. Hold one and the colour
    // set it carries is unlocked in CFG for good (content_themes.h), so they are one
    // family and written as one block. Each is a Quest/Keys row that is INERT in the bag:
    // itemUsable answers "SET IN CFG > THEME", because the chip is not spent to apply a
    // theme and applying one is not a thing done to the pet. Nothing sells them and
    // nothing but a commendation drops them.
    //
    // Rare or Epic by what the set costs to give up on the device it is imitating: the
    // two that are somebody else's hardware (a sunset arcade cabinet, a pocket LCD)
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
    // Frag effect: rest de-frags), applied by resolveSafeRestEvent through the same
    // applyItemEffects every other item goes through.
    {"safe_mode_key", "Safe-Mode Key", ItemDef::Type::Quest,
     ItemDef::Rarity::Uncommon,
     "Boots you somewhere quiet with only the essentials running.",
     ItemDef::Context::Anytime,
     /*effects=*/{{ItemEffect::Kind::Frag, -20}}, /*combatHeal=*/0, /*preEncounterXp=*/0,
     /*bits=*/0, /*walkWarp=*/ItemDef::WalkWarp::SafeRest, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Keys},
    
    // Rollback: a TOOL, and the row the band is shaped around. Use opens a stat picker
    // (use=Rollback) and hands the operator a lever over the stat TABLE: the pet is the
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
    // names: same level, same total, a different shape. So it is a lever over the same
    // system (a TOOL, like its sibling), and its whole cost is the item: there is no
    // grind to pay because nothing was un-earned, which is what puts it an entire tier
    // above the Rollback. Use opens the two-step picker (use=Repartition): a FROM stat
    // with a point to spare, then a TO stat that isn't it. Inert at level 0, for the
    // same reason a Rollback is (nothing earned, nothing to move), and it cannot reach
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
    // Four devices that all steer the SAME boundary the Ambig-USB does (the pet's next
    // evolution), which is what makes them one family and why the soak pair below can
    // lock the port against the other three (defs.h's isUsbEffect). All but the
    // Hypervisor are DeepWeb Dive drops (areas/deepweb_dive/area.cpp): no counter sells
    // the ability to overrule how a pet was raised, so the only way to hold one is to go
    // down and take it.

    // Bad-USB: the firmware attack the name comes from, and the item that does to a pet
    // what it does to a host. The branch is decided by the DEVICE, not by the record.
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
    // inverse item. The GOOD successor whatever the record says, which is the half that
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
    // pays {soak} times the XP for everything the pet does while it is stretched: the
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

    // Hypervisor-USB: the Sandbox-USB's Epic upgrade. Four times as deep AND it reaches a
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
    // are DeepWeb Dive drops: a zone that scales to the pet's own level from depth 0, so
    // the ingredients are gated behind the one place the hold makes hardest to farm. The
    // XP has to be worth a stage to be worth arming at all.
    {"hypervisor_usb", "Hypervisor-USB", ItemDef::Type::Tool,
     ItemDef::Rarity::Epic,
     "Runs a whole world inside the one it's plugged into.",
     ItemDef::Context::Anytime, {{IE::Kind::ArmEvolveSoakLate, 8}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/2048},

    // Halt-USB: the pet stops evolving. Not a stretch of the clock: a refusal to reach
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
    // already there: a port that could only be emptied by the boundary it was refusing
    // to reach would be a trap rather than a decision. Drawn commoner than the devices it
    // undoes (dropWeight), for the same reason.
    {"eject_usb", "Eject-USB", ItemDef::Type::Tool,
     ItemDef::Rarity::Rare,
     "Pull it out safely and life goes back to normal.",
     ItemDef::Context::Anytime, {{IE::Kind::ClearUsbPort, 0}},
     /*combatHeal=*/0, /*preEncounterXp=*/0, /*bits=*/0,
     /*walkWarp=*/ItemDef::WalkWarp::None, /*use=*/ItemDef::Use::Consume,
     /*category=*/ItemDef::Category::Derive, /*dropWeight=*/8},

    // Checkpoint Bell: the Backdoor Bell's ultimate cousin, instead of a fixed depth,
    // it warps the next DeepWeb Dive straight to THIS PET's own best-ever depth
    // (SetDeepWebStartDepthToBest reads bestDeepWebDepth_ at dive-start), never any
    // other pet's or the device's frontier.
    {"zeroday_bell", "Checkpoint Bell", ItemDef::Type::Tool,
     ItemDef::Rarity::Epic,
     "Saved at your finest moment and ready to load.",
     ItemDef::Context::Anytime, {{IE::Kind::SetDeepWebStartDepthToBest, 0}, {IE::Kind::DiveStartBonusPct, 100}}},

    // Deep-Learning Core: Deep-Learning Module's Epic upgrade. See its comment above.
    {"deep_learning_core", "Deep-Learning Core", ItemDef::Type::Tool,
     ItemDef::Rarity::Epic, "Trained on more dives than you'll ever take.",
     ItemDef::Context::Anytime, {{IE::Kind::ArmDeepWebDepthMultiplier, 4}, {IE::Kind::DiveStepBonusPct, 100}}},
};
inline constexpr int kRareKitItemsCount = sizeof(kRareKitItems) / sizeof(kRareKitItems[0]);

}  // namespace mal::item_table
