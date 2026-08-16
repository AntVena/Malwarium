// content_tournament.cpp — ROCK THE DOCK's name, briefing and entrant handles. See
// content_tournament.h for the arena, the bracket's shape and every magnitude the roll
// reads.

#include "core/content/content_tournament.h"

namespace mal {

const char* const kTourneyName = "ROCK THE DOCK";

// The briefing, in the order it is read. It answers the four questions the arena's own
// row cannot: where am I, who are these people, what is different about fighting them,
// and what am I risking. Written to be read once and then skipped forever, so it leads
// with the thing that is actually novel — the opponents are real pets — rather than
// with the rules.
//
// Second person and present tense, like every other authored string on the device.
// Sections are kept SHORT on purpose — the page flows whole rows only (prose_page.h),
// so a section that runs past half the panel leaves the rest of the screen empty
// rather than seating a second one. Four short sections read as a briefing; two long
// ones read as a wall with a scrollbar.
const TourneyBriefDef kTourneyBrief[] = {
    {"THE DOCK",
     "The Bayou's cracked-key harbour runs a season. Eight operators, one bracket, "
     "single elimination."},
    {"THE FIELD",
     "Your rivals are not malbeasts. Each is real petware off the hatchable roster, "
     "any level up to 60."},
    {"SCOUT THEM",
     "Hold B on the bracket to read an entrant's moves, mods and stat spread before "
     "you have to face it."},
    {"BOTH SIDES CHEAT",
     "You bring an Exploit. So do they - fired without a picker, at a moment they "
     "have already picked."},
    {"THE TELL",
     "Firing one costs a turn. Some open with it; some wait until they are cornered. "
     "Learn which."},
    {"NO WAY OUT",
     "A bout has no retreat: win, or you are out of the draw. Between bouts you may "
     "leave and come back."},
    {"THE PURSE",
     "A won bout pays nothing. Take the whole bracket and it lands at once - Bits, "
     "experience, and one mod."},
};
const int kTourneyBriefCount =
    static_cast<int>(sizeof(kTourneyBrief) / sizeof(kTourneyBrief[0]));

// Warez/demoscene nicknames — the register another operator would actually pick for
// themselves, which is what makes an entrant read as a person rather than as a rolled
// stat block. None names a real handle; each is a joke about the scene's own plumbing
// (a nuked release, a bad CRC, a ratio nobody can hold).
const char* const kTourneyHandles[] = {
    "KEYGEN_KID",   "NFO_PRIEST",   "RAR_BARON",    "SPLIT_VOLUME",
    "CRC_MISMATCH", "NUKED_TWICE",  "DUPE_HUNTER",  "RATIO_WIDOW",
    "SFV_SHERIFF",  "TOPSITE_TED",  "SEEDBOX_SAL",  "LEECHFINGER",
    "PARITY_PETE",  "PRE_TIMER",    "SCENE_TAX",    "HASH_COLLIE",
    "PADDED_ZIP",   "BURN_PROOF",   "FXP_FERAL",    "GREETZ_GRETA",
    "ISO_9660",     "SILENT_SWAP",  "TRACKER_TAM",  "ZERO_SEEDER",
};
const int kTourneyHandleCount =
    static_cast<int>(sizeof(kTourneyHandles) / sizeof(kTourneyHandles[0]));

const char* tourneyHandleName(int index) {
    return (index >= 0 && index < kTourneyHandleCount) ? kTourneyHandles[index] : "";
}

}  // namespace mal
