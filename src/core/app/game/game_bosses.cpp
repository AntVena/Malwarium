#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/content/areas/area_defs.h"
#include "core/ui/expl_screen.h"

namespace mal {

bool Game::moveIsTeachable(const MoveDef* m) const {
    // The innate is in the owned set like anything else, so one check covers it: every
    // tier-1 wild swings it and every gauntlet opens with it, and none of that is a
    // move to be "learned" by a pet that already has it.
    if (!m || pet_.moves.owns(m->id)) return false;
    // A move exclusive to ANOTHER line is skipped rather than dropped: the equip gate
    // would refuse it anyway (MoveDef::line), so granting it would be a reward the pet
    // can never field. Generic moves (line == nullptr) drop to everyone, which is what
    // makes an area's apex rider worth hunting whatever you hatched.
    if (m->line && (!pet_.creature || !pet_.creature->line || std::strcmp(m->line, pet_.creature->line) != 0))
        return false;
    return true;
}

int Game::movesToLearnIn(const std::vector<const char*>& pool) const {
    // No pet, nothing to teach: an empty rack must not advertise a zone as holding
    // anything, and moveIsTeachable already refuses a line-exclusive move without one.
    if (!pet_.creature) return 0;
    int n = 0;
    for (const char* id : pool)
        if (moveIsTeachable(registry_.move(id))) ++n;
    return n;
}

int Game::areaMovesToLearn(int area) const {
    return movesToLearnIn(areaMovePool(area));
}
int Game::areaGauntletMovesToLearn(int area) const {
    return movesToLearnIn(areaGauntletMovePool(area));
}
int Game::subAreaMovesToLearn(int area, int sub) const {
    return movesToLearnIn(subAreaMovePool(area, sub));
}
int Game::deepWebMovesToLearn() const { return movesToLearnIn(deepWebMovePool()); }
int Game::darkWebMovesToLearn() const { return movesToLearnIn(darkWebMovePool()); }

bool Game::rollEnemyMoveDrop(const Combatant& from, int dropPct) {
    // You learn a move by BEATING something that knows it. The pool is the defeated
    // enemy's whole KIT (every move it could have used), so a win teaches the same set
    // however the fight went. What a given enemy is worth farming is therefore a
    // property of the enemy, legible before the first swing.
    //
    // It also means a move becomes findable purely by giving it to something that uses
    // it, with no drop table to keep in step with the roster. Filters to not-yet-owned
    // rather than reroll-looping, so "it knew nothing I don't" is a legitimate no-drop,
    // and that is also what keeps a re-run self-limiting without a decay curve: an enemy
    // you have learned out stops paying on its own.
    rng_ = rng_ * 1664525u + 1013904223u;
    if (static_cast<int>((rng_ >> 16) % 100) >= dropPct) return false;
    std::vector<const char*> candidates;
    auto consider = [&](const MoveDef* m) {
        if (!moveIsTeachable(m)) return;
        for (const char* c : candidates)
            if (std::strcmp(c, m->id) == 0) return;              // a kit may repeat
        candidates.push_back(m->id);
    };
    for (const MoveDef* m : from.moves) consider(m);
    // Own-line catch-up. A FALLBACK rather than a peer of the main pool: a pet raised
    // before one of its line moves existed still fills the gap in, on a win the enemy
    // taught nothing new. A fresh hatch owns its whole line kit already
    // (MoveLoadout::startingForLine), so this is normally a no-op.
    if (candidates.empty() && pet_.creature && pet_.creature->line)
        for (const MoveDef* m : registry_.allMoves())
            if (m->line && std::strcmp(m->line, pet_.creature->line) == 0) consider(m);
    if (candidates.empty()) return false;
    rng_ = rng_ * 1664525u + 1013904223u;
    const char* id = candidates[(rng_ >> 16) % candidates.size()];
    pet_.moves.grant(id);
    const MoveDef* m = registry_.move(id);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "LEARNED %s", m ? m->displayName : id);
    log_.push(LogEventType::ItemGained, buf);
    markSaveDirty();
    return true;
}

// Boss ladder ----------------------------------------------------

