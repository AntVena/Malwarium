// hacker_rank.h — the player/device-level Hacker Rank progression (
// resolves). Driven purely by lifetime unique networks seen —
// "breadth, not grind": revisiting a known network never advances rank. Rank is
// orthogonal to the doc-07 hacker classes (Red/Blue identity) — this is purely
// an exploration-progression tier, not a playstyle/allegiance choice.
//
// XP MODEL:
// every newly-discovered distinct network grants kHackerRankXpPerNetwork XP;
// kHackerRankXpPerRank XP = one rank (defaults ⇒ a rank every 10 networks),
// UNBOUNDED. Rank is a REWARD-ONLY track — it gates nothing (see expl_screen).
#pragma once

namespace mal {

// The rank TITLES are a growable, threshold-keyed ladder: every
// tier unlocks at a specific numeric rank, and the DISPLAYED title is the nearest
// tier at or beneath the current rank. New titles are just APPENDED to the end of
// the table (higher unlock ranks) — nothing assumes a fixed count, so the list can
// grow indefinitely while the numeric rank keeps climbing past the top title. rank 0
// ("Script Kiddie") is the start, held at zero networks seen.

// Total lifetime XP for `networksSeen` networks (= networksSeen * XP-per-network).
int hackerRankXp(int networksSeen);

// The (unbounded) rank at `networksSeen` — monotonic, never decreases as
// networksSeen grows. = totalXp / XP-per-rank.
int hackerRankForNetworksSeen(int networksSeen);

// XP accrued *into the current rank* (0..kHackerRankXpPerRank-1) — the progress
// toward the next rank, for a badge/progress readout.
int hackerRankXpIntoRank(int networksSeen);

// The number of named rank tiers currently defined — DERIVED from the title table,
// so appending a title grows it with no other edits. Callers must never hardcode
// the ladder length.
int hackerRankTierCount();

// The numeric rank at which tier `i` (0-based) unlocks, or -1 if out of range —
// exposed so UI/tests can reason about thresholds without duplicating the table.
int hackerRankTierUnlock(int tier);

// Punny rank title for a numeric `rank`: the nearest tier at or beneath it (the
// highest tier whose unlock rank ≤ `rank`). Never null — a rank below the first
// tier returns the first title; a rank past the last keeps the top title.
const char* hackerRankTitle(int rank);

} // namespace mal
