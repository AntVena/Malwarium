// content_quotes.cpp — the DECRYPTOGRAM quote pool. One row per quote; see
// content_quotes.h for the field schema, the two rules a row must pass, and the wire
// number discipline.

#include "core/content/content_quotes.h"

#include "core/content/content_achievements.h"

namespace mal {

namespace {

const QuoteDef kQuotes[] = {
    {1, "ANY SUFFICIENTLY ADVANCED TECHNOLOGY IS EQUIVALENT TO MAGIC.",
     "ARTHUR C. CLARKE"},
    {2, "TECHNOLOGY IS ANYTHING THAT WASN'T AROUND WHEN YOU WERE BORN.", "ALAN KAY"},
    {3, "MACHINES TAKE ME BY SURPRISE WITH GREAT FREQUENCY.", "ALAN TURING"},
    {4, "THOSE WHO CAN IMAGINE ANYTHING, CAN CREATE THE IMPOSSIBLE.", "ALAN TURING"},
    {5, "IF A MACHINE IS EXPECTED TO BE INFALLIBLE, IT CANNOT ALSO BE INTELLIGENT.",
     "ALAN TURING"},

    // Gated, and the two gates are the whole point of the field: a quote about seeing
    // further than you have gone waits on the dive that proves you have, and a quote
    // about forgiveness over permission waits on the run that took it.
    {6, "WE CAN ONLY SEE A SHORT DISTANCE AHEAD, BUT WE CAN SEE PLENTY THERE THAT NEEDS TO BE DONE.",
     "ALAN TURING", ach::kDeepWebDepth8},
    {7, "IF IT'S A GOOD IDEA, GO AHEAD AND DO IT.", "GRACE HOPPER", ach::kGoneRogue},

    {13, "A PROGRAM THAT HAS NOT BEEN TESTED DOES NOT WORK.", "BJARNE STROUSTRUP"},
    {14, "PRIVACY IS AN INHERENT HUMAN RIGHT, AND A REQUIREMENT FOR HUMAN DIGNITY.",
     "BRUCE SCHNEIER"},
    {15, "IT IS POOR CIVIC HYGIENE TO INSTALL TECHNOLOGIES THAT COULD SOMEDAY FACILITATE A POLICE STATE.",
     "BRUCE SCHNEIER"},
    {16, "A BRILLIANT SOLUTION TO THE WRONG PROBLEM CAN BE WORSE THAN NO SOLUTION AT ALL.",
     "DON NORMAN"},
    {17, "WALKING ON WATER AND DEVELOPING SOFTWARE FROM A SPECIFICATION ARE EASY IF BOTH ARE FROZEN.",
     "EDWARD V. BERARD"},
    {21, "PROGRAMS MUST BE WRITTEN FOR PEOPLE TO READ, AND ONLY INCIDENTALLY FOR MACHINES TO EXECUTE.",
     "HAROLD ABELSON"},
    {22, "HARDWARE: THE PARTS OF A COMPUTER THAT CAN BE KICKED.", "JEFF PESIS"},
    {23, "IT DOESN'T MATTER WHAT YOU KNOW, WHAT MATTERS IS WHEN YOU KNOW.", "KATHY SIERRA"},
    {24, "PERL - THE ONLY LANGUAGE THAT LOOKS THE SAME BEFORE AND AFTER RSA ENCRYPTION.",
     "KEITH BOSTIC"},
    {25, "ONE OF MY MOST PRODUCTIVE DAYS WAS THROWING AWAY 1000 LINES OF CODE.",
     "KEN THOMPSON"},
    {26, "IT IS EASIER TO PORT A SHELL THAN A SHELL SCRIPT.", "LARRY WALL"},
    {27, "TALK IS CHEAP. SHOW ME THE CODE.", "LINUS TORVALDS"},
    {29, "TO ITERATE IS HUMAN, TO RECURSE DIVINE.", "PETER DEUTSCH"},
    {31, "GOOD CODE IS ITS OWN BEST DOCUMENTATION.", "STEVE MCCONNELL"},
    {32, "I DON'T CARE IF IT WORKS ON YOUR MACHINE! WE ARE NOT SHIPPING YOUR MACHINE!",
     "VIDIU PLATON"},

    {35, "FREE SOFTWARE IS A MATTER OF LIBERTY, NOT PRICE.", "RICHARD STALLMAN"},
    {36, "GIVEN ENOUGH EYEBALLS, ALL BUGS ARE SHALLOW.", "ERIC S. RAYMOND"},
    {37, "IF PRIVACY IS OUTLAWED, ONLY OUTLAWS WILL HAVE PRIVACY.", "PHIL ZIMMERMANN"},
    {38, "THE ONLY WAY TO LEARN A NEW PROGRAMMING LANGUAGE IS BY WRITING PROGRAMS IN IT.",
     "DENNIS RITCHIE"},
    {39, "NEVER TRUST A COMPUTER YOU CAN'T THROW OUT A WINDOW.", "STEVE WOZNIAK"},
    {40, "THE POWER OF THE WEB IS IN ITS UNIVERSALITY.", "TIM BERNERS-LEE"},
    {41, "THE MOST DANGEROUS PHRASE IN THE LANGUAGE IS: WE'VE ALWAYS DONE IT THIS WAY.",
     "GRACE HOPPER"},
    {42, "THE INTERNET IS FOR EVERYONE, BUT IT WON'T BE UNLESS WE MAKE IT SO.", "VINT CERF"},
    {43, "INFORMATION IS POWER. BUT LIKE ALL POWER, THERE ARE THOSE WHO WANT TO KEEP IT FOR THEMSELVES.",
     "AARON SWARTZ"},
    {44, "SOME PEOPLE, WHEN CONFRONTED WITH A PROBLEM, THINK: I KNOW, I'LL USE REGULAR EXPRESSIONS. NOW THEY HAVE TWO PROBLEMS.",
     "JAMIE ZAWINSKI"},
    {45, "FOCUS IS A MATTER OF DECIDING WHAT THINGS YOU'RE NOT GOING TO DO.", "JOHN CARMACK"},
    {46, "TREAT YOUR PASSWORD LIKE YOUR TOOTHBRUSH. DON'T LET ANYBODY ELSE USE IT, AND GET A NEW ONE EVERY SIX MONTHS.",
     "CLIFF STOLL"},
    {47, "IT HAS BECOME APPALLINGLY OBVIOUS THAT OUR TECHNOLOGY HAS EXCEEDED OUR HUMANITY.",
     "ALBERT EINSTEIN"},
    {48, "WHEN IN DOUBT, USE BRUTE FORCE.", "KEN THOMPSON"},
    {49, "CODE IS READ MUCH MORE OFTEN THAN IT IS WRITTEN.", "GUIDO VAN ROSSUM"},
    {50, "C IS QUIRKY, FLAWED, AND AN ENORMOUS SUCCESS.", "DENNIS RITCHIE"},
    {51, "PROPRIETARY SOFTWARE TENDS TO HAVE MALICIOUS FEATURES.", "RICHARD STALLMAN"},
    {52, "SECURITY IS A PROCESS, NOT A PRODUCT.", "BRUCE SCHNEIER"},
    {53, "THE BEST WAY TO PREDICT THE FUTURE IS TO INVENT IT.", "ALAN KAY"},
    {54, "PRIVACY IS NECESSARY FOR AN OPEN SOCIETY IN THE ELECTRONIC AGE.", "ERIC HUGHES"},
    {55, "THERE ARE ONLY TWO HARD THINGS IN COMPUTER SCIENCE: CACHE INVALIDATION AND NAMING THINGS.",
     "PHIL KARLTON"},
    {56, "DEBUGGING IS TWICE AS HARD AS WRITING THE CODE IN THE FIRST PLACE.", "BRIAN KERNIGHAN"},
};

}  // namespace

const QuoteDef* quotes() { return kQuotes; }

int quoteCount() { return static_cast<int>(sizeof(kQuotes) / sizeof(kQuotes[0])); }

int quoteIndexByWire(int wire) {
    for (int i = 0; i < quoteCount(); ++i)
        if (kQuotes[i].wire == wire) return i;
    return -1;
}

// The prize ladder — the dish each solve teaches, in payout order. See the banner in
// the header for why these are all recipes and why the order is the one it is.
namespace {
const char* const kQuotePrizeLadder[] = {
    // The two starters, then the pantry's own fry-ups.
    "pwnzu_patched_noodles",
    "fully_stacked_nachos",
    "cracquettes",
    "hackshuka",
    "serial_bar",
    "applet_turnover",
    // The Browns pair carries its own "have you met the dish" gate, so it can be
    // skipped and come back around later without holding the rest of the ladder up.
    "hashed_browns",
    "salted_hashed_browns",
    // The three that heal in combat, and the roast that strips the most Fragmentation
    // — the dishes worth walking a long pool of quotes for.
    "macrol_fry_up",
    "vanilla_java_roast",
    "gnulash",
    // The second service, and the ladder's real endgame. RISCotto and LANsagne are the
    // two dishes whose ingredients tell the same joke as their names, so they lead it;
    // Cacherole wants a dish already cooked, so a player who has come this far has one.
    // Tiramisudo is last on purpose: it is the only food that permanently upgrades the
    // pet eating it, and nothing else in the game hands that over at any price.
    "riscotto",
    "ramen",
    "forkaccia",
    "core_dumplings",
    "lansagne",
    "cacherole",
    "tiramisudo",
    // The third service. Portridge leads it because it is the cheapest method in the
    // kitchen — one Bootmeal, one bowl — and Halloumi, World is the one anybody can
    // cook the moment they own it. The two that want a dish already cooked (Chrootons)
    // or the scarcest shelf (Semaphreddo) sit at the back, and the last three are the
    // biggest plates the pantry can make.
    "portridge",
    "halloumi_world",
    "nan_bread",
    "bisectuits",
    "lossy_lassi",
    "emacsaroni",
    "cod_review",
    "spaghetti_code",
    "chrootons",
    "quicksortbet",
    "gzipacho",
    "semaphreddo",
    "peking_duck_typing",
    "recursive_turducken",
};
}  // namespace

const char* const* quotePrizeLadder() { return kQuotePrizeLadder; }

int quotePrizeLadderCount() {
    return static_cast<int>(sizeof(kQuotePrizeLadder) / sizeof(kQuotePrizeLadder[0]));
}

QuoteReward quoteFallbackReward() {
    return {QuoteReward::Kind::RigGrant, 1, "bandwidth"};
}

}  // namespace mal
