// content_riddles.cpp — the SHIBBOLETH pool. One row per riddle; see
// content_riddles.h for the field schema, the two rules a row must pass, and why the
// first reply is always the true one.

#include "core/content/content_riddles.h"

namespace mal {

namespace {

// A guardian is a thing that watches a network and has done so for a very long time. It
// asks the way the old riddles ask — a door, a mirror, a shadow, a candle burning down —
// and it does not know it is a program.
//
// TWO RULES, AND THE SECOND IS THE ONE THAT KEEPS GETTING BROKEN.
//
// 1. IT MUST BE A RIDDLE, not a definition. A riddle MISDIRECTS: the imagery points at a
//    person, a place, an animal, and every line turns out to have been literally true
//    only once you hold the answer. A row that describes its answer in its answer's own
//    terms is trivia in a riddle's clothes. If a row can be answered without re-reading
//    it, it is not finished.
//
// 2. THE ANSWER MUST BE A WORD ANYONE KNOWS. A door, an echo, a name, nothing. NOT a
//    nonce, an off-by-one, a passive scan. A player who solves the riddle and still
//    cannot answer it has been beaten by vocabulary rather than by the puzzle, and that
//    is the worst outcome this screen has: the whole point of learning the CANT is that
//    reading the question is what wins it. The strangeness belongs in the guardian's
//    VOICE, never in the word it wants back.
//
// So the machine shows up as flavour and not as jargon. A stale record is a ghost that
// still answers to a name; the 'net is a web of nothing but lines and gaps. Anyone can
// answer those, and they still sound like something a watching thing would ask.
//
// THE DECOYS carry the rest. Each is a plain word the imagery could honestly have been
// pointing at, so a fluent pet still has to choose, and a blind pick stays a real gamble.
const RiddleDef kRiddles[] = {
    // --- Things that copy you ----------------------------------------------
    {"I COPY YOU EXACTLY AND I HAVE LEARNED NOTHING. LOOK AWAY AND I FORGET YOU.",
     {"A MIRROR", "A SHADOW", "A WINDOW"}},
    {"I SPEAK ONLY IN YOUR VOICE, AND ONLY ONCE YOU HAVE FINISHED. I HAVE NO IDEAS.",
     {"AN ECHO", "A PARROT", "A MIRROR"}},
    {"I FOLLOW YOU EVERYWHERE AND HAVE NEVER TOUCHED YOU. LIGHT MAKES ME AND UNMAKES ME.",
     {"A SHADOW", "A GHOST", "A FOOTPRINT"}},
    {"THERE ARE TWO OF US AND ONLY ONE WAS BORN. NEITHER OF US WILL SAY WHICH.",
     {"A TWIN", "A REFLECTION", "A RUMOUR"}},
    {"I AM THE SHAPE OF SOMEONE NO LONGER HERE, AND I STILL ANSWER TO THEIR NAME.",
     {"A GHOST", "A MEMORY", "A STATUE"}},

    // --- Things that let you in, or do not ---------------------------------
    {"I AM THE ONLY PART OF A WALL THAT AGREES WITH YOU.",
     {"A DOOR", "A WINDOW", "A CRACK"}},
    {"I HAVE NO OPINION AND I REFUSE EVERYONE EQUALLY.",
     {"A WALL", "A GUARD", "A LOCK"}},
    {"I AM SMALL AND CUT WITH TEETH, AND I HAVE NEVER EATEN. I OPEN A HOUSE I DO NOT LIVE IN.",
     {"A KEY", "A TOOTH", "A THIEF"}},
    {"EVERYONE WHO WANTS IN MUST MAKE ME AN OFFER, AND I ACCEPT ONLY ONE.",
     {"A LOCK", "A DOOR", "A TOLL"}},
    {"I LET IN THE WORLD AND KEEP OUT THE WEATHER. BREAK ME AND I DO NEITHER.",
     {"A WINDOW", "A DOOR", "A ROOF"}},
    {"I AM USELESS LYING DOWN.",
     {"A LADDER", "A BRIDGE", "A DOOR"}},
    {"I AM A HOUSE WITH NO DOOR. WHOEVER IS INSIDE MUST BREAK THE WALLS TO LEAVE.",
     {"AN EGG", "A TOMB", "A CAGE"}},

    // --- Things that are not there -----------------------------------------
    {"SAY MY NAME AND I AM GONE.",
     {"SILENCE", "A SECRET", "THE DARK"}},
    {"THE MORE YOU TAKE FROM ME, THE LARGER I BECOME.",
     {"A HOLE", "A DEBT", "A WOUND"}},
    {"THE POOR HAVE ME. THE RICH NEED ME. EAT ME AND YOU WILL DIE.",
     {"NOTHING", "DUST", "A SECRET"}},
    {"I AM IN EVERY ROOM BEFORE YOU ARRIVE, AND I LEAVE THE MOMENT YOU LOOK.",
     {"THE DARK", "DUST", "SILENCE"}},
    {"I AM ALWAYS COMING AND I NEVER ARRIVE.",
     {"TOMORROW", "A PROMISE", "THE HORIZON"}},

    // --- Things that carry, and things that hold ---------------------------
    {"I AM MADE OF NOTHING BUT LINES AND GAPS, AND STILL I HOLD THINGS.",
     {"A WEB", "A CAGE", "A BASKET"}},
    {"PULL BOTH MY ENDS AND I ONLY TIGHTEN. BE GENTLE AND I COME APART.",
     {"A KNOT", "A ROPE", "A TRAP"}},
    {"I AM ONLY AS STRONG AS MY WORST PART, AND EVERYONE KNOWS WHICH ONE THAT IS.",
     {"A CHAIN", "A ROPE", "A BRIDGE"}},
    {"EVERYONE WALKS ON ME ON THEIR WAY SOMEWHERE, AND I AM GOING NOWHERE AT ALL.",
     {"A BRIDGE", "A ROAD", "A STAIR"}},
    {"I GO EVERYWHERE AND I HAVE NEVER MOVED.",
     {"A ROAD", "A RIVER", "A MAP"}},
    {"I HOLD CITIES WITH NO PEOPLE IN THEM AND RIVERS WITH NO WATER.",
     {"A MAP", "A DREAM", "A RUIN"}},

    // --- Things that pass between people -----------------------------------
    {"I BELONG TO YOU, AND EVERYONE ELSE USES ME MORE THAN YOU DO.",
     {"A NAME", "A VOICE", "A SHADOW"}},
    {"I TRAVEL FURTHER THAN THE TRUTH AND I ALWAYS GET THERE FIRST.",
     {"A LIE", "A RUMOUR", "AN ECHO"}},
    {"TELL ME TO ONE PERSON AND I AM HALF GONE. TELL TWO AND I AM NO LONGER YOURS.",
     {"A SECRET", "A RUMOUR", "A PROMISE"}},
    {"I AM MADE OF BREATH AND NOTHING ELSE, AND MEN HAVE DIED STILL HOLDING ME.",
     {"A PROMISE", "A NAME", "A SONG"}},
    {"I VISIT EVERYONE AND I AM NEVER REMEMBERED CORRECTLY.",
     {"A DREAM", "A GHOST", "A RUMOUR"}},
    {"I AM MANY AND I AM SPOKEN OF AS ONE. TAKE ONE AWAY AND I DO NOT NOTICE.",
     {"A CROWD", "AN ARMY", "A SWARM"}},

    // --- Things that run out -----------------------------------------------
    {"THE LONGER I STAND, THE SHORTER I GROW.",
     {"A CANDLE", "A SHADOW", "A DAY"}},
    {"THE MORE OF ME YOU TAKE, THE MORE OF ME YOU LEAVE BEHIND.",
     {"FOOTSTEPS", "A TRAIL", "A SHADOW"}},
    {"I WAS A MOUNTAIN ONCE. I AM IN YOUR LUNGS NOW.",
     {"DUST", "ASH", "SAND"}},
    {"I HAVE A FACE AND TWO HANDS, AND I HAVE NEVER ONCE WAVED.",
     {"A CLOCK", "A STATUE", "A MIRROR"}},
    {"I HAVE A MOUTH AND NEVER SPEAK, A BED AND I NEVER SLEEP.",
     {"A RIVER", "A CAVE", "A VALLEY"}},
    {"I DIE AT THE MOMENT I AM UNDERSTOOD.",
     {"A RIDDLE", "A SECRET", "A WARNING"}},
};

}  // namespace

const RiddleDef* riddles() { return kRiddles; }
int riddleCount() { return static_cast<int>(sizeof(kRiddles) / sizeof(kRiddles[0])); }

}  // namespace mal
