#include "core/app/game/cryptogram_screen.h"

#include "core/content/content_quotes.h"
#include "core/model/cryptogram_run.h"
#include "core/ui/cryptogram_screen.h"

namespace mal {

void CryptogramScreen::onButton(CryptogramHost& host, const ButtonEvent& ev) {
    if (host.onGameBriefInput(ev)) return;
    CryptogramRun* run = host.liveCryptogram();
    if (!run) return;
    Cryptogram& board = run->board;
    if (!board.running()) {
        // Parked on the verdict, which the settle has already banked. B leaves. C is
        // disabled, as on every board, because the ticket is spent either way.
        if (ev.button == Button::B) host.finishCryptogram();
        return;
    }
    // A and C are the live cursor's two DIRECTIONS: forward and back through the pool,
    // or through the closed cells once a letter is in hand. Bidirectional because a
    // board opens with thirty-odd gaps, and overshooting by one on a forward-only
    // cursor costs a full lap of the quote.
    //
    // That uses both buttons a cancel would need, so the A+C chord hands the letter
    // back. C is therefore not "cancel" here and there is no way off a board at all,
    // which is the deal every ticket is bought on.
    //
    // dropLetter() is inert with nothing held (PickLetter), which is exactly when the
    // chord is free to mean something else instead: the RULES page. A player mid-place
    // (PickCell) still gets the drop, never the page, so the chord can never strand a
    // held letter behind an overlay.
    if (ev.chordAC) {
        if (board.stage() == Cryptogram::Stage::PickCell) board.dropLetter();
        else host.openGameBrief();
        return;
    }
    if (ev.button == Button::A) {
        board.cycle();
        host.armHoldRepeat(Button::A);   // ...and holding it repeats (tick, kCryptogramRepeatMs)
    } else if (ev.button == Button::C) {
        board.cycleBack();
        host.armHoldRepeat(Button::C);
    } else if (ev.button == Button::B) {
        const int opened = board.score();
        board.accept();
        if (board.solved()) host.playSound(Sound::Clear);
        else if (!board.running()) host.playSound(Sound::Crash);
        else if (board.score() > opened) host.playSound(Sound::Point);
    }
    if (!board.running()) host.settleCryptogram();
}

void CryptogramScreen::draw(Framebuffer& fb, const CryptogramHost& host) const {
    if (host.gameBriefOpen()) { host.drawGameBrief(fb); return; }
    const CryptogramRun* open = host.cryptogramRun();
    if (!open) return;
    const CryptogramRun& run = *open;
    const QuoteDef* q = (run.quote >= 0 && run.quote < quoteCount()) ? &quotes()[run.quote]
                                                                     : nullptr;
    CryptogramView v;
    v.attribution = q ? q->attribution : "";
    v.tier = host.quoteTier(run.quote);
    v.prizeBits = run.prizeBits;
    v.prizeLabel = host.cryptogramPrizeLabel();
    v.arcade = host.inArcadeRun();
    v.beat = host.screenBeat();
    drawCryptogramBoard(fb, run.board, v);
}

}  // namespace mal
