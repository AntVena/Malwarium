#include "core/model/hacker_rank.h"

#include "tunables.h"

namespace mal {

int hackerRankXp(int networksSeen) {
    if (networksSeen < 0) networksSeen = 0;
    return networksSeen * kHackerRankXpPerNetwork;
}

int hackerRankForNetworksSeen(int networksSeen) {
    return hackerRankXp(networksSeen) / kHackerRankXpPerRank;
}

int hackerRankXpIntoRank(int networksSeen) {
    return hackerRankXp(networksSeen) % kHackerRankXpPerRank;
}

namespace {
// The growable rank-title ladder. {unlockRank, title}, ordered by unlockRank
// ASCENDING. APPEND freely at the end to add higher titles — the count is derived
// (kRankTierCount) and hackerRankTitle() returns the highest tier at or beneath the
// numeric rank, so a longer list just raises the ceiling. The unlock ranks ARE the
// tunable spacing: sequential today (one new title per rank), but widen them (e.g.
// {0,3,7,12,…}) to gate the higher titles behind more exploration without touching
// any logic. Nothing outside this table assumes a fixed length.
//
// The ladder is a genuine SKILL/POWER progression,
// not a set of horizontal "hat" roles — grey/white/black hats describe the
// *stance* a hacker takes, not their level, so they no longer appear here as tiers.
// SCRIPT KIDDIE also left the ladder (it becomes an achievement). PACKET RAT anchors
// rank 0; DEEPWEB DEITY caps the climb (ties to the endless DeepWeb Dive endgame).
// Kept to a few-dozen escalating puns for now; a dedicated session grows the pool to
// ~100. Titles are ≤16 chars to match the on-screen ceiling ("R{n} {title}").
struct RankTier { int unlockRank; const char* title; };
constexpr RankTier kRankTiers[] = {
    {0,  "PACKET RAT"},
    {1,  "PING PEON"},
    {2,  "LOG LURKER"},
    {3,  "SHELL SPROUT"},
    {4,  "CACHE CADET"},
    {5,  "GREP GREENHORN"},
    {6,  "PROXY PROWLER"},
    {7,  "SUBNET SCOUT"},
    {8,  "DAEMON DABBLER"},
    {9,  "ROOTKIT ROOKIE"},
    {10, "BUFFER BANDIT"},
    {11, "CIPHER SLINGER"},
    {12, "EXPLOIT ADEPT"},
    {13, "KERNEL KNAVE"},
    {14, "PACKET PHANTOM"},
    {15, "ZERO-DAY DUELIST"},
    {16, "FIREWALL FELLER"},
    {17, "CRYPTO CORSAIR"},
    {18, "ROOT REAVER"},
    {19, "NETWORK NINJA"},
    {20, "STACK OVERLORD"},
    {21, "SEGFAULT MAGE"},
    {22, "BINARY BARON"},
    {23, "MAINFRAME MAGUS"},
    {24, "KERNEL KINGPIN"},
    {25, "SPECTRE LORD"},
    {26, "CIPHER CZAR"},
    {27, "ROOT ARCHON"},
    {28, "NULL EMPEROR"},
    {29, "DEEPWEB DEITY"},
};
constexpr int kRankTierCount =
    static_cast<int>(sizeof(kRankTiers) / sizeof(kRankTiers[0]));
}  // namespace

int hackerRankTierCount() { return kRankTierCount; }

int hackerRankTierUnlock(int tier) {
    return (tier >= 0 && tier < kRankTierCount) ? kRankTiers[tier].unlockRank : -1;
}

const char* hackerRankTitle(int rank) {
    // The nearest title at or beneath the numeric rank: scan for the highest tier
    // whose unlockRank ≤ rank. Tiers are ascending, so the last qualifying one wins;
    // stop early once a threshold overshoots. Below the first tier → the first title;
    // above the last → the last (the numeric rank keeps climbing, the title caps at
    // the top of the ladder — a title is "unlocked" once its rank is reached).
    const char* title = kRankTiers[0].title;
    for (int i = 0; i < kRankTierCount; ++i) {
        if (rank >= kRankTiers[i].unlockRank) title = kRankTiers[i].title;
        else break;
    }
    return title;
}

} // namespace mal
