#include "core/app/game/explore_control_screen.h"

#include "core/ui/expl_screen.h"
#include "core/ui/layout.h"

namespace mal {

void ExploreControlScreen::onButton(ExploreControlHost& host, const ButtonEvent& ev) {
    // The chord only opens this: driving the overlay with it would be awkward, since A
    // registers on its own first. A cycles, B does the focused thing, C backs out.
    if (ev.button == Button::A) { row_ = stepRow(row_, kExploreControlRows, +1); return; }
    if (ev.button == Button::C) { host.leaveToIdle(); return; }
    if (ev.button != Button::B) return;
    switch (static_cast<ExploreControlRow>(row_)) {
        case ExploreControlRow::Ping: host.pingWalk(); break;
        // Inert with no key to spend: the row stays, and says so.
        case ExploreControlRow::Warp: if (host.hasWarpKey()) host.openWarpPicker(); break;
        // The two modes leave the overlay open, so the ON/OFF just changed is what the
        // player is looking at.
        case ExploreControlRow::AutoProgress: host.toggleAutoProgress(); break;
        case ExploreControlRow::Sound: host.setExploreSound(!host.exploreSound()); break;
        case ExploreControlRow::Stop: host.stopWalk(); break;
    }
}

void ExploreControlScreen::draw(Framebuffer& fb, const ExploreControlHost& host) const {
    host.drawHabitat(fb, -1);
    ExploreControlView v;
    v.cursor = row_;
    v.hasWarpKey = host.hasWarpKey();
    v.autoProgress = host.autoProgress();
    v.sound = !soundModeAllows(host.soundMode(), SoundTier::Event) ? ExploreSound::OffInCfg
              : host.exploreSound()                                ? ExploreSound::On
                                                                   : ExploreSound::Muted;
    v.xpEfficiencyPct = host.exploreXpEfficiencyPct();
    drawExploreControl(fb, v);
}

}  // namespace mal
