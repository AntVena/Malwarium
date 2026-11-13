#include "core/app/game/encounter_screen.h"

#include "core/content/registry.h"
#include "core/ui/expl_screen.h"

namespace mal {

void EncounterScreen::onButton(EncounterHost& host, const ButtonEvent& ev) {
    const ItemDef* bypass = host.heldEncounterBypass();
    if (ev.button == Button::A) {
        choice_ = (choice_ + 1) % (bypass ? 3 : 2);
    } else if (ev.button == Button::B) {
        if (choice_ == 0) host.fightWild(wild_);
        else if (choice_ == 1) host.resolveFlee(wild_);
        else if (bypass) host.resolveSinkhole(*bypass);
    } else if (ev.button == Button::C) {
        host.resolveFlee(wild_);                    // flee shortcut
    }
}

void EncounterScreen::draw(Framebuffer& fb, const EncounterHost& host) const {
    EncounterIntroView v;
    v.enemyName = wild_.name;
    v.enemySprite = host.content().sprite(wild_.spriteName);
    v.level = wild_.level;
    v.diffPips = wild_.diffPips;
    v.sinkholeAvailable = host.heldEncounterBypass() != nullptr;
    v.choice = choice_;
    v.beat = host.screenBeat();
    drawEncounterIntro(fb, v);
}

}  // namespace mal
