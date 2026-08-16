// content_tournament.cpp — THE COMPO's entrant handles. See content_tournament.h for
// the arena, the bracket's shape and every magnitude the roll reads.

#include "core/content/content_tournament.h"

namespace mal {

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
