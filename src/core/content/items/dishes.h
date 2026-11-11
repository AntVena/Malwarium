// The dishes: every MERGE HUB output. The first four are the two-snack merges and the
// browns, which a storefront also carries; every row under COOKED DISHES is the recipe
// or nothing.
//
// One of the item families listed in item_families.h; ITEM_CONTENT_STANDARD.md says how
// a family is laid out and how a row reads.
#pragma once

#include "core/content/defs.h"

namespace mal::item_table {

using IE = ItemEffect;

inline constexpr ItemDef kDishItems[] = {
    // Created when Null_Noodles and Pwn-zu Sauce are combined (Hacker MERGE HUB).
    {"pwnzu_patched_noodles", "Pwnzu-Patched Noodles", ItemDef::Type::Food,
     ItemDef::Rarity::Uncommon,
     "Fixed all the problems in one go. Grandma Yubi would be proud.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 100}, {IE::Kind::Frag, -100}, {IE::Kind::Happy, 100}}, /*combatHeal=*/0},

    // Created when Tor-Tilla Chip and OSI Dip are combined (Hacker MERGE HUB): the
    // eighth layer OSI Dip was missing. Fills every stat at once.
    {"fully_stacked_nachos", "Fully-Stacked Nachos", ItemDef::Type::Food,
     ItemDef::Rarity::Rare,
     "Chip to topping and you made each layer yourself.",
     ItemDef::Context::Anytime, {{IE::Kind::Hunger, 100}, {IE::Kind::Frag, -100}, {IE::Kind::Happy, 100}}, /*combatHeal=*/0},

    // Hashed Browns + Salted&Hashed Browns: the pantry's first cooked dish and its
    // second pass through the pan. Both are MERGE HUB outputs (game_internal.h's
    // kMergeRecipes) AND stocked at Moor-to-Moor: buying one is how a player MEETS
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
    // What the pantry is FOR. Every row below is a MERGE HUB output and nothing else:
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

    // EPIC. The upgrade that is not a stat: a permanently shorter Bandwidth regen.
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
    // dish, not a staple. Which is what a casserole is: yesterday's cooking, kept.
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

    // The fourth service: the rest of the kitchen. Filed by what they are rather than
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

    // EPIC: a race is decided by who gets there first, which is what a Speed point buys.
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

    // EPIC. A spare in the array is the whole idea of a Defence point: one more thing
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

    // EPIC: the pet keeps a Power point for life. The pun is the mechanic: an escalation
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

    // The condiments: small effects, but they are what the big plates are built on.
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

    // EPIC: the one Epic dish that grants no stat at all. A profiler doesn't make the
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

    // EPIC: the pet's max Health IS its buffer, and this is the drink that writes past
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
};
inline constexpr int kDishItemsCount = sizeof(kDishItems) / sizeof(kDishItems[0]);

}  // namespace mal::item_table
