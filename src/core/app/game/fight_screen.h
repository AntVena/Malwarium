// fight_screen.h: the Combat screen (Nav::Combat), as a feature that owns its own state.
//
// The screen over a Fight run (core/model/fight.h): its input, its draw, and the stage's
// own motion. It holds the stats panel's page, the sprite anim clock, and the FX_CAMO
// disguise the pet is wearing. All of that lives while the screen is up, which spans every
// round of a boss gauntlet, and the motion clears when the screen closes. The fight itself
// is the run's, which the screen reads through FightScreenHost and drives with its verbs.
//
// This header names no core/ui type, so game.h can hold a FightScreen by value: the
// combat renderer's vocabulary stays in fight_screen.cpp.
#pragma once

#include <cstdint>

#include "core/audio/sound.h"
#include "core/device/platform.h"
#include "core/model/fight.h"
#include "core/render/camo.h"   // CamoTarget
#include "core/render/scene_id.h"
#include "core/render/swarm.h"

namespace mal {

class ContentRegistry;
class Framebuffer;
struct SpriteData;

// What the Combat screen needs from the game. Game implements it; a test can too.
class FightScreenHost {
public:
    virtual const Fight& fight() const = 0;
    // The run the screen drives: its Exploit picker, its flee, its hit clock.
    virtual Fight& liveFight() = 0;
    virtual const ContentRegistry& content() const = 0;
    // The local pet's own sheet. A duel seats both fighters off the fight instead.
    virtual const SpriteData* petSprite() const = 0;
    // A linked duel is resolving, and the guest's pet sits in Combat's enemy_ seat.
    virtual bool pvpFighting() const = 0;
    virtual bool combatLocalIsEnemySide() const = 0;
    // Whether the beaten rival dissolves, and which of its moves it could teach.
    virtual bool combatOutroEligible() const = 0;
    virtual uint32_t rivalTeachableMoveMask() const = 0;
    virtual int fxBeat() const = 0;     // the dissolve clock the outro reads
    virtual int screenBeat() const = 0; // the shared heartbeat (the gauge's pulse)
    virtual SceneId stageScene() const = 0;
    // The art a camo target names (FX_CAMO), or null for nothing to wear.
    virtual const SpriteData* camoSpriteForTarget(const CamoTarget& t, const char* rivalSprite,
                                                  Stage wearer) const = 0;
    // An area guardian's body, drawn in the rival's seat, and its step on the stage clock.
    virtual SwarmView guardianSwarm() const = 0;
    virtual void stepGuardianFlock() = 0;

    virtual void openExploitPicker() = 0;            // A+C: the moves, items and crew row
    virtual void consumeCombatItem(const char* id) = 0;
    virtual void advanceCombatTurn() = 0;
    virtual void playCombatTurnSound() = 0;
    virtual void playSound(Sound s) = 0;
    virtual void finishCombat() = 0;                 // B/C on the verdict

protected:
    ~FightScreenHost() = default;
};

class FightScreen {
public:
    void onButton(FightScreenHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const FightScreenHost& host) const;
    // One stage tick at `nowMs`: steps the sprite motion, the hit clock, the guardian's
    // swarm and the camo every kCombatAnimMs. True when it stepped.
    bool tick(FightScreenHost& host, uint32_t nowMs);
    // The screen is not up: prime the clock to `nowMs`, so arriving never bursts to
    // catch up, and drop the disguise, so every fight opens bare.
    void closed(uint32_t nowMs);
    // Every fight opens with the stats panel shut.
    void shutPanel() { statsPage_ = 0; }

    int statsPage() const { return statsPage_; }

private:
    void stepCamo(const FightScreenHost& host);

    // Which page of the mid-combat panel is showing: 0 closed, else 1..kCombatStatPages
    // (core/ui/combat_screen.h). B CYCLES it: the panel grew a second page when opponents
    // started arriving with real loadouts, and a second key to reach it would have had
    // to come out of the three combat already spends.
    int statsPage_ = 0;
    // Sprite motion runs on its own faster clock (kCombatAnimMs) instead of the shared
    // heartbeat, so a feeding-frenzy streak's accelerated turns still show real motion.
    int animBeat_ = 0;
    uint32_t lastAnimMs_ = 0;
    // Whose colours the pet is wearing (FX_CAMO): the ART being sampled plus a LEVEL,
    // neither of which is a beat. The tick reads the pet's live cast (camoTarget),
    // resolves it to a sprite (camoSpriteForTarget) and eases the level toward it
    // (camoAdvance), so the colours hold while the pet holds the move.
    //
    // The SPRITE is what is held rather than the target that named it, because the
    // sprite decides whether the palette is actually changing: a cast onto a fighter
    // whose line the pet already wears must sit still rather than dissolve into itself.
    // camoLeaving_ is the art a swap is coming off, held until the dissolve finishes.
    const SpriteData* camoWorn_ = nullptr;
    const SpriteData* camoLeaving_ = nullptr;
    uint8_t camoLevel_ = 0;
};

}  // namespace mal
