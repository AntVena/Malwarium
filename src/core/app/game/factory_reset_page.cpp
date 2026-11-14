#include "core/app/game/factory_reset_page.h"

#include "tunables.h"
#include "core/ui/cfg_screen.h"

namespace mal {

// A cycles the scope, B arms the hold that commits it, C is the safe abort.
CfgMove FactoryResetPage::onButton(FactoryResetHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        scope_ = scope_ == FactoryScope::ResetPet ? FactoryScope::WipeEverything
                                                  : FactoryScope::ResetPet;
    } else if (ev.button == Button::B) {
        host.armHoldB();
    } else if (ev.button == Button::C) {
        return CfgMove::leave();
    }
    return CfgMove::stay();
}

bool FactoryResetPage::holdB(FactoryResetHost& host, uint32_t heldMs) {
    if (heldMs < kFactoryCommitMs) return false;
    host.executeFactoryReset(scope_);
    return true;
}

void FactoryResetPage::draw(Framebuffer& fb, const FactoryResetHost& host) const {
    const int32_t held = host.heldBMs();
    const float hf = held >= 0 ? static_cast<float>(held) / kFactoryCommitMs : 0.0f;
    drawFactoryReset(fb, static_cast<int>(scope_), hf);
}

}  // namespace mal
