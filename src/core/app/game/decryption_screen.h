// decryption_screen.h: the DISK DECRYPTION board (Nav::Decryption), the Ransomware
// line's hatch game and a GAMES cabinet, over the DiskDecryption run
// (core/model/disk_decryption.h).
//
// The run holds the rules. The screen holds how much the board discloses (the arcade's
// easy cabinet spends the damage per cell) and turns A into cycling the focused slot's
// colour, B into settling it and C into stepping back. B on the parked verdict banks
// and leaves. What a run buys is the game's (DecryptionHost::finishDecryption).
#pragma once

#include "core/app/game/board_host.h"
#include "core/model/disk_decryption.h"   // DecryptHints

namespace mal {

class DecryptionHost : public BoardHost {
public:
    virtual DiskDecryption& liveDecryption() = 0;
    virtual const DiskDecryption& decryption() const = 0;
    virtual void finishDecryption() = 0;

protected:
    ~DecryptionHost() = default;
};

class DecryptionScreen {
public:
    void open(DecryptHints hints) { hints_ = hints; }
    void onButton(DecryptionHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const DecryptionHost& host) const;

private:
    DecryptHints hints_ = DecryptHints::Anonymous;
};

}  // namespace mal
