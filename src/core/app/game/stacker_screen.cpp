#include "core/app/game/stacker_screen.h"

#include "core/model/stacker.h"
#include "core/ui/maint_screen.h"
#include "tunables.h"

namespace mal {

void StackerScreen::onButton(StackerHost& host, const ButtonEvent& ev) {
    if (host.onGameBriefInput(ev)) return;
    Stacker& s = host.liveStacker();
    if (!s.running()) {                  // parked on the result: any press moves on
        host.finishStacker();
        return;
    }
    if (ev.button == Button::B) {
        const int hand = s.width();
        const int threats = s.threatCount();
        s.drop();
        // A shrunk hand is the louder news; a clean lock that touched the ground still
        // has to say it called an intruder.
        if (s.width() < hand) host.playSound(Sound::Miss);
        else if (s.threatCount() > threats) host.playSound(Sound::Probe);
        else host.playSound(Sound::Point);
    } else if (ev.button == Button::C) {
        // Stop early and bank the board. A run that ends mid-air keeps what it locked, so
        // quitting keeps it too and C never throws away earned blocks.
        host.finishStacker();
    }
}

void StackerScreen::draw(Framebuffer& fb, const StackerHost& host) const {
    if (host.gameBriefOpen()) { host.drawGameBrief(fb); return; }
    const Stacker& s = host.stacker();
    PlayOutcome sofar;
    sofar.units = s.blocks();
    sofar.score = s.score();
    sofar.scoreMax = kStackerMaxScore;
    drawStackerBoard(fb, s, host.playHud(sofar));
}

}  // namespace mal
