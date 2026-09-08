#include "core/content/areas/darkweb_crawl/area.h"

#include "core/content/areas/area_defs.h"

namespace mal {

// The level half is the dive's, so a pet meets one curve on both zones; what differs is
// that there is no foothold and the linear depth term runs twice as fast. Both halves are
// measured against the crawl's OWN population and its own scramble — see area.h.
const int kDarkWebEnemyLevelOffset = 0;
const int kDarkWebDepthLevelPerLog2 = 2;
const int kDarkWebBudgetPct = 100;
const int kDarkWebDepthPointsPerN = 2;
const int kDarkWebDepthBitsPctPerLog2 = 48;
const int kDarkWebDepthBitsMaxPct = 768;

// Every 16 wins deep, which is one past the MEDIAN run: over 2000 measured crawls a
// ladder-cleared pet reaches depth 14 (mean 18.6, p90 41), so a sigil costs a
// better-than-median run rather than being handed out for showing up. That works out at
// 0.73 sigils a run and about 35 runs for the whole 26-letter Cant — a campaign, next to
// the radio's one capture per handshake, which is the balance this door is meant to keep:
// a player with no network in range is not locked out of the Cant, and one with an aerial
// is never made to farm here instead.
const int kDarkWebSigilEveryN = 16;

const char* const kDarkWebIcon = "ICON_SECTOR_DARKWEB_CRAWL";

// The terminal zone's shelf: the Lode's own rank-6 set, plus the Dive's hard-gated line
// build-arounds. Reaching it opens the deep end of every line at once, which is what the
// last zone on the map owes a player who got here without being able to read the menu.
const char* const kAreaModsDarkWeb[] = {
    "crib_sheet",     "vault_door",       "spoof_relay",    "fork_farm",
    "false_flag",     "recompiler",       "phishing_rod",   "extortion_ledger",
    "ring_zero_shim", "replication_bus",  "mutation_engine",
};
const int kAreaModsDarkWebCount = arrLen(kAreaModsDarkWeb);

// The staple set, plus the five unbuyable USBs the Dive used to be the only source of.
// They move here with the terminal slot: the price of overruling how a pet was raised is
// a trip to the end of the map, and the end of the map is no longer the Dive.
const LootEntry kWildLootDarkWeb[] = {
    {"dyno_nuggets"}, {"tortilla_chip"}, {"backup_drive"}, {"disk_scrubber"},
    {"pwnzu_sauce"},
    {"sandbox_usb"}, {"bad_usb"}, {"signed_usb"}, {"halt_usb"}, {"eject_usb"},
};
const int kWildLootDarkWebCount = arrLen(kWildLootDarkWeb);

}  // namespace mal
