// content_riddles.cpp — the SHIBBOLETH pool. One row per riddle; see
// content_riddles.h for the field schema, the two rules a row must pass, and why the
// first reply is always the true one.

#include "core/content/content_riddles.h"

namespace mal {

namespace {

// A guardian is a thing that watches a network and has done so for a very long time, so
// what it asks about is the machinery it lives inside. But it does not ask the way a
// manual would. It asks the way the old riddles do — a doorway, a fence, a gift, a
// letter home — and the answer turns out to be a computer, which the guardian does not
// know it is talking about.
//
// THE FORM, and it is the whole difference between this pool and a quiz. A riddle
// MISDIRECTS: the imagery points at a person, a place, an animal, and every line is
// literally true of the answer only once you have it. "I sat in your room all evening
// and touched nothing" is a houseguest until it is a passive scan. A row that merely
// DEFINES its answer ("I hold what you give me and return it backwards") is trivia
// wearing a riddle's clothes — it is answerable only by someone who already knows the
// word, which makes reading the Cant worth nothing. If a row can be answered without
// re-reading it, it is not finished.
//
// THE DECOYS carry the other half. Each is a real term that the imagery could plausibly
// have been pointing at — a collision for a race, a fork for a worm, reflection for
// recursion — so a fluent pet still has to think, and a blind pick stays a real gamble.
const RiddleDef kRiddles[] = {
    // --- Things that wait, and things that go wrong ------------------------
    {"TWO OF US STAND IN A DOORWAY, EACH WAITING FOR THE OTHER TO PASS. WE ARE PATIENT.",
     {"A DEADLOCK", "A TIMEOUT", "A STALL"}},
    {"I AM THE MOMENT YOU STOP BELIEVING AN ANSWER IS COMING.",
     {"A TIMEOUT", "A DEADLOCK", "A DROP"}},
    {"WE ARRIVED TOGETHER AND BOTH SWEAR WE WERE FIRST. ASK AGAIN, THE STORY CHANGES.",
     {"A RACE", "A COLLISION", "A FAULT"}},
    {"I WAS TOLD TO WALK UNTIL I GREW TIRED. I HAVE NO LEGS.",
     {"AN INFINITE LOOP", "A DEADLOCK", "A STALL"}},
    {"I BUILT A FENCE A HUNDRED PACES LONG, A POST AT EVERY PACE. I AM ONE SHORT.",
     {"AN OFF BY ONE", "AN OVERFLOW", "A ROUNDING"}},
    {"KEEP POURING. THE CUP DOES NOT COMPLAIN. THE TABLE IS WHERE THE WINE WILL BE.",
     {"AN OVERRUN", "A LEAK", "A FLUSH"}},
    {"I WALKED EAST UNTIL I CAME HOME FROM THE WEST. I NEVER ONCE TURNED ROUND.",
     {"AN OVERFLOW", "A LOOP", "A ROUTE"}},
    {"I TAKE A LITTLE OF THE ROOM EACH TIME WE SPEAK, AND NEVER GIVE IT BACK.",
     {"A LEAK", "A LOOP", "A CACHE"}},
    {"EVERY HAND THAT REACHES FOR ME FALLS THROUGH. I AM NOT EMPTY. I AM NOT THERE.",
     {"A NULL", "A VOID", "A GAP"}},

    // --- Things that pretend -----------------------------------------------
    {"I WROTE TO YOUR MOTHER IN YOUR OWN HAND, AND SHE WROTE BACK TO ME.",
     {"A SPOOF", "A PROXY", "A RELAY"}},
    {"I WAS A GIFT. YOU CARRIED ME IN YOURSELF.",
     {"A TROJAN", "A WORM", "A ROOTKIT"}},
    {"I AM THE ONLY DOOR LEFT STANDING OPEN. WALK IN. YOU ARE EXPECTED.",
     {"A HONEYPOT", "A BACKDOOR", "A GATEWAY"}},
    {"THE BUILDER CUT ME FOR HIMSELF AND TOLD NO ONE. I HAVE NO LOCK.",
     {"A BACKDOOR", "A HONEYPOT", "A GATEWAY"}},
    {"THE DOOR HAS STOOD OPEN SINCE THE HOUSE WAS BUILT. TODAY IS THE FIRST DAY.",
     {"A ZERO DAY", "A BACKDOOR", "AN EXPLOIT"}},
    {"EVERYTHING YOU OWN IS STILL HERE. I HAVE PUT IT OUT OF REACH. I SELL LADDERS.",
     {"A RANSOM", "A LOCKOUT", "A PAYWALL"}},
    {"I AM TEN THOUSAND STRANGERS WHO AGREED ON ONE THING WITHOUT BEING ASKED.",
     {"A BOTNET", "A SWARM", "A QUORUM"}},

    // --- Things that remember ----------------------------------------------
    {"I HAVE SEEN YOUR FACE AND KEPT NO PICTURE OF IT, YET I WOULD KNOW YOU ANYWHERE.",
     {"A HASH", "A CIPHER", "A KEY"}},
    {"TWO MEN SAID THE SAME WORD, AND I MADE THEM STRANGERS.",
     {"A SALT", "A CIPHER", "A NONCE"}},
    {"YOU MADE ME FROM YOUR DOG AND THE YEAR YOU WERE BORN. SO DID NINE THOUSAND MORE.",
     {"A PASSWORD", "A NONCE", "A SALT"}},
    {"SPEND ME ONCE AND I AM GOLD. SPEND ME TWICE AND I AM NOTHING.",
     {"A NONCE", "A TOKEN", "A COOKIE"}},
    {"I HAVE A TWIN. WHAT I SHUT, ONLY THEY MAY OPEN. WE HAVE NEVER MET.",
     {"A KEY PAIR", "A PASSPHRASE", "A HANDSHAKE"}},
    {"I TURN YOUR LETTER INTO A FIELD OF STONES. YOUR FRIEND STILL SEES THE LETTER.",
     {"A CIPHER", "A HASH", "A DIGEST"}},
    {"NOBODY SPEAKS TO ME FOR YEARS. THEN I AM THE MOST IMPORTANT THING IN THE HOUSE.",
     {"A BACKUP", "A LOG", "A CACHE"}},
    {"I REMEMBER EVERY PROMISE YOU BROKE, AND MENTION IT ONLY AFTER THE FIRE.",
     {"A LOG", "A BACKUP", "A CACHE"}},
    {"I ANSWER FASTER THAN THE ONE WHO KNOWS. SOMETIMES FOR A HOUSE THAT BURNED DOWN.",
     {"A CACHE", "A PROXY", "A MIRROR"}},

    // --- Things that count, and things that carry --------------------------
    {"I STAND BETWEEN TWO MIRRORS AND COUNT WHAT I SEE, ONE FEWER EACH TIME, THEN STOP.",
     {"RECURSION", "REFLECTION", "ITERATION"}},
    {"I ASKED ONE FRIEND FOR HELP. HE ASKED TWO. BY MORNING THE TOWN WAS IN MY KITCHEN.",
     {"A DEPENDENCY", "A BOTNET", "A BROADCAST"}},
    {"I SAT IN YOUR ROOM ALL EVENING AND TOUCHED NOTHING. I KNOW EVERY NAME THAT SPOKE.",
     {"A PASSIVE SCAN", "A BROADCAST", "A PROBE"}},
    {"I HAVE NOTHING TO SAY AND I SAY IT EVERY SECOND. WHEN I STOP, THEY COME LOOKING.",
     {"A HEARTBEAT", "A HANDSHAKE", "A CHECKSUM"}},
    {"I LET NOBODY IN AND EVERYBODY OUT, AND I AM PROUD OF HALF OF THAT.",
     {"A FIREWALL", "A GATEWAY", "A ROUTER"}},
    {"CUT ME IN TWO AND YOU HAVE DOUBLED ME.",
     {"A WORM", "A FORK", "A TROJAN"}},
    {"I DIE AT THE MOMENT I AM UNDERSTOOD.",
     {"A RIDDLE", "A RUMOUR", "A WARNING"}},
};

}  // namespace

const RiddleDef* riddles() { return kRiddles; }
int riddleCount() { return static_cast<int>(sizeof(kRiddles) / sizeof(kRiddles[0])); }

}  // namespace mal
