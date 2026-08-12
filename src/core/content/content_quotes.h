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
//
// Bandwidth is the deliberate first (and only) unlock kind in play: it is the one Rig
// Shop row with no purchase cap, so a prize can never be dead on arrival for a player
// who already owns everything, and it is the cheapest thing they are likely to want a
// lot of.
inline constexpr int kQuoteWinBits = 256;
QuoteReward quoteWinReward();

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