void Game::startSubAreaBoss(int area, int sub) {
    // A sub-area boss: a single strong malbeast, manually triggered from EXPL
    // once a 10-win streak unlocked it. A full clear marks the SUB-AREA cleared.
    if (!pet_.creature) return;
    gauntlet_ = GauntletRun::started(area, sub, subAreaBoss(area, sub));
    startBossRound(/*carryHealth=*/-1);             // round 0 starts at full Health
}

void Game::startAreaBoss(int area) {
    // The area boss: the 5 sub-area bosses fought back-to-back with carried
    // Health (no heal). Reachable once all 5 sub-areas are cleared; a full clear
    // clears the AREA (→ next area) + grants its Title.
    if (!pet_.creature) return;
    // ...and the THRESHOLD chapter goes in front of it, the same way the arrival one
    // goes in front of the first encounter (game_story.cpp). Its `then` carries the
    // area, because by the time the reader hands back nothing else remembers which
    // gauntlet was being opened.
    if (fireStory(area, StoryBeat::BossIntro, StoryThen::AreaBoss, area)) return;
    gauntlet_ = GauntletRun::started(area, /*sub=*/-1, areaBoss(area));
    startBossRound(/*carryHealth=*/-1);
}

void Game::startBossRound(int carryHealth) {
    const CombatEnemy* foe = gauntlet_ ? gauntlet_->foe() : nullptr;
    if (!foe) {
        gauntlet_.reset();                          // a gauntlet with no rounds never ran
        return;
    }
    Combatant p = buildPlayerCombatant();
    Combatant e = makeEnemyCombatant(registry_, *foe);
    rng_ = rng_ * 1664525u + 1013904223u;
    // Live stakes; carryHealth < 0 on round 0 (full), else the wounded Health the
    // pet limped out of the previous round with (no heal between).
    fight_ = Fight::started(FightCaller::Boss);
    fight_.combat.begin(p, e, Combat::Stakes::Live, rng_, /*forceEnemyFirst=*/false,
                        carryHealth, exploitUsesPerBattle());
    fxBeat_ = 0;
    nav_ = Nav::Combat;
    dirty_ = true;
    playSound(Sound::BossStart);
}

