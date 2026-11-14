#include "core/app/game/csf_screen.h"

#include "tunables.h"
#include "core/ui/modals.h"

namespace mal {

void CsfScreen::tick() {
    if (beat_ < kCsfHoldBeats) beat_++;
}

bool CsfScreen::revealed() const { return beat_ >= kCsfHoldBeats; }

void CsfScreen::draw(Framebuffer& fb, const CsfHost& host) const {
    drawCSFModal(fb, host.petSprite(), revealed() ? CsfPhase::Held : CsfPhase::Crashing,
                 host.screenBeat());
}

}  // namespace mal
