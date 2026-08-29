// content_riddles.cpp — the SHIBBOLETH pool. One row per riddle; see
// content_riddles.h for the field schema, the two rules a row must pass, and why the
// first reply is always the true one.

#include "core/content/content_riddles.h"

namespace mal {

namespace {

// A guardian is a thing that watches a network and has done so for a long time, so what
// it asks about is the machinery it lives inside: the shapes of storage, the shapes of
// failure, and the small rituals by which one process decides another is who it claims
// to be. The voice is deliberately old — a guardian does not know it is a program.
const RiddleDef kRiddles[] = {
    // --- What a thing IS ---------------------------------------------------
    {"I HOLD WHAT YOU GIVE ME AND RETURN IT BACKWARDS. NAME ME.",
     {"A STACK", "A QUEUE", "A HEAP"}},
    {"I TAKE THE FIRST TO ARRIVE AND MAKE THE REST WAIT THEIR TURN.",
     {"A QUEUE", "A STACK", "A CACHE"}},
    {"I ANSWER EVERY QUESTION WITH A SMALLER ONE OF MY OWN.",
     {"RECURSION", "ITERATION", "POLLING"}},
    {"I AM THE NAME YOU SPEAK BEFORE THE NUMBER YOU MEAN.",
     {"A HOSTNAME", "A PORT", "A CHECKSUM"}},
    {"I AM THE DOOR THE BUILDER LEFT UNLOCKED BEHIND THEM.",
     {"A BACKDOOR", "A FIREWALL", "A GATEWAY"}},
    {"I AM SWEET, AND EVERYTHING THAT TASTES ME IS WRITTEN DOWN.",
     {"A HONEYPOT", "A COOKIE", "A SANDBOX"}},
    {"CUT ME IN TWO AND YOU HAVE MADE TWO OF ME.",
     {"A WORM", "A TROJAN", "A ROOTKIT"}},
    {"I AM THE COPY THAT OUTLIVES WHAT I WAS COPIED FROM.",
     {"A BACKUP", "A CACHE", "A SHADOW"}},

    // --- What a thing DOES -------------------------------------------------
    {"I SPEAK ONLY TO SAY THAT I AM STILL HERE.",
     {"A HEARTBEAT", "A HANDSHAKE", "A CHECKSUM"}},
    {"I LISTEN TO EVERYTHING AND I ANSWER NOTHING.",
     {"A PASSIVE SCAN", "A BROADCAST", "A PROBE"}},
    {"I REPEAT WHAT I HEAR UNTIL NO ONE ELSE CAN BE HEARD.",
     {"A FLOOD", "AN ECHO", "A RELAY"}},
    {"I WEAR YOUR NAME SO THAT YOUR FRIENDS WILL SPEAK TO ME.",
     {"A SPOOF", "A PROXY", "A TUNNEL"}},
    {"I COUNT HOW FAR YOU HAVE COME AND NEVER HOW LONG IT TOOK.",
     {"A HOP COUNT", "A TIMESTAMP", "A LATENCY"}},
    {"I ASK A QUESTION ONLY TO LEARN WHETHER YOU CAN ANSWER IT.",
     {"A CHALLENGE", "A GREETING", "A CENSUS"}},

    // --- What goes WRONG ---------------------------------------------------
    {"I WAIT FOREVER FOR THE ONE WHO IS WAITING FOR ME.",
     {"A DEADLOCK", "A TIMEOUT", "A STALL"}},
    {"I AM TRUE, AND THEN YOU LOOK AGAIN AND I AM NOT.",
     {"A RACE", "A FAULT", "A DRIFT"}},
    {"I GROW LARGER EVERY TIME YOU FORGET ME.",
     {"A LEAK", "A LOOP", "A BACKLOG"}},
    {"I COUNT FROM NOTHING AND STOP ONE STEP TOO SOON.",
     {"AN OFF BY ONE", "AN OVERFLOW", "A ROUNDING"}},
    {"I AM WHAT YOU PROMISED TO DO LATER, AND I CHARGE INTEREST.",
     {"DEBT", "A BACKLOG", "A DEFERRAL"}},
    {"POUR ENOUGH INTO ME AND I WILL WRITE WHERE I WAS NOT ASKED TO.",
     {"A BUFFER", "A PIPE", "A SOCKET"}},

    // --- Secrets, and the keeping of them ----------------------------------
    {"MY KEY OPENS ONLY WHAT MY TWIN HAS SHUT.",
     {"A KEY PAIR", "A PASSPHRASE", "A ONE TIME PAD"}},
    {"USE ME TWICE AND I AM WORTH NOTHING AT ALL.",
     {"A NONCE", "A TOKEN", "A COOKIE"}},
    {"I SAY WHO YOU ARE. I DO NOT SAY WHAT YOU MAY DO.",
     {"IDENTITY", "PERMISSION", "OWNERSHIP"}},
    {"I AM ADDED TO A SECRET SO THAT TWO ALIKE ARE NOT.",
     {"A SALT", "A CIPHER", "A DIGEST"}},
    {"I CAN PROVE A THING WAS NOT CHANGED. I CANNOT SAY WHAT IT WAS.",
     {"A HASH", "A KEY", "A LOG"}},
    {"I DIE AT THE MOMENT I AM UNDERSTOOD.",
     {"A RIDDLE", "A RUMOUR", "A WARNING"}},
};

}  // namespace

const RiddleDef* riddles() { return kRiddles; }
int riddleCount() { return static_cast<int>(sizeof(kRiddles) / sizeof(kRiddles[0])); }

}  // namespace mal