void Game::finishBossRound() {
    // Whether this call cleared the whole area. The tail needs only this from the
    // branches below, and it is the one beat with two chapters (game_story.cpp's
    // fireStoryPair). A re-run of a cleared gauntlet is not a first clear and reads
    // nothing, which the read-set already enforces; this is what keeps a sub-area clear
    // and a LOSS from asking in the first place.
    bool clearedArea = false;
    // Settled every round, not once at the end: a gauntlet rebuilds the player Combatant
    // (buildPlayerCombatant) between rounds, so a drive spent in one round must neither
    // re-arm for the next nor lose the achievement it earned.
    settleBackupDrive();
    // A boss fight is only ever opened by startBossRound, which holds a gauntlet.
    GauntletRun& run = *gauntlet_;
    const int sector = run.sector;                  // the outro chapters read it after the run ends
    if (fight_.combat.outcome() == Combat::Outcome::Win) {
        // Bank this round's boss roll (a gauntlet pays the lump at the end).
        run.bitsAccrued += bossBitsReward(run.gauntlet.stageRank, rng_);
        // ...and roll THIS round's boss for a move off its own kit, before the gauntlet
        // advances and replaces the combatant. Per round rather than once at the clear,
        // because each round is a different boss with a different kit: the signature
        // (sub 4) round is the only one carrying its area's apexThreatMoveId, so rolling
        // once at the end would make the marquee move the rarest thing in the game by
        // accident. It also means a 5-round area gauntlet pays five chances against a
        // sub-boss's one, which is the right proportion to what each costs to reach.
        //
        // No refarm decay, deliberately: refarmDropScalePct exists to tax grinding one
        // wild rung, and a cleared gauntlet is a deliberate re-run, not a farm. The
        // not-yet-owned filter already makes a boss stop paying once it has taught
        // everything it knows, which is a cap that needs no curve.
        rollEnemyMoveDrop(fight_.combat.enemy(), kBossMoveDropPct);
        const int surviving = fight_.combat.player().health;
        ++run.round;
        ++op_.tallies.bossWins;   // per ROUND, so a three-boss gauntlet counts as three bosses beaten
        if (!run.cleared()) {
            startBossRound(surviving);              // next round, no heal: stays in Combat
            return;
        }
        // Boss beaten: pay the lump, record the clear. A SUB-AREA boss marks just that
        // sub-area cleared; the AREA boss clears the whole area → the next area unlocks +
        // the Title is granted.
        op_.bits += applyCombatBitsBonus(run.bitsAccrued);   // Scraping Cluster Expansion
        addFightXp(applyCombatXpBonus(kWildWinXpReward));  // Well-Fed XP Boost
        if (!run.clearsArea() && run.sub < kExplSubAreas) {
            const bool firstClear = !op_.world.subCleared[sector][run.sub];
            op_.world.subCleared[sector][run.sub] = true;
            log_.push(LogEventType::CombatWon, "SUB-AREA CLEARED");
            // a sub-area boss's FIRST clear has a chance to drop one of the
            // area's mods (rarity-weighted). Re-clears never re-roll: permanent items
            // aren't farmable. The rolled equip-level gate decides when it's fieldable.
            if (firstClear) {
                rng_ = rng_ * 1664525u + 1013904223u;
                if (static_cast<int>((rng_ >> 16) % 100) < kModSubBossDropPct)
                    if (const char* id = rollAreaModId(sector))
                        grantMod(id);
            }
        } else {
            clearedArea = true;
            const bool firstClear = !op_.world.sectorCleared[sector];
            const SceneId placeWon = area(sector).scene;
            const bool hadPlace = backgroundOwned(placeWon);
            op_.world.sectorCleared[sector] = true;
            unlockTitle(sector);            // clearing the area grants its Title
            // ...and the AREA ITSELF, as a background the operator may stand their pet
            // in. The Title says where they have been; this is a piece of it to keep.
            if (!hadPlace) announceBackground(placeWon);
            log_.push(LogEventType::CombatWon, "AREA CLEARED");
            // the area boss's first clear guarantees a mod from the area table, the
            // reward for beating the 5-stage gauntlet. A re-run is not a first clear,
            // so it pays its Bits lump and XP (above) and no mod: the same rule a
            // re-farmed sub-area follows.
            if (firstClear)
                if (const char* id = rollAreaModId(sector))
                    grantMod(id);
            // Auto-progress moves off the gauntlet here. autoProgressStep cannot tell
            // whether the gauntlet is next or just finished, because the armed sub-area
            // is the area's last either way. Re-aiming at the next area resets the
            // streak, so the shared hook finds nothing to do and the rotation carries
            // on.
            if (autoProgress_) startExplore(nextOpenArea(sector), 0);
        }
    } else {
        // Any non-win loses the whole gauntlet. A loss takes the standard live-stakes Frag hit once
        // (×Bad-branch multiplier); a flee just bails (no penalty, same as a wild flee). A draw is
        // a loss. No sector clear either way. A boss loss ALSO cancels explore-mode + resets the
        // streak, but the boss-unlock PERSISTS, so the player can retry it without grinding again.
        if (fight_.combat.lostOrDrawn()) {
            const int frag = kWildLossFrag * fight_.combat.player().fragMultPct / 100;
            pet_.model.setFragmentation(pet_.model.fragmentation() + frag);
            log_.push(LogEventType::CombatLost, "GAUNTLET FAILED");
        }
        walk_.end();
    }
    // Cleared, lost or fled, the gauntlet is over.
    gauntlet_.reset();
    // A cleared area's two OUTRO chapters, read back to back: what was just beaten,
    // then the place closing behind the pet. They fire AFTER the clear bookkeeping (and
    // after auto-progress has re-aimed the walk at the next area), so the reader hands
    // back to a walk already aimed at its next stop: StoryThen::Walk is the
    // returnToExplore below, deferred until the chapters are done.
    if (!(clearedArea && fireStoryPair(sector, StoryBeat::BossOutro,
                                       StoryBeat::AreaOutro, StoryThen::Walk)))
        returnToExplore();                           // back to the idle habitat
    dirty_ = true;
    markSaveDirty();
    persistSave();
}

}  // namespace mal
