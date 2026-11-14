#include "core/app/game/fight_screen.h"

#include "core/content/registry.h"
#include "core/render/camo.h"
#include "core/render/canvas.h"   // kCombatAnimMs: the stage clock
#include "core/render/sprite.h"
#include "core/model/shiboleet_meeting.h"
#include "core/ui/combat_screen.h"
#include "core/ui/swarm_view.h"

namespace mal {

// --- Input -------------------------------------------------------------------

void FightScreen::onButton(FightScreenHost& host, const ButtonEvent& ev) {
    Combat& combat = host.liveFight().combat;
    // Once resolved, the result beat waits for B/C to return to the caller.
    if (combat.outcome() != Combat::Outcome::Ongoing) {
        if (ev.button == Button::B || ev.button == Button::C) host.finishCombat();
        return;
    }
    // A linked duel is running the SAME fight on two devices with no turn-by-turn
    // traffic (game_pvp.cpp), so anything that pauses or ends this copy of it would
    // desync the pair: the Exploit picker stops the clock for a human, and fleeing ends
    // a fight the opponent is still playing. Both are inert for the duration.
    // beginPvpBattle grants zero Exploit uses, and this blocks the input side.
    const bool duel = host.pvpFighting();

    // A+C opens the Exploit picker: the chord arrives here because
    // onButton bypasses its early-out in the combat state. The picker lists the
    // pet's moves, any combat-usable items from the live inventory, and (while the
    // player belongs to one) their crew's signature Exploit (game_crew.cpp).
    if (ev.chordAC) {
        if (!duel) host.openExploitPicker();
        return;
    }
    if (combat.overrideOpen()) {                  // standard A/B/C inside the picker
        if (ev.button == Button::A) combat.cycleOverride();
        else if (ev.button == Button::B) {
            // B descends at the band level and commits inside a band, so each level
            // gives B one meaning. A picker holding a single band never has a level
            // to descend, so B commits there as it always did.
            if (combat.enterOverrideBand()) return;
            combat.commitOverride();
            host.playSound(Sound::ExploitFire);
            // If a USE-ITEM was committed, Combat has already patched Health; the
            // Game now consumes the stack + applies the item's out-of-combat effect.
            if (const char* id = combat.takeCommittedItem()) host.consumeCombatItem(id);
        } else if (ev.button == Button::C) {
            // C is Cancel at both levels; what it cancels is the band you opened,
            // and then the picker.
            if (!combat.leaveOverrideBand()) combat.cancelOverride();
        }
        return;
    }
    // B CYCLES the stat panel: closed -> VS -> KIT -> closed (combat_screen.h). A
    // live readout that does NOT pause the fight, so paging through it costs turns and
    // is a real decision rather than a free look.
    if (ev.button == Button::B) {
        statsPage_ = (statsPage_ + 1) % (kCombatStatPages + 1);
        return;
    }
    // Auto-play: A fast-forwards the current beat, C flees / quits. A fast-forward is
    // safe in a duel (it only advances THIS screen's playback of an already-decided
    // fight), but fleeing is not (see `duel` above). ...and an arena bout is the
    // second fight with no exit: forfeiting a bracket mid-match would let a losing
    // draw be replayed, which is the whole stake gone. The way out of the arena is
    // the bracket screen, before a match starts.
    const bool noExit = duel || host.fight().caller == FightCaller::Tourney;
    if (ev.button == Button::A) {
        host.advanceCombatTurn();
    } else if (ev.button == Button::C && !noExit) {
        combat.flee();
        host.playCombatTurnSound();   // a failed flee hands the rival a free swing
    }
}

// --- The stage clock -----------------------------------------------------------

void FightScreen::closed(uint32_t nowMs) {
    lastAnimMs_ = nowMs;
    // Off the fight, off the disguise: every fight opens bare.
    camoLevel_ = 0;
    camoWorn_ = nullptr;
    camoLeaving_ = nullptr;
}

bool FightScreen::tick(FightScreenHost& host, uint32_t nowMs) {
    if (nowMs - lastAnimMs_ < static_cast<uint32_t>(kCombatAnimMs)) return false;
    lastAnimMs_ = nowMs;
    animBeat_++;
    host.liveFight().hitBeat++;
    // A guardian's SWARM is a fighter, so it moves on the clock the fighters move on
    // rather than on the dissolve's: the stage is an ~8fps screen, and a body stepping
    // four times faster than the creature beside it would read as two screens composited
    // together. The mood is a health band (Game::guardianFlockMood), so the flock visibly
    // comes apart as it loses.
    if (host.fight().combat.enemy().isSwarm) host.stepGuardianFlock();
    stepCamo(host);
    return true;
}

// FX_CAMO. Whose colours the pet is in is read fresh every tick rather than latched at
// the swing: a borrowed move is still the pet's last cast while the rival takes its turn,
// so the colours hold until the pet itself casts something else. A beat driving this
// instead would hand the change to whichever fighter swung last, and a counter-attack
// would strip it.
//
// Both seats are passed, because the question is about the PAIR: which source a cast
// names depends on what the fighter opposite is carrying (camoTarget). Only the target is
// resolved here. Turning it into tones ranks a sprite's colours, which the draw does once
// per repaint.
void FightScreen::stepCamo(const FightScreenHost& host) {
    const Combat& combat = host.fight().combat;
    const bool flip = host.combatLocalIsEnemySide();
    const Combatant& self = flip ? combat.enemy() : combat.player();
    const Combatant& rival = flip ? combat.player() : combat.enemy();
    const SpriteData* wear =
        host.camoSpriteForTarget(camoTarget(self, rival), rival.spriteName, self.stage);
    // Trading one borrowed palette for ANOTHER restarts the scatter with the old one held
    // behind it (draw passes it as camo.h's `from`), so the change reads as one disguise
    // dissolving into the next. Gated on the art: consecutive casts that name the same
    // creature (the fighter opposite, or its line) keep one disguise, and re-running the
    // dissolve into the same colour would flicker the pet. Only while it is actually
    // wearing something, too: from bare there is nothing to dissolve out of and the level
    // simply rises.
    if (wear && wear != camoWorn_ && camoLevel_ > 0) {
        camoLeaving_ = camoWorn_;
        camoLevel_ = 0;
    }
    if (wear) camoWorn_ = wear;
    camoLevel_ = camoAdvance(camoLevel_, wear != nullptr);
    // The palette being left is only true mid-dissolve. Once the front is past every
    // pixel, or the pet is returning to its own colours, what the unflipped pixels wear
    // is the creature itself.
    if (!wear || camoLevel_ == 255) camoLeaving_ = nullptr;
}

// --- Draw ----------------------------------------------------------------------

void FightScreen::draw(Framebuffer& fb, const FightScreenHost& host) const {
    const Combat& combat = host.fight().combat;
    const ContentRegistry& registry = host.content();
    // In a duel BOTH combatants are remote-describable pets, so the player-side sprite
    // comes from the fight: on a guest's screen the player_ slot holds the HOST's
    // creature (core/model/pvp_battle.h).
    const bool duel = host.pvpFighting();
    const SpriteData* ps = duel ? registry.sprite(combat.player().spriteName)
                                : host.petSprite();
    const SpriteData* es = registry.sprite(combat.enemy().spriteName);

    // Both duellists read their own pet in the left seat on the zoned gauge, like every
    // other fight: `localIsEnemySide` tells the screen to swap the roles for the guest,
    // whose pet occupies Combat's enemy_ slot. Display only: mirroring the fight itself
    // would desync the two devices.
    CombatSides sides;
    if (duel) {
        sides.rivalLabel = "RIVAL";
        sides.localLabel = "YOU";
        sides.localIsEnemySide = host.combatLocalIsEnemySide();
    }
    // An arena bout is the other pet-vs-pet fight, and the other one with no way out,
    // so it borrows the duel's caption for the opposite seat and drops the RUN hint.
    const bool arena = host.fight().caller == FightCaller::Tourney;
    if (arena) sides.rivalLabel = "RIVAL";
    sides.canRun = !duel && !arena;

    // The beaten rival's outro, over the beats finishCombat() holds the result for.
    // WHICH fights get one is combatOutroEligible(): shared with the tick, which has
    // to hold the auto-dismiss back for the same fights. This side only picks which of
    // the two dissolves plays.
    const uint32_t teachable = host.rivalTeachableMoveMask();
    CombatOutro outro;
    if (host.combatOutroEligible()) {
        outro.kind = teachable != 0 ? CombatOutro::Kind::Absorb : CombatOutro::Kind::Shred;
        outro.beat = host.fxBeat();
    }
    // What the KIT page marks as worth winning: the same mask the outro's dissolve
    // decides from, so the panel and the fight's last beat agree.
    RivalPrizes prizes;
    prizes.mask = teachable;
    // What the pet is currently wearing (FX_CAMO). The level and both sprites are standing
    // state the tick maintains; this draw only ranks each one's colours into a ladder.
    CombatCamo camo;
    camo.level = camoLevel_;
    camo.ramp = camoWorn_ ? camoRampFrom(*camoWorn_) : CamoRamp{};
    camo.leaving = camoLeaving_ ? camoRampFrom(*camoLeaving_) : CamoRamp{};
    // A rival with no sheet that is nonetheless a creature: the area guardian, drawn as
    // its flock into the seat the stage reserved (FX_SWARM). The same body the pet
    // spoke to on the hail, moved into the fighter's cell when the fight opened.
    SwarmView rivalSwarm;
    const bool swarmRival = combat.enemy().isSwarm && !sides.localIsEnemySide;
    if (swarmRival) rivalSwarm = swarmViewOf(host.shiboleetMeeting().flock);
    CombatView v;
    v.playerSprite = ps;
    v.enemySprite = es;
    v.beat = host.screenBeat();
    v.animBeat = animBeat_;
    v.hitBeat = host.fight().hitBeat;
    v.statPage = statsPage_;
    v.sides = sides;
    v.outro = outro;
    v.prizes = prizes;
    v.camo = camo;
    v.scene = host.stageScene();
    v.rivalSwarm = swarmRival ? &rivalSwarm : nullptr;
    drawCombat(fb, combat, v);
}

}  // namespace mal
