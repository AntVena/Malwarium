#include "core/app/game.h"

#include "core/content/content_quotes.h"
#include "core/ui/cryptogram_screen.h"

// game_cryptogram.cpp — THE DECRYPTOGRAM, the quote board.
//
// A Decryptogram is a found item with one use: cash it in at the Hacker VAULT and a
// quote comes up struck out. Open every letter without a single wrong placement and it
// pays Bits plus one account unlock; place one letter wrong and the ticket is gone and
// the quote keeps its secret. There is no partial credit and no walking away — C does
// not leave a board any more than it leaves a hatch.
//
// What makes losing survivable is that the QUOTE remembers. Every quote starts Hard;
// each loss ratchets it down a rung (Medium opens one more letter at the start, Easy
// three), and it stops at Easy. So a quote that has beaten you twice is a quote you
// will eventually crack, and the pool drains.
//
// Three files, and the split is the usual one: the rules are core/model/cryptogram.h
// (no Game, no framebuffer), the pool is core/content/content_quotes.h (no rules), and
// this file is the lifecycle, the eligibility roll and the payout. The board is
// core/ui/cryptogram_screen.cpp.
//
// Entered from Game::cashCryptogramTicket (the VAULT), and from the GAMES arcade's own
// cabinet once kQuoteArcadeUnlockWins quotes are solved. Leaves to the VAULT, or to the
// arcade's till.

