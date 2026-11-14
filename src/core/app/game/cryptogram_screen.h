// cryptogram_screen.h: THE DECRYPTOGRAM board (Nav::Cryptogram), over a CryptogramRun
// (core/model/cryptogram_run.h).
//
// The run holds the board and its stake. The screen holds nothing of its own: A and C
// are the cursor's two directions (and repeat while held), B accepts, and the A+C chord
// hands a held letter back or, with nothing held, opens the RULES page. The verdict is
// banked the moment the board stops running, and B on it leaves; both are the game's
// (CryptogramHost), since they move the quote's standing and pay its prize.
#pragma once

#include "core/app/game/board_host.h"
#include "core/model/cryptogram.h"   // CryptogramTier

namespace mal {

struct CryptogramRun;

class CryptogramHost : public BoardHost {
public:
    virtual CryptogramRun* liveCryptogram() = 0;   // null with no board open
    virtual const CryptogramRun* cryptogramRun() const = 0;
    virtual CryptogramTier quoteTier(int quote) const = 0;
    virtual const char* cryptogramPrizeLabel() const = 0;
    virtual void openGameBrief() = 0;
    // Start the hold that repeats A or C's step on the tick.
    virtual void armHoldRepeat(Button b) = 0;
    virtual void settleCryptogram() = 0;   // bank the verdict
    virtual void finishCryptogram() = 0;   // leave the board

protected:
    ~CryptogramHost() = default;
};

class CryptogramScreen {
public:
    void onButton(CryptogramHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const CryptogramHost& host) const;
};

}  // namespace mal
