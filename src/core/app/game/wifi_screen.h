// wifi_screen.h: the Wi-Fi network event's screen (Nav::Wifi), the thin screen over a
// WifiEvent.
//
// The run (core/model/wifi_event.h) holds the event's result. The screen holds nothing
// of its own: it draws the discovery beat and the rolled outcome, and turns B into
// playing the outcome out, which the game carries out because it pays loot, starts a
// fight or arms an ally buff and then re-enters the walk.
#pragma once

#include "core/device/platform.h"
#include "core/model/wifi_event.h"

namespace mal {

class Framebuffer;
struct SpriteData;

// What the screen needs from the game. Game implements it; a test can too.
class WifiHost {
public:
    virtual const WifiEvent* wifiEvent() const = 0;   // null with no event open
    virtual const char* walkSectorName() const = 0;
    virtual const SpriteData* petSprite() const = 0;
    virtual int screenBeat() const = 0;
    virtual int fxBeat() const = 0;                   // the absorb's dissolve clock

    // The event resolves itself after a hands-off hold; a press starts that hold again
    // so a watching player is not rushed.
    virtual void restartEventHold() = 0;
    virtual void playOutWifiEvent() = 0;

protected:
    ~WifiHost() = default;
};

class WifiScreen {
public:
    void onButton(WifiHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const WifiHost& host) const;
};

}  // namespace mal
