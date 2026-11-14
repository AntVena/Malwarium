#include "core/app/game/wifi_screen.h"

#include "core/ui/expl_screen.h"

namespace mal {

void WifiScreen::onButton(WifiHost& host, const ButtonEvent& ev) {
    // B plays out the rolled outcome; A and C do nothing, since the event resolves
    // itself with no engage or flee choice.
    host.restartEventHold();
    if (ev.button == Button::B) host.playOutWifiEvent();
}

void WifiScreen::draw(Framebuffer& fb, const WifiHost& host) const {
    const WifiEvent* ev = host.wifiEvent();
    // The discovery beat's reward, as the thing the pet does to the network glyph.
    // Untouched means the pet declined its own home turf, which the TIRED OF line
    // says.
    //
    // The banner states the same fact in words, so it is derived here from the
    // event's discovery. A fixed "NEW WI-FI NETWORK" over a repeat sighting or an empty
    // queue would contradict the two flavor lines.
    WifiEventView v;
    v.banner = "QUIET AIR";
    switch (ev ? ev->discovery : NetDiscovery::None) {
        case NetDiscovery::New:
            v.absorb = WifiAbsorb::Whole; v.banner = "NEW WI-FI NETWORK"; break;
        case NetDiscovery::Fond:
            v.absorb = WifiAbsorb::Nibble; v.banner = "A KNOWN NETWORK"; break;
        case NetDiscovery::HomeTurf:
            v.absorb = WifiAbsorb::Untouched; v.banner = "HOME TURF"; break;
        case NetDiscovery::None: break;
    }
    v.sectorName = host.walkSectorName();
    v.outcomeLine = ev ? ev->outcomeLine() : "";
    v.discoveryLine = ev ? ev->discoveryLine : "";
    v.pet = host.petSprite();
    v.beat = host.screenBeat();
    v.fxBeat = host.fxBeat();
    drawWifiEvent(fb, v);
}

}  // namespace mal
