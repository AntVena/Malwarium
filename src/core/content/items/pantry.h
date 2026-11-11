// The pantry: the raw staples the MERGE HUB cooks with. Filed as their own family
// rather than under the kit's rarity headings because what makes one of these findable
// is being a STAPLE, not its tier: the tier only says how much a pet gets out of eating
// one raw, which is generally "not much, and sometimes less than nothing". Most carry an
// explicit dropWeight so the pantry isn't a flat shelf: what every kitchen is full of
// (Spam, Breadcrumbs) turns up far more often than the scarce things (Root Veg, Fresh
// Macrol).
//
// One of the item families listed in item_families.h; ITEM_CONTENT_STANDARD.md says how
// a family is laid out and how a row reads.
#pragma once

#include "core/content/defs.h"

namespace mal::item_table {

using IE = ItemEffect;

inline constexpr ItemDef kPantryItems[] = {
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

    // Boolean Cubes take no dropWeight: Common's own default is exactly the middle
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

    // Kernel Oil takes Uncommon's own default weight: the middle of its shelf.
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
     // high enough that hoarding it is a losing plan, which is the whole point of the
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
    // reach for. Same rule as the first shelf: a tier says how little a pet gets out
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
    // same numbers: the one recipe in the kitchen that changes nothing but where it
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
    // the kitchen reaches for. Same rule as the shelves above: the tier says how little
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
};
inline constexpr int kPantryItemsCount = sizeof(kPantryItems) / sizeof(kPantryItems[0]);

}  // namespace mal::item_table
