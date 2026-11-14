#include "core/app/game/fragger_screen.h"

#include "core/model/fragger.h"
#include "core/ui/maint_screen.h"
#include "tunables.h"

namespace mal {

void FraggerScreen::onButton(FraggerHost& host, const ButtonEvent& ev) {
    if (host.onGameBriefInput(ev)) return;
    Fragger& f = host.liveFragger();
    if (!f.running()) {                  // parked on the result: any press moves on
        host.finishFragger();
        return;
    }
    // A and C move the head (the outer buttons, left to left), departing from the
    // standard contract as the Isolation Protocol does. B fires. There is no stop: the
    // run ends when a fragment lands, and everything shot before that is already
    // banked, so a quit button would only be a slower way to the same toast.
    if (ev.button == Button::A) {
        f.moveLeft();
    } else if (ev.button == Button::C) {
        f.moveRight();
    } else if (ev.button == Button::B) {
        int row = -1;
        const Fragger::Shot shot = f.fire(&row);
        shotLane_ = f.lane();
        shotRow_ = row;
        if (shot == Fragger::Shot::Hit) host.playSound(Sound::Point);
        else if (shot == Fragger::Shot::Blocked) host.playSound(Sound::Miss);
    }
}

void FraggerScreen::draw(Framebuffer& fb, const FraggerHost& host) const {
    if (host.gameBriefOpen()) { host.drawGameBrief(fb); return; }
    const Fragger& f = host.fragger();
    PlayOutcome sofar;
    sofar.units = f.hits();
    sofar.bitsUnits = f.caught();
    sofar.score = f.hits() + f.caught();
    sofar.scoreMax = kFraggerWinHits;
    drawFraggerBoard(fb, f, host.playHud(sofar), shotLane_, shotRow_);
}

}  // namespace mal
