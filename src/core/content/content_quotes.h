// content_quotes.h — the DECRYPTOGRAM quote pool: what the board can ask you to crack.
//
// A row is a quote, who said it, and what has to be true before the pool will offer it.
// The rules of the board are core/model/cryptogram.h; this table is only the material,
// and nothing here knows how a letter opens.
//
// ADDING ONE IS ONE ROW. That is the design goal — this table is expected to grow into
// the hundreds, and a quote must never cost more than a line to add:
//
//     {35, "TALK IS CHEAP. SHOW ME THE CODE.", "LINUS TORVALDS"},
//
// TWO RULES A NEW ROW MUST PASS, both checked by the native content gate rather than by
// eye (test/test_native/test_cryptogram.cpp):
//   * It WRAPS into kCryptogramRows rows of kCryptogramCols cells. A character count is
//     the wrong check — wrapping wastes the tail of most lines, so a punctuation-heavy
//     96 can fail where a well-broken 100 passes. Write it, then run the gate.
//   * Its text is A-Z, digits, spaces and plain ASCII punctuation. Curly quotes and
//     dashes have no glyph in the face and would render as holes; normalise them.
//     Case is folded by the model, so authoring in caps is a convention, not a rule.
//
// WIRE NUMBERS — read before editing. The save stores each quote's state as a 2-bit
// field indexed by `wire`, not by row position, so rows may be reordered, regrouped by
// author, or removed without invalidating a save. In exchange: **assign the next unused
// number and never reuse or renumber one.** A retired quote's number stays burned. The
// gate asserts uniqueness and that every number is under kQuoteWireCap.
//
// Compiled-in and index-addressed, like the crews (content_crews.h) and the arcade
// cabinets — every call site is "the quote on row i".
//
// The starting rows come from the CC0 "Awesome IT Quotes" collection
// (github.com/victorlaerte/awesome-it-quotes), filtered to what fits the panel.
#pragma once

#include <cstdint>

namespace mal {

// What a solved board hands over besides Bits. A structured kind+magnitude+id triple
// rather than named fields, so the second and third kinds of prize are a new Kind and
// one applier case — the same shape AchievementReward uses, and for the same reason.
struct QuoteReward {
    enum class Kind : uint8_t {
        None = 0,
        RigGrant,   // +magnitude levels on the Rig Shop row `id`, free (game_rig_shop.h)
        Item,       // +magnitude copies of item `id`
        Recipe,     // the MERGE HUB recipe that COOKS item `id` (magnitude unused) —
                    // named by the dish, because that is what the board prints and what
                    // the player is actually winning
    };
    Kind kind = Kind::None;
    int magnitude = 0;
    const char* id = nullptr;
};

struct QuoteDef {
    // Stable save identity — the 2-bit state field's index. Never reused, never
    // renumbered; see the banner above.
    int wire;
    const char* text;
    // Always shown once the board is solved, never guessed — the attribution is the
    // reward for finishing, not another row of blanks.
    const char* attribution;
    // An achievement id (content_achievements.h's `ach::` namespace) that must be
    // EARNED before this quote enters the pool, or nullptr for one that is always
    // offered. The second eligibility axis alongside "have you already solved it";
    // both are answered in one place, Game::quoteEligible.
    const char* requiresAch = nullptr;
};

// Capacity for the save's per-quote state, in wire numbers — two bits each, so the
// array costs a quarter of this in bytes and every save carries all of it. Raise it (in
// multiples of 4, with a save-version note) as the table approaches it; the array is
// length-prefixed, so a longer one loads into an older build's shorter view harmlessly
// and a shorter one reads back as "never played".
inline constexpr int kQuoteWireCap = 256;
inline constexpr int kQuoteStateBytes = kQuoteWireCap / 4;

// --- What a board pays -------------------------------------------------------
//
// One answer for every quote, because a first solve is a first solve: the prize is the
// GAME's, not the row's. When a quote wants its own — a marquee one worth a rack slot,
// a throwaway worth half the Bits — QuoteDef grows an optional reward and these two
// resolve it off the row instead of ignoring it. That is the growth point; until a row
// actually wants a different prize, a per-row field would be 300 copies of the same
// literal for whoever is pasting the table in.
inline constexpr int kQuoteWinBits = 256;

// THE PRIZE LADDER — what a first solve hands over, in payout order. Each entry is a
// DISH, naming the MERGE HUB recipe that cooks it (game_internal.h's kMergeRecipes).
//
// A recipe is the one thing in the game Bits cannot buy: no shop lists one at any
// price. That is the deal the board is offering — the pantry drops staples for free all
// day, the hub is on the shelf for anyone who saves up, and the METHOD only ever comes
// off a quote nobody has cracked yet. It also fixes what a Decryptogram used to be
// worth, which was a Bandwidth level a rich player already had and a poor one could
// have bought.
//
// A solve pays the first dish on this list the operator can't already cook, skipping
// any the kitchen isn't ready for — no hub to cook in, or a dish they've never met
// (Game::recipeGrantable). Order is therefore the difficulty curve of the kitchen, not
// a ranking: plain fry-ups first, the dishes that heal mid-combat last.
//
// Run out of recipes and the fallback takes over. Bandwidth is what it falls back TO
// for the reason it used to be the only prize: it is the one Rig Shop row with no
// purchase cap, so the last quote of the pool can never pay out nothing.
const char* const* quotePrizeLadder();
int quotePrizeLadderCount();
QuoteReward quoteFallbackReward();

// A quote already solved has no first-solve prize left, so re-playing one — from the
// VAULT once the whole pool is won, or from the arcade cabinet — pays only Bits. The
// arcade prices its own runs (kArcadePlayBits); this is the VAULT's figure.
inline constexpr int kQuoteReplayBits = 48;

// How many quotes must be solved before the GAMES arcade offers the cabinet at all.
// The arcade is where a solved quote goes to be played again for Bits, so it has
// nothing to show until there is a back catalogue to show it from.
inline constexpr int kQuoteArcadeUnlockWins = 8;

const QuoteDef* quotes();
int quoteCount();
// Row index for `wire`, or -1 — how a saved state finds its quote again after the rows
// have been reordered.
int quoteIndexByWire(int wire);

}  // namespace mal
