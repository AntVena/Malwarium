#include "core/app/game/decryption_screen.h"

#include "core/ui/decryption_screen.h"

namespace mal {

void DecryptionScreen::onButton(DecryptionHost& host, const ButtonEvent& ev) {
    if (host.onGameBriefInput(ev)) return;
    DiskDecryption& run = host.liveDecryption();
    if (!run.running()) {
        // Parked on the verdict. B banks and leaves. C is disabled, as on every
        // hatch screen, because there is no pet to go back to.
        if (ev.button == Button::B) host.finishDecryption();
        return;
    }
    // A cycles the focused slot's colour, B settles it (and the third settle plays the
    // row), C steps back and re-opens the slot it lands on. C is not "cancel" here,
    // which is the deviation the hint band spells out.
    if (ev.button == Button::A) run.cycleColour();
    else if (ev.button == Button::B) {
        const int rows = run.played();
        run.lockIn();
        if (run.cracked()) host.playSound(Sound::Clear);
        else if (!run.running()) host.playSound(Sound::Crash);
        else if (run.played() > rows) host.playSound(Sound::Probe);
    }
    else if (ev.button == Button::C) run.stepBack();
}

void DecryptionScreen::draw(Framebuffer& fb, const DecryptionHost& host) const {
    if (host.gameBriefOpen()) { host.drawGameBrief(fb); return; }
    drawDiskDecryption(fb, host.decryption(), hints_,
                       host.inArcadeRun() ? DecryptStakes::Arcade : DecryptStakes::Hatch,
                       host.screenBeat());
}

}  // namespace mal
