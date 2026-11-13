// encounter_screen.h: the wild-encounter intro (Nav::Encounter), as a feature that owns
// its own state.
//
// The screen between a wild's roll and what the player does about it: Fight, Flee, or
// (with a bypass item held) Sinkhole. It holds the wild it presents and its cursor. The
// roll, the escape roll and the bypass's XP are the game's, reached through
// EncounterHost, and a fight it opens is handed its own copy of the wild.
//
// This header names no core/ui type, so game.h can hold an EncounterScreen by value.
#pragma once

#include "core/device/platform.h"
#include "core/model/combat.h"   // CombatEnemy

namespace mal {

class ContentRegistry;
class Framebuffer;
struct ItemDef;

// What the encounter intro needs from the game. Game implements it; a test can too.
class EncounterHost {
public:
    virtual const ContentRegistry& content() const = 0;
    virtual int screenBeat() const = 0;
    // The held item that skips a wild for XP (any row with a preEncounterXp), or null.
    // The third row shows only while one is held.
    virtual const ItemDef* heldEncounterBypass() const = 0;

    virtual void fightWild(const CombatEnemy& wild) = 0;
    // The escape roll: away to the walk, or caught and fought with the wild first.
    virtual void resolveFlee(const CombatEnemy& wild) = 0;
    virtual void resolveSinkhole(const ItemDef& bypass) = 0;

protected:
    ~EncounterHost() = default;
};

class EncounterScreen {
public:
    // Every intro opens on Fight.
    void open(const CombatEnemy& wild) {
        wild_ = wild;
        choice_ = 0;
    }
    void onButton(EncounterHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const EncounterHost& host) const;

    const CombatEnemy& wild() const { return wild_; }
    int choice() const { return choice_; }   // 0 Fight, 1 Flee, 2 Sinkhole

private:
    CombatEnemy wild_{};
    int choice_ = 0;
};

}  // namespace mal
