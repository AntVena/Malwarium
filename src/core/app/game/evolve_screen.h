// evolve_screen.h: the evolution modal (Nav::ModalEvolve).
//
// A cinematic toward the successor: hold, flash, then the reveal, which parks until B.
// The screen holds the successor and the cinematic's clock, since both die with it, and
// owns the draw. B on the reveal hands the successor to the game, which commits the
// swap (EvolveHost), because an evolution changes the pet record. C does nothing: the
// evolution has already happened.
#pragma once

#include "core/device/platform.h"
#include "core/model/pet_model.h"   // CareBranch

namespace mal {

class Framebuffer;
struct CreatureDef;
struct SpriteData;

// What the modal needs from the game. Game implements it; a test can too.
class EvolveHost {
public:
    virtual const CreatureDef* pet() const = 0;   // the form evolving, shown until the swap
    virtual const SpriteData* creatureSprite(const CreatureDef& c) const = 0;
    virtual int careMistakes() const = 0;
    virtual CareBranch effectiveCareBranch() const = 0;   // the path the errors put it on
    virtual int screenBeat() const = 0;
    virtual void completeEvolution(const CreatureDef* to) = 0;

protected:
    ~EvolveHost() = default;
};

class EvolveScreen {
public:
    void open(const CreatureDef* to) { to_ = to; beat_ = 0; }
    // A heartbeat: hold -> flash -> reveal, then it parks at the reveal.
    void tick();
    bool revealed() const;
    void onButton(EvolveHost& host, const ButtonEvent& ev) {
        if (ev.button == Button::B && revealed()) host.completeEvolution(to_);
    }
    void draw(Framebuffer& fb, const EvolveHost& host) const;

private:
    const CreatureDef* to_ = nullptr;   // the successor being revealed
    int beat_ = 0;
};

}  // namespace mal
