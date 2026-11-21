#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/content/areas/darkweb_crawl/area.h"
#include "core/app/game_internal.h"   // backupDriveAchievement: declared, defined here
#include "core/ui/combat_screen.h"
#include "core/ui/expl_screen.h"
#include "core/model/lcg.h"

namespace mal {

// The combat activity ---------------------------

// The rule for what a MIRROR is, in the one place both seams can reach it. Compared by
// species id rather than by anything the fight computed: two pets of one species are the
// mirror whatever their levels, kits or branches did, and that is what the operator sees.
// Cheap and idempotent, so a caller fires it on every bout without guarding.
void Game::noteMirrorMatch(const char* opponentCreatureId) {
    if (!opponentCreatureId || !pet_.creature || !pet_.creature->id) return;
    if (std::strcmp(pet_.creature->id, opponentCreatureId) == 0)
        unlockAchievement(ach::kHashCollision);
}

const char* backupDriveAchievement(Combatant::BackupUse used, Combat::Outcome outcome) {
    // Overwhelmed is read FIRST because a mutual KO resolves as a Win: the pet still
    // didn't get up, and what the player watched was the restore failing.
    if (used == Combatant::BackupUse::None) return nullptr;
    if (used == Combatant::BackupUse::Overwhelmed) return ach::kShatteredPlatter;
    if (outcome == Combat::Outcome::Lose || outcome == Combat::Outcome::Draw)
        return ach::kNeededMoreBackup;               // a draw is a loss
    if (outcome == Combat::Outcome::Win) return ach::kBackUpAndDriven;
    return nullptr;   // Fled: the run isn't over, so neither is the story
}

void Game::settleBackupDrive() {
    // Every path out of a fight lands here (applyCombatResult for wild/Sim, and every
    // boss round), so a spent drive is settled in exactly one place. It is spent for the
    // rest of its 1h window, win or lose.
    if (fight_.combat.player().backupUse == Combatant::BackupUse::None) return;
    pet_.armed.backupShieldUntilMs = 0;
    if (const char* id = backupDriveAchievement(fight_.combat.player().backupUse,
                                                fight_.combat.outcome()))
        unlockAchievement(id);
}

Combatant Game::buildPlayerCombatant() {
    // In the DARKWEB CRAWL the Silk Lode's apex rider is simply the weather: every fight
    // runs with the A+C picker reordered and drawn in the Cant, so what the pet can still
    // use is exactly what the device can still read. -1 is the "for the whole fight"
    // sentinel (Combatant::scrambleTurns) rather than a big number, because a big number
    // is an assumption about how long a fight lasts and this is not one.
    // Crib Sheet still answers it, which is why the ward is checked here and not only in
    // applyEffect: a mod that refused a three-turn rider but not the zone built out of it
    // would be selling half of what it says.
    // debugStartCombat is the dev/test hook and stays raw/deterministic.
    Combatant p = makePlayerCombatant(registry_, *pet_.creature, pet_.moves, pet_.mods);
    // Backup Drive's timed death-save (save v30): armed while lifetimeUptimeMs() is
    // still under the buff's deadline. Consumption is read back in applyCombatResult /
    // finishBossRound (Combatant::itemShieldFired) to clear pet_.armed.backupShieldUntilMs early.
    if (pet_.armed.backupShieldUntilMs != 0 && lifetimeUptimeMs() < pet_.armed.backupShieldUntilMs)
        p.itemShield = true;
    if (inDarkWebCrawl() && p.mods.mag(ModEffect::ScrambleWard) <= 0)
        p.scrambleTurns = kScrambleWholeFight;
    // Per-pet stat points: additive into the combat maths, stacking with the branch
    // multipliers and mods. Shared with the duel path's remote-fighter rebuild
    // (applyLevelStatPoints, core/model/combat.h) so the two can't drift apart. Earned
    // and granted points go in as one total. An Epic dish's off-level point counts
    // fully in a fight; only the level and the Rollback picker tell them apart.
    int points[kLevelStatCount];
    for (int i = 0; i < kLevelStatCount; ++i) points[i] = pet_.totalStatPoint(i);
    applyLevelStatPoints(p, points);
    // FRESH PATCH: timed on awake time, so every fight inside the window has it and none
    // spends it.
    if (patchBuffArmed()) p.powerMultPct += kPatchBuffPowerPct;
    return p;
}

void Game::startSimBattle(int tier) {
    if (!pet_.creature) return;
    Combatant p = buildPlayerCombatant();
    // A Sim Battle has no stakes (losing costs nothing), so there is no death to save
    // the pet from and no reason to spend a Rare item's shield on training. Stripped
    // here rather than in buildPlayerCombatant, which doesn't know the stakes.
    p.itemShield = false;
    CombatEnemy dummy = simDummy(tier);
    applySimDummyLevelScale(dummy, pet_.level);
    Combatant e = makeEnemyCombatant(registry_, dummy);
    lcgAdvance(rng_);     // advance for a fresh deterministic seed
    fight_ = Fight::started(FightCaller::Sim);
    fight_.combat.begin(p, e, Combat::Stakes::Safe, rng_, /*forceEnemyFirst=*/false,
                        /*carryPlayerHealth=*/-1, exploitUsesPerBattle());
    fx_.restart();
    nav_ = Nav::Combat;
    dirty_ = true;
    playSound(Sound::CombatStart);
}

// --- FightScreenHost: the Combat screen's way back into the game --------------

const SpriteData* Game::petSprite() const {
    return pet_.creature ? registry_.creatureSprite(*pet_.creature) : nullptr;
}

void Game::openExploitPicker() {
    // The pet's Cant goes in with the picker, because it decides how much of a
    // scrambled move the pet can read (MoveDef::scrambleTurns) and Combat has no
    // other source for it. The sigils are the device's, not the fight's.
    fight_.combat.openOverride(combatItemOptions(), crewExploitOption(), op_.cant.sigils);
}

void Game::stepGuardianFlock() { shib_.flock.step(guardianFlockMood()); }

int Game::combatBeatsForTurn() const {
    // Combat already tracks the same-actor streak for its own combo-damage bonus
    // (Combat::streakCount_): this reuses it. streakCount()/ streakIsPlayer() describe
    // the LAST resolved hit; playerTurnNext() names who's about to act, so if that
    // matches, the upcoming hit extends the run by one. Ramps the wait down from
    // kCombatBeatsPerTurn, floored at kCombatMinBeatsPerTurn.
    const Combat& c = fight_.combat;
    const bool nextIsPlayer = c.playerTurnNext();
    const int upcoming = (c.streakCount() > 0 && nextIsPlayer == c.streakIsPlayer())
                              ? c.streakCount() + 1
                              : 1;
    const int beats = kCombatBeatsPerTurn - (upcoming - 1);
    return beats < kCombatMinBeatsPerTurn ? kCombatMinBeatsPerTurn : beats;
}

void Game::advanceCombatTurn() {
    fight_.combat.step();
    fight_.turnBeat = 0;
    fight_.hitBeat = 0;   // restart the impact-punch decay for drawCombat
    playCombatTurnSound();
    // FX_CAMO deliberately takes nothing from here. The colours the pet is wearing are a
    // reading of its LIVE cast, eased every anim tick (tick(), camoAdvance), so this
    // step changes them only by changing what the pet holds. The rival's turn never
    // touches them.
}

bool Game::exploreFightLive() const {
    return walk_.active && nav_ == Nav::Combat &&
           (fight_.caller == FightCaller::Wild || fight_.caller == FightCaller::Boss);
}

void Game::playCombatTurnSound() {
    const Combat& c = fight_.combat;
    switch (c.outcome()) {
        case Combat::Outcome::Ongoing: break;
        case Combat::Outcome::Fled: playSound(Sound::Fled); return;
        default: playSound(Sound::Knockout); return;
    }
    // Nobody is watching an auto-explore fight swing by swing; a cue every turn would
    // only be noise from a device sitting in a pocket. The start and the KO still sound.
    if (exploreFightLive()) return;
    if (c.lastWasStunned()) playSound(Sound::Stunned);
    // Only a swing gets a hit cue: the passive ticks and wind-ups would make every turn
    // beep, and a shielded or ransomed swing that moved no Health reads as a block.
    else if (!c.lastWasStrike()) return;
    else if (c.lastDamage() == 0 || c.lastRansomed()) playSound(Sound::Blocked);
    else playSound(c.lastByPlayer() ? Sound::HitDealt : Sound::HitTaken);
}

std::vector<OverrideItem> Game::combatItemOptions() const {
    // The Exploit picker's USE-ITEM section: every owned stack whose ItemDef
    // marks a combatHeal. Order follows the inventory so it's stable/testable.
    std::vector<OverrideItem> out;
    for (const auto& s : inventory_.stacks()) {
        if (s.qty <= 0) continue;
        const ItemDef* d = registry_.item(s.id);
        if (d && d->combatHeal > 0)
            out.push_back({d->id, d->displayName, d->combatHeal});
    }
    return out;
}

void Game::consumeCombatItem(const char* id) {
    const ItemDef* d = registry_.item(id);
    if (!d) return;
    inventory_.remove(id, 1);
    // Interpretation "1 and 2": the item both patched combat Health (done in
    // Combat) AND applies its own out-of-combat effect to the persistent pet. The same
    // effects[] levers a normal Use would pull.
    applyItemEffects(*d);
    char buf[28];
    std::snprintf(buf, sizeof(buf), "USED %s", d->displayName);
    log_.push(LogEventType::ItemUsed, buf);
    markSaveDirty();
}

void Game::finishCombat() {
    fightScreen_.shutPanel();   // the next fight opens with the panel shut
    if (fight_.combat.outcome() == Combat::Outcome::Win) playSound(Sound::BattleWin);
    else if (fight_.combat.lostOrDrawn()) playSound(Sound::BattleLose);
    // A duel has its own result path (a banner on the LINK screen and a log line; a
    // duel has no stakes, so no reward and no penalty), so it hands off before
    // applyCombatResult() the same way a boss round does.
    if (pvpFighting()) { finishPvpBattle(); return; }
    // A boss/gauntlet round has its own result path (advance/clear/fail + the
    // lump) with no wild loot rolls: hand off before applyCombatResult().
    if (fight_.caller == FightCaller::Boss) { finishBossRound(); return; }
    // An arena bout advances a bracket instead of paying a fight (game_tourney.cpp):
    // Safe stakes, no loot rolls, and nothing but the title pays. Same hand-off shape.
    if (fight_.caller == FightCaller::Tourney) { finishTourneyMatch(); return; }
    applyCombatResult();
    // Return to the caller, never a fixed layer. Sim-Battle returns to the
    // LOADOUT hub; a wild encounter returns to the idle habitat (explore-mode),
    // not the EXPL list or the carousel.
    if (fight_.caller == FightCaller::Wild) {
        // Explore win-streak: a win advances the streak (10 in a row →
        // unlock the sector's boss, unless already cleared); a loss cancels the mode
        // and resets the streak. That's the whole risk model.
        if (fight_.combat.outcome() == Combat::Outcome::Win) ++away_.won;
        else if (fight_.combat.lostOrDrawn()) ++away_.lost;
        if (fight_.combat.outcome() == Combat::Outcome::Win) {
            // 'Pedia malbeast "defeated": a WON live wild fight
            // (never a boss round. Those return via finishBossRound() above before
            // ever reaching here, and Sim never reaches this branch either).
            if (const int idx = wildMalbeastIndex(fight_.combat.enemy().name);
                idx >= 0 && !(op_.reveals.malbeastDefeatedMask & (1u << idx))) {
                op_.reveals.malbeastDefeatedMask |= static_cast<uint16_t>(1u << idx);
                markSaveDirty();
            }
            if (walk_.active) {
                // In the endless DeepWeb Dive an armed Deep-Learning Module/Core advances
                // the streak (= depth) by more than one step per win, so a
                // blitzing endgame pet catches back up to a genuine struggle faster.
                // Every other area always advances by exactly 1.
                walk_.streak += inDeepWebDive() ? walk_.dive.depthMultiplier : 1;
                // 10 wins in a row unlock the ARMED sub-area's boss, unless
                // it's already cleared (re-farming a done sub-area never re-arms it).
                if (walk_.streak >= kExploreStreakToBoss &&
                    walk_.sector >= 0 && walk_.sector < kExplSectors &&
                    walk_.sub >= 0 && walk_.sub < kExplSubAreas &&
                    !op_.world.subCleared[walk_.sector][walk_.sub])
                    op_.world.subBossUnlocked[walk_.sector][walk_.sub] = true;
                // In the endless DeepWeb Dive the streak IS the DEPTH. Two records get
                // it: the pet's own (read back by the Checkpoint Bell) and its species'
                // record, which outlives the pet and feeds the depth achievements
                // (device best, best per line, and how many species have been taken
                // deep).
                if (inDeepWebDive()) {
                    if (walk_.streak > pet_.bestDeepWebDepth) pet_.bestDeepWebDepth = walk_.streak;
                    recordSpeciesDive(pet_.creature, walk_.streak);
                }
                // The crawl keeps its own record and pays the thing it exists to pay: a
                // SIGIL every kDarkWebSigilEveryN wins. It is credited as a spent shake
                // rather than as a free letter, so the two doors onto the Cant meet in one
                // ledger and the aerial's captures are never devalued by the zone.
                if (inDarkWebCrawl()) {
                    if (walk_.streak > pet_.bestDarkWebDepth) pet_.bestDarkWebDepth = walk_.streak;
                    if (kDarkWebSigilEveryN > 0 && walk_.streak > 0 &&
                        walk_.streak % kDarkWebSigilEveryN == 0 &&
                        sigilsKnown() < kCantSigils) {
                        op_.cant.learnNextSigil();
                        log_.push(LogEventType::ItemGained, "LEARNED A SIGIL");
                    }
                }
            }
        } else if (fight_.combat.lostOrDrawn()) {
            // A loss ends the run: a draw is one. What rode along with it ends with it.
            const bool redive = inDeepWebDive() && op_.dive.autoRedive;
            walk_.end();
            walk_.redive = redive;
        }
        // Post-encounter status readout: a FOUGHT battle (win or loss, a draw
        // being a loss) parks on the BANDWIDTH/FRAG readout before the habitat. A
        // flee never reaches finishCombat() with those stakes touched, so it skips
        // straight back (returnToExplore also surfaces an awakened-guardian
        // rank-up).
        if (fight_.combat.outcome() == Combat::Outcome::Win || fight_.combat.lostOrDrawn()) {
            nav_ = Nav::PostEncounter;
            fight_.result.readoutUntilMs = nowMs_ + kPostEncounterMs;
        } else {
            returnToExplore();
        }
    } else {
        // Sim-Battle came from PRACTISE, so it returns to the LOADOUT hub with that
        // row still focused.
        leaveLoadoutTab();
    }
    dirty_ = true;
    persistSave();
}

bool Game::combatCanTeach() const {
    // Only a fight against something the world owns. A duel or arena bout fights another
    // operator's pet and a Sim fights a dummy. Neither rolls a drop, so neither shows
    // anything to win.
    return !pvpFighting() &&
           (fight_.caller == FightCaller::Wild || fight_.caller == FightCaller::Boss);
}

bool Game::combatOutroEligible() const {
    // ...and WON, which is the only thing the dissolve adds to the question above: the
    // KIT panel answers it live, mid-fight, off the same gate.
    return combatCanTeach() && fight_.combat.outcome() == Combat::Outcome::Win;
}

bool Game::combatDissolveRunning() const {
    return combatOutroEligible() && fx_.beat() < kAbsorbTotalBeats;
}

uint32_t Game::rivalTeachableMoveMask() const {
    // This is the roll's own filter (moveIsTeachable), because a mark here promises that
    // winning could teach the move. Anything the filter rejects (an owned move, the
    // innate jab, another line's exclusive) stays unmarked. A rival whose whole kit is
    // rejected returns 0, and shreds, which is the truth.
    if (!combatCanTeach()) return 0;
    // The rival BY ROLE is Combat's enemy_ in every fight that reaches here: the one
    // seating that differs (a duel guest, core/ui/combat_screen.h's CombatSides) is a
    // duel, which combatCanTeach has already refused.
    uint32_t mask = 0;
    const std::vector<const MoveDef*>& kit = fight_.combat.enemy().moves;
    // 32 is far above any kit the game builds (a boss carries the depth spine, its
    // area's rider and at most kMaxBossTeaches), so the bound only guards the shift.
    for (size_t i = 0; i < kit.size() && i < 32; ++i)
        if (moveIsTeachable(kit[i])) mask |= 1u << i;
    return mask;
}

void Game::startDummyFight(Combat::Stakes stakes) {
    if (!pet_.creature) return;
    beginStagedFight(makePlayerCombatant(registry_, *pet_.creature, pet_.moves, pet_.mods),
                     simDummy(loadout_.simTier()), stakes);
}

void Game::startLethalFight(Combat::Stakes stakes) {
    if (!pet_.creature) return;
    Combatant p = makePlayerCombatant(registry_, *pet_.creature, pet_.moves, pet_.mods);
    // Huge Health and an overwhelming move against a one-Health pet, so the loss is
    // certain whatever the pet's moves hit for.
    p.maxHealth = 1;
    beginStagedFight(p, CombatEnemy{"Lethal", "SPR_PET_CACHEMUTT", 3, 9999, 99, {"packet_storm"}},
                     stakes);
}

void Game::startFightVs(const CombatEnemy& spec, Combat::Stakes stakes) {
    if (!pet_.creature) return;
    Combatant p = makePlayerCombatant(registry_, *pet_.creature, pet_.moves, pet_.mods);
    beginStagedFight(p, spec, stakes);
}

void Game::beginStagedFight(const Combatant& p, const CombatEnemy& spec, Combat::Stakes stakes) {
    Combatant e = makeEnemyCombatant(registry_, spec);
    lcgAdvance(rng_);
    fight_ = Fight::started(FightCaller::Sim);   // a staged fight always returns to the hub
    fight_.combat.begin(p, e, stakes, rng_,
                        /*forceEnemyFirst=*/false, /*carryPlayerHealth=*/-1,
                        exploitUsesPerBattle());
    fx_.restart();
    nav_ = Nav::Combat;
    dirty_ = true;
}

void Game::fillLoadout() {
    if (!pet_.creature) return;
    // Moves: the first owned move that matches the slot's stamped kind and isn't
    // already sitting in an earlier slot (equipping a move MOVES it, so reusing one
    // would empty the slot behind us and leave the page shorter than it should be).
    const int slots = MoveLoadout::slotsForStage(pet_.creature->stage);
    for (int s = 0; s < slots; ++s)
        for (const char* id : pet_.moves.owned()) {
            const MoveDef* m = registry_.move(id);
            if (!m || m->kind != slotRequiredKind(s)) continue;
            if (pet_.moves.slotOf(id) >= 0) continue;
            pet_.moves.equip(s, m->id);
            break;
        }
    // Mods: any three distinct rows, placed directly (setEquipped, no spare consumed).
    int filled = 0;
    for (const ModDef* d : registry_.allMods()) {
        if (filled >= kModSlots) break;
        if (pet_.mods.slotOf(d->id) >= 0) continue;
        pet_.mods.setEquipped(filled++, d->id);
    }
    markSaveDirty();
}


// The wild a walk meets here, at full strength for where it was met: the zone's own pair
// and its scaling. The Wi-Fi guardian (game_net.cpp) rolls through this too, so it is the
// same creature a walk in the same place would meet.
CombatEnemy Game::rollWild() {
    CombatEnemy wild;
    lcgAdvance(rng_);                    // roster variant roll
    if (inEndlessZone()) {
        // Both endless zones field a tier-3 pair (the crawl its own, kDarkWebWilds) and
        // scale it to the PET's level, ramped by the run's current win-streak
        // (walk_.streak) so pushing deeper gradually punches the pet up instead of
        // sitting at flat parity forever. The dive's curve has a foothold and spends a
        // fraction of a level's stat points; the crawl's has neither.
        wild = inDarkWebCrawl() ? wildMalbeastOf(kDarkWebWilds, 3, rng_ >> 16)
                                : wildMalbeast(3, rng_ >> 16);
        lcgAdvance(rng_);                // the zone's stat/kit roll
        if (inDarkWebCrawl())
            applyDarkWebScale(wild, pet_.level, walk_.streak, rng_);
        else
            applyDeepWebScale(wild, pet_.level, walk_.streak, rng_);
    } else {
        wild = wildMalbeastIn(walk_.sector, rng_ >> 16);     // the area's pair
        applyWildSubAreaRamp(wild, walk_.sector, walk_.sub);  // depth ramp
        lcgAdvance(rng_);                // this meeting's body
        rollWildBody(wild, walk_.sub, rng_);
    }
    // 'Pedia malbeast "seen": the wild roll itself is the
    // glimpse, regardless of what happens next (fight/flee/sinkhole/auto-resolve).
    // wildMalbeastIndex returns -1 for anything outside the wild roster.
    if (const int idx = wildMalbeastIndex(wild.name);
        idx >= 0 && !(op_.reveals.malbeastSeenMask & (1u << idx))) {
        op_.reveals.malbeastSeenMask |= static_cast<uint16_t>(1u << idx);
        markSaveDirty();
    }
    return wild;
}

void Game::startEncounter() {
    // THE FIRST ENCOUNTER IN A ZONE IS WHERE ITS CHAPTER FIRES (game_story.cpp). Before
    // the roll, because the chapter is about arriving and has to come before the first
    // malbeast. fireStory answers false for a zone with nothing authored or a chapter
    // already read, in which case this is an ordinary encounter and nothing here knows a
    // story exists; when it answers true the reader has the screen and calls back into
    // this function once it is done, by which time the chapter is marked read and the
    // gate falls straight through.
    if (fireStory(walk_.sector, StoryBeat::AreaIntro, StoryThen::Encounter)) return;
    const CombatEnemy wild = rollWild();
    // Auto-explore is a hands-off background mode: a wild encounter resolves
    // itself. There's no human at the intro to press Fight, and Flee is pointless
    // since a loss already cancels the mode + zeroes the streak (finishCombat). A
    // held Sinkhole Trap auto-bypasses for XP while the overlay's SINKHOLES switch
    // is on (Operator::useEncounterBypass); otherwise the fight auto-starts. The Fight/Flee/Sinkhole intro stands only for a
    // (currently unused) manual encounter path.
    if (walk_.active) {
        const ItemDef* bypass = op_.useEncounterBypass ? heldEncounterBypass() : nullptr;
        if (bypass) resolveSinkhole(*bypass);
        else fightWild(wild);
        return;
    }
    encounter_.open(wild);
    nav_ = Nav::Encounter;
}

void Game::fightWild(const CombatEnemy& wild) {
    startWildCombat(wild, /*forceEnemyFirst=*/false);
}

void Game::resolveFlee(const CombatEnemy& wild) {
    const bool escaped = rollD100() < kFleeChancePct;
    if (escaped) {
        std::snprintf(walk_.flavor, sizeof(walk_.flavor), "GOT AWAY");
        playSound(Sound::Fled);
        returnToExplore();
    } else {
        // A failed flee forces the fight, enemy first (retreat isn't free).
        startWildCombat(wild, /*forceEnemyFirst=*/true);
    }
}

const ItemDef* Game::heldEncounterBypass() const {
    for (const ItemDef* d : registry_.allItems())
        if (d->preEncounterXp > 0 && inventory_.count(d->id) > 0) return d;
    return nullptr;
}

int Game::encounterBypassHeld() const {
    int n = 0;
    for (const ItemDef* d : registry_.allItems())
        if (d->preEncounterXp > 0) n += inventory_.count(d->id);
    return n;
}

void Game::resolveSinkhole(const ItemDef& d) {
    const int xp = d.preEncounterXp;
    inventory_.remove(d.id, 1);
    addCombatXp(xp);
    char used[36];
    std::snprintf(used, sizeof(used), "USED %s", d.displayName);
    log_.push(LogEventType::ItemUsed, used);
    std::snprintf(walk_.flavor, sizeof(walk_.flavor), "%s +%d XP", d.displayName, xp);
    // Persist now: the debounced autosave can lose a level-up to a reboot. Every other XP
    // grant (finishCombat, finishBossRound) does the same, and this source skips combat,
    // so it needs its own call.
    persistSave();
    returnToExplore();
}

void Game::startWildCombat(const CombatEnemy& wild, bool forceEnemyFirst) {
    if (!pet_.creature) return;
    Combatant p = buildPlayerCombatant();
    // A friend's visit or a guardian's escort walks with the pet, so it fights the walk's
    // own fights and is spent one per fight; a Sim Battle or a bout away from the walk
    // never draws on it.
    if (walk_.active && walk_.escortBattles > 0) {
        p.powerMultPct += kAllyBuffPowerPct;
        --walk_.escortBattles;
    }
    Combatant e = makeEnemyCombatant(registry_, wild);
    lcgAdvance(rng_);
    fight_ = Fight::started(FightCaller::Wild);
    fight_.combat.begin(p, e, Combat::Stakes::Live, rng_, forceEnemyFirst,
                        /*carryPlayerHealth=*/-1, exploitUsesPerBattle());
    fx_.restart();
    nav_ = Nav::Combat;
    dirty_ = true;
    playSound(Sound::CombatStart);
}

}  // namespace mal
