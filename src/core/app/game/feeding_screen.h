// feeding_screen.h: the feeding modal (Nav::ModalFeeding), the thin screen over a
// Feeding run (core/model/feeding.h).
//
// The run holds the bite: the food, the vitals before it and the clock that ends it.
// The screen holds nothing. It draws the pet eating and the gauges settling, and B or C
// ends the feeding early, which the game carries out because where it hands back to
// (the item's page, the list, or the Lockout it resolved) is the game's.
#pragma once

#include "core/device/platform.h"

namespace mal {

class Framebuffer;
class PetModel;
struct Feeding;
struct SpriteData;

// What the modal needs from the game. Game implements it; a test can too.
class FeedingHost {
public:
    virtual const Feeding* feeding() const = 0;   // null with no bite open
    virtual const PetModel& model() const = 0;    // the vitals the gauges settle on
    virtual const SpriteData* petSprite() const = 0;
    virtual const SpriteData* itemSprite(const char* itemId) const = 0;
    virtual int fxBeat() const = 0;               // the bite's dissolve clock
    virtual void endFeeding() = 0;

protected:
    ~FeedingHost() = default;
};

class FeedingScreen {
public:
    void onButton(FeedingHost& host, const ButtonEvent& ev) {
        if (ev.button == Button::B || ev.button == Button::C) host.endFeeding();
    }
    void draw(Framebuffer& fb, const FeedingHost& host) const;
};

}  // namespace mal
