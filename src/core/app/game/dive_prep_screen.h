// dive_prep_screen.h: DIVE PREP (Nav::DivePrep), the screen in front of a DeepWeb Dive.
//
// Four fixed rows, so a fuller bag adds nothing to press past: DIVE NOW, the BELL and
// DEVICE slots of the standing loadout, and AUTO-REDIVE. B on a slot steps its pick
// (nothing, the best held, then each held item); B on AUTO-REDIVE flips it; B on DIVE NOW
// arms the loadout and dives; C backs out with nothing spent. The loadout is the
// Operator's (Operator::dive) and stands for every later dive. The screen holds only its
// cursor and where it opened from.
#pragma once

#include "core/app/nav.h"
#include "core/app/operator.h"   // DiveLoadout, DiveSlot
#include "core/device/platform.h"

namespace mal {

class Framebuffer;
struct ItemDef;

// What DIVE PREP needs from the game. Game implements it; a test can too.
class DivePrepHost {
public:
    virtual const DiveLoadout& diveLoadout() const = 0;
    // The item `s` would arm if the dive started now: its pick resolved against the bag.
    virtual const ItemDef* divePickItem(DiveSlot s) const = 0;
    virtual int heldCount(const char* itemId) const = 0;
    virtual void cycleDivePick(DiveSlot s) = 0;
    virtual void toggleAutoRedive() = 0;
    virtual void diveWithLoadout() = 0;
    virtual void leaveDivePrep(Nav backTo) = 0;

protected:
    ~DivePrepHost() = default;
};

// The rows, in screen order.
enum class DivePrepRow : uint8_t { DiveNow, Bell, Device, AutoRedive };
inline constexpr int kDivePrepRows = 4;

class DivePrepScreen {
public:
    // Every opening lands on DIVE NOW, so the common case is one press.
    void open(Nav backTo) { row_ = 0; backTo_ = backTo; }
    void stepList(const DivePrepHost& host, int dir);
    void onButton(DivePrepHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const DivePrepHost& host) const;

private:
    int row_ = 0;
    Nav backTo_ = Nav::Idle;
};

}  // namespace mal