namespace mal {

// --- Per-quote standing (save v48) -----------------------------------------
//
// Two bits per QuoteDef::wire, four to a byte. Every read and write of that packing is
// in this pair, so nothing else has to know the layout.

CryptogramTier Game::quoteTier(int i) const {
    if (i < 0 || i >= quoteCount()) return CryptogramTier::Hard;
    const int wire = quotes()[i].wire;
    if (wire < 0 || wire >= kQuoteWireCap) return CryptogramTier::Hard;
    return static_cast<CryptogramTier>((quoteStates_[wire / 4] >> (2 * (wire % 4))) & 0x3);
}

void Game::setQuoteTier(int i, CryptogramTier t) {
    if (i < 0 || i >= quoteCount()) return;
    const int wire = quotes()[i].wire;
    if (wire < 0 || wire >= kQuoteWireCap) return;
    const int shift = 2 * (wire % 4);
    uint8_t& cell = quoteStates_[wire / 4];
    cell = static_cast<uint8_t>((cell & ~(0x3u << shift)) |
                                (static_cast<unsigned>(t) << shift));
}

int Game::quotesSolved() const {
    int n = 0;
    for (int i = 0; i < quoteCount(); ++i)
        if (quoteTier(i) == CryptogramTier::Solved) ++n;
    return n;
}

// --- Which quote comes up ---------------------------------------------------

bool Game::quoteEligible(int i, QuotePick pick) const {
    if (i < 0 || i >= quoteCount()) return false;
    const QuoteDef& q = quotes()[i];
    // The achievement gate applies to every pick mode: a quote you have not earned the
    // right to see is not one the arcade may replay either.
    if (q.requiresAch && !hasAchievement(q.requiresAch)) return false;
    const bool solved = quoteTier(i) == CryptogramTier::Solved;
    switch (pick) {
        case QuotePick::Unsolved:   return !solved;
        case QuotePick::SolvedOnly: return solved;
        case QuotePick::Any:        break;
    }
    return true;
}

bool Game::quotesLeftToWin() const {
    for (int i = 0; i < quoteCount(); ++i)
        if (quoteEligible(i, QuotePick::Unsolved)) return true;
    return false;
}

int Game::pickQuote(QuotePick pick) {
    // Two-pass draw: count what qualifies, then take the nth. One LCG step whatever the
    // pool size, so the roll costs the same on a device with four quotes and one with
    // four hundred.
    int n = 0;
    for (int i = 0; i < quoteCount(); ++i)
        if (quoteEligible(i, pick)) ++n;
    if (n == 0) return -1;
    rng_ = rng_ * 1664525u + 1013904223u;
    int nth = static_cast<int>((rng_ >> 16) % static_cast<uint32_t>(n));
    for (int i = 0; i < quoteCount(); ++i) {
        if (!quoteEligible(i, pick)) continue;
        if (nth-- == 0) return i;
    }
    return -1;
}

// --- Lifecycle --------------------------------------------------------------

void Game::startCryptogram(int quote, int extraReveals) {
    if (quote < 0 || quote >= quoteCount()) return;
    cryptogramQuote_ = quote;
    // The stake is decided at the START, not at the payout, so the board can state what
    // is riding on the next placement while the player is still deciding to make it.
    // A cabinet run has no stake of its own — the arcade's till prices it on the way out
    // — and a quote already solved has no first-solve prize left to win twice.
    const bool firstSolve = !arcadeRun_ && quoteTier(quote) != CryptogramTier::Solved;
    cryptogramPrizeBits_ = arcadeRun_ ? 0 : (firstSolve ? kQuoteWinBits : kQuoteReplayBits);
    cryptogramPrize_ = firstSolve ? quoteWinReward() : QuoteReward{};
    // A quote the content gate has passed always fits, so the return is ignored here
    // and asserted there instead — a board that refused to load at runtime would strand
    // a spent ticket with nothing to show for it.
    cryptogram_.reset(quotes()[quote].text, extraReveals);
    nav_ = Nav::Cryptogram;
    dirty_ = true;
}

void Game::onCryptogram(const ButtonEvent& ev) {
    if (!cryptogram_.running()) {
        // Parked on the verdict, which the settle has already banked. B leaves; C is
        // DISABLED, like every other board — the ticket is spent either way.
        if (ev.button == Button::B) finishCryptogram();
        return;
    }
    // A cycles (the pool, or the open cells once a letter is in hand), B takes the
    // letter and then places it, C hands the letter back. C is not "cancel" here, which
    // is the deviation the hint band spells out.
    if (ev.button == Button::A) cryptogram_.cycle();
    else if (ev.button == Button::B) cryptogram_.accept();
    else if (ev.button == Button::C) cryptogram_.stepBack();
    if (!cryptogram_.running()) settleCryptogram();
    dirty_ = true;
}

void Game::settleCryptogram() {
    // An arcade run is played off a solved quote for Bits; it has no standing to move
    // and no first-solve prize to pay. Its till is finishArcadeRun, on the way out.
    if (arcadeRun_ || cryptogramQuote_ < 0) return;

    const CryptogramTier before = quoteTier(cryptogramQuote_);
    if (cryptogram_.solved()) {
        // Both halves of the stake were fixed at the start; this only banks them. A
        // replay (the whole pool already won) carries Bits and a None unlock, so the
        // same two lines pay it.
        setQuoteTier(cryptogramQuote_, CryptogramTier::Solved);
        applyQuoteReward();
    } else if (before == CryptogramTier::Hard) {
        setQuoteTier(cryptogramQuote_, CryptogramTier::Medium);
    } else if (before == CryptogramTier::Medium) {
        setQuoteTier(cryptogramQuote_, CryptogramTier::Easy);
    }
    // Easy is the floor and Solved never moves — both fall through untouched.
    markSaveDirty();
}

void Game::applyQuoteReward() {
    bits_ += cryptogramPrizeBits_;
    switch (cryptogramPrize_.kind) {
        case QuoteReward::Kind::RigGrant: {
            // Free, but otherwise exactly a purchase — grantRigLevel is the same call
            // buyRigUpgrade makes once it has taken the Bits, so a granted Bandwidth
            // level tops the live pool up the way a bought one does.
            const int row = rigRowIndexById(cryptogramPrize_.id);
            if (row >= 0) grantRigLevel(row, cryptogramPrize_.magnitude);
            break;
        }
        case QuoteReward::Kind::Item:
            if (cryptogramPrize_.id && registry_.item(cryptogramPrize_.id))
                inventory_.add(cryptogramPrize_.id, cryptogramPrize_.magnitude);
            break;
        case QuoteReward::Kind::None:
            break;
    }
    log_.push(LogEventType::ItemGained, "CRACKED A QUOTE");
}

void Game::finishCryptogram() {
    if (arcadeRun_) {
        finishArcadeRun(cryptogram_.solved(), cryptogram_.score(), cryptogram_.maxScore());
        return;
    }
    // Back to the VAULT the ticket was cashed at — the one screen that can say whether
    // there is another Decryptogram to spend.
    nav_ = Nav::Submenu;
    dirty_ = true;
}

// --- Render -----------------------------------------------------------------

// The unlock half of the stake, as the words the board prints. Resolved HERE rather
// than on the screen because it is a lookup across two content tables (the Rig Shop's
// rows, the item registry) and core/ui has no business knowing either.
const char* Game::cryptogramPrizeLabel() const {
    switch (cryptogramPrize_.kind) {
        case QuoteReward::Kind::RigGrant: {
            const int row = rigRowIndexById(cryptogramPrize_.id);
            return row >= 0 ? kRigUpgrades[row].displayName : nullptr;
        }
        case QuoteReward::Kind::Item: {
            const ItemDef* d = cryptogramPrize_.id ? registry_.item(cryptogramPrize_.id)
                                                   : nullptr;
            return d ? d->displayName : nullptr;
        }
        case QuoteReward::Kind::None:
            break;
    }
    return nullptr;
}

void Game::drawCryptogram(Framebuffer& fb) const {
    const QuoteDef* q = (cryptogramQuote_ >= 0 && cryptogramQuote_ < quoteCount())
                            ? &quotes()[cryptogramQuote_] : nullptr;
    drawCryptogramBoard(fb, cryptogram_, q ? q->attribution : "",
                        quoteTier(cryptogramQuote_), cryptogramPrizeBits_,
                        cryptogramPrizeLabel(), arcadeRun_, beat_);
}

}  // namespace mal
