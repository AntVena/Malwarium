// game_pedia.cpp — reveal-state + achievement bookkeeping.
//
// Two related jobs. The 'Pedia reveal tiers (save v25): the glimpsed-but-not-hatched
// creature set and the raised-species tally. And the achievement system (save v40): the
// earned/notified bitsets, the per-tick sweep that unlocks every countable row, the
// reward payout, and the home-screen banner queue that is the ONLY way an unlock reaches
// a player on a device with no achievement browser.
//
// The catalogue itself is data (content/content_achievements.cpp) and the set sizes its
// kGoalAll rows measure against are a pure helper (app/game_achievements.h) — so what
// lives here is strictly the player's side of it: how far along they are, and what to do
// when a row lands. Adding an achievement touches none of this file; adding a new PROGRESS
// SOURCE is one case in achValue(), and a new REWARD is one case in
// applyAchievementRewards().
//
// The handful of rows that can't be swept (AchSeries::Event — the moment leaves no
// counter behind) are fired by hand from wherever they happen: hatch/evolution →
// game_lifecycle.cpp, Lockout resolve → game_lifecycle.cpp, a duel → game_pvp.cpp, the
// DevTools honeytoken → ap_server.h.
#include "core/content/content_backgrounds.h"  // the places an achievement pays out
#include "core/content/content_homes.h"   // sceneForCreature — a raised creature brings its place
#include "core/app/game.h"

#include <cstring>

#include "core/app/game_achievements.h"
#include "core/app/game_internal.h"

namespace mal {

void Game::markCreatureRaised(const char* id) {
    if (!id) return;
    for (const CreatureDef* c : raisedCreatures_)
        if (std::strcmp(c->id, id) == 0) return;    // already tallied — idempotent
    if (const CreatureDef* def = registry_.creature(id)) {
        // Raising a creature is also how its HOME becomes a background the operator may
        // choose (content/content_backgrounds.h). Ownership is derived from this very
        // list, so whether it is new has to be asked before the push.
        const SceneId home = sceneForCreature(*def);
        const bool had = backgroundOwned(home);
        raisedCreatures_.push_back(def);
        if (!had) announceBackground(home);
        markSaveDirty();
    }
}

bool Game::creatureRaised(const char* id) const {
    if (!id) return false;
    for (const CreatureDef* c : raisedCreatures_)
        if (std::strcmp(c->id, id) == 0) return true;
    return false;
}

void Game::markCreatureSeen(const char* id) {
    if (!id || creatureRaised(id)) return;          // "raised" always wins over "seen"
    for (const CreatureDef* c : seenCreatures_)
        if (std::strcmp(c->id, id) == 0) return;    // already glimpsed — idempotent
    if (const CreatureDef* def = registry_.creature(id)) {
        seenCreatures_.push_back(def);
        markSaveDirty();
    }
}

bool Game::creatureSeen(const char* id) const {
    if (!id) return false;
    for (const CreatureDef* c : seenCreatures_)
        if (std::strcmp(c->id, id) == 0) return true;
    return false;
}

bool Game::itemCollected(const char* id) const {
    if (!id) return false;
    for (const ItemDef* d : collectedItems_)
        if (std::strcmp(d->id, id) == 0) return true;
    return false;
}

void Game::sweepCollectedItems() {
    for (const auto& s : inventory_.stacks()) {
        if (s.qty <= 0 || itemCollected(s.id)) continue;
        if (const ItemDef* d = registry_.item(s.id)) {
            collectedItems_.push_back(d);
            markSaveDirty();
        }
    }
}

int Game::speciesDeepestDive(const char* creatureId) const {
    if (!creatureId) return 0;
    for (const SpeciesDive& s : speciesDives_)
        if (s.def && std::strcmp(s.def->id, creatureId) == 0) return s.depth;
    return 0;
}

int Game::deepestDiveEver() const {
    int best = 0;
    for (const SpeciesDive& s : speciesDives_)
        if (s.depth > best) best = s.depth;
    return best;
}

void Game::recordSpeciesDive(const CreatureDef* c, int depth) {
    if (!c || depth <= 0) return;
    for (SpeciesDive& s : speciesDives_) {
        if (s.def != c) continue;
        if (depth > s.depth) { s.depth = depth; markSaveDirty(); }
        return;
    }
    speciesDives_.push_back({c, depth});
    markSaveDirty();
}

// --- Achievements ----------------------------------------------------------------

bool Game::hasAchievement(const char* id) const {
    const AchievementDef* d = achievementById(id);
    return d && achBit(achEarned_, d->wire);
}

void Game::unlockAchievement(const char* id) {
    const AchievementDef* d = achievementById(id);
    // An unknown id is a call site naming a row that no longer exists. Nothing to do but
    // ignore it — the native gate is what catches the typo, not a runtime crash on a
    // device someone is carrying around.
    if (!d || achBit(achEarned_, d->wire)) return;   // unknown / already earned
    setAchBit(achEarned_, d->wire);
    applyAchievementRewards(*d);
    char line[kSaveTextCap];
    std::snprintf(line, sizeof(line), "* %s", d->displayName);
    log_.push(LogEventType::ItemGained, line);
    // ...and whatever PLACE this row pays out, if any. Cheaper than the ask-before /
    // ask-after the other three grants have to do: the early return above already
    // established the bit was clear, so a background keyed on this id was unowned a
    // line ago and is owned now, with no second question to get wrong.
    for (const BackgroundDef& b : kBackgrounds)
        if (b.source == BackgroundSource::Achieve && b.earnedById &&
            std::strcmp(b.earnedById, id) == 0)
            announceBackground(b.scene);
    markSaveDirty();
    // The banner is raised by tickAchievementBanner() rather than here: an unlock lands
    // mid-combat or mid-walk as often as not, and the announcement belongs on the home
    // screen where the player is actually looking.
}

void Game::applyAchievementRewards(const AchievementDef& d) {
    for (const AchievementReward& r : d.rewards) {
        switch (r.kind) {
            case AchievementReward::Kind::None:
                break;
            case AchievementReward::Kind::Bits:
                bits_ += r.magnitude;
                break;
            case AchievementReward::Kind::Item:
                // Straight into the bag — a Commendation Cache is opened from the VAULT
                // when the player chooses, so nothing here interrupts what they are doing.
                if (r.id && registry_.item(r.id)) inventory_.add(r.id, r.magnitude);
                break;
        }
    }
}

int Game::achValue(const AchievementDef& d) const {
    switch (d.series) {
        case AchSeries::Event:
            return 0;                       // never swept — fired at its own event
        case AchSeries::DeepWebDepth:
            return deepestDiveEver();
        case AchSeries::DeepWebDepthLine: {
            int best = 0;
            for (const SpeciesDive& s : speciesDives_)
                if (s.def && s.def->line && d.key &&
                    std::strcmp(s.def->line, d.key) == 0 && s.depth > best)
                    best = s.depth;
            return best;
        }
        case AchSeries::SpeciesAtDepth: {
            int n = 0;
            for (const SpeciesDive& s : speciesDives_)
                if (s.depth >= d.param) ++n;
            return n;
        }
        case AchSeries::BossWins:
            return bossWins_;
        case AchSeries::AreasCleared: {
            int n = 0;
            for (int i = 0; i < kAreaCount; ++i) n += sectorCleared_[i] ? 1 : 0;
            return n;
        }
        case AchSeries::SubAreasCleared: {
            int n = 0;
            for (int a = 0; a < kAreaCount; ++a)
                for (int s = 0; s < kSubAreasPerArea; ++s) n += subCleared_[a][s] ? 1 : 0;
            return n;
        }
        case AchSeries::MalbeastsDefeated: {
            int n = 0;
            for (int i = 0; i < kWildMalbeastCount; ++i) n += malbeastDefeated(i) ? 1 : 0;
            return n;
        }
        case AchSeries::SpeciesRaised:
            return speciesRaised();
        case AchSeries::LineRaised: {
            // Read off the RAISED tally, not what is held: a line is walked one stage at
            // a time, so nobody ever holds all of it at once.
            int n = 0;
            for (const CreatureDef* c : raisedCreatures_)
                if (c->line && d.key && std::strcmp(c->line, d.key) == 0) ++n;
            return n;
        }
        case AchSeries::FoodsCollected: {
            int n = 0;
            for (const ItemDef* it : collectedItems_)
                if (it->type == ItemDef::Type::Food) ++n;
            return n;
        }
        case AchSeries::RarityCollected: {
            int n = 0;
            for (const ItemDef* it : collectedItems_)
                if (static_cast<int>(it->rarity) == d.param) ++n;
            return n;
        }
        case AchSeries::ItemCollected:
            return itemCollected(d.key) ? 1 : 0;
        case AchSeries::RecipesKnown: {
            int n = 0;
            for (int i = 0; i < kMergeRecipeCount; ++i) n += recipeOwned(i) ? 1 : 0;
            return n;
        }
        case AchSeries::RigRowsOwned: {
            int n = 0;
            for (int i = 0; i < kRigUpgradeCount; ++i) n += rigLevel_[i] > 0 ? 1 : 0;
            return n;
        }
        case AchSeries::RigCappedLevels: {
            // Capped rows only, and clamped to each row's cap — an uncapped row (or an
            // over-bought one) must not be able to stand in for a row never touched.
            int n = 0;
            for (int i = 0; i < kRigUpgradeCount; ++i) {
                const int cap = kRigUpgrades[i].maxLevel;
                if (cap == kRigLevelUnlimited) continue;
                n += rigLevel_[i] < cap ? rigLevel_[i] : cap;
            }
            return n;
        }
        case AchSeries::NetworksSeen:
            return networksSeen_;
        case AchSeries::Handshakes:
            return handshakesSeen_;
        case AchSeries::BitsHeld:
            return bits_;
        case AchSeries::PetsRaised:
            return petsRaised_;
        case AchSeries::StackerWins:
            return stackerWins_;
        case AchSeries::ArcadePlays:
        case AchSeries::ArcadeWins:
        case AchSeries::ArcadeLosses: {
            int plays = 0, wins = 0;
            for (int i = 0; i < arcadeGameCount() && i < kArcadeMaxCabinets; ++i) {
                plays += arcadePlays_[i];
                wins += arcadeWins_[i];
            }
            if (d.series == AchSeries::ArcadeWins) return wins;
            if (d.series == AchSeries::ArcadeLosses) return plays - wins;
            return plays;
        }
        case AchSeries::ArcadeCabinetWins:
            return arcadeWins(arcadeGameIndexById(d.key));
        case AchSeries::ArcadeCabinetBest:
            return arcadeBestById(d.key);
        case AchSeries::TourneyWins:
            return tourneyWins_;
        case AchSeries::PvpWins:
            return pvpWins_;
        case AchSeries::MergesCooked:
            return mergesCooked_;
        case AchSeries::QuotesSolved:
            return quotesSolved();
        case AchSeries::TitlesUnlocked:
            return titlesUnlockedCount();
        case AchSeries::PeersMet:
            return static_cast<int>(peerLedger_.size());
        case AchSeries::SpeciesSeen:
            return static_cast<int>(seenCreatures_.size());
        case AchSeries::RackHeld:
            return rackCount();
        // The four COLLECTION reads of the same shelf. All of them walk the rack rather
        // than a tally, because every one is a live "at once" fact — a released pet has
        // to take its contribution with it. The walks are O(n^2) in the rack, which is a
        // container that tops out at 64 entries and is only read on the achievement
        // sweep; a set worth allocating would cost more than it saves.
        case AchSeries::RackSpecies: {
            int n = 0;
            for (size_t i = 0; i < rack_.size(); ++i) {
                bool dupe = false;
                for (size_t j = 0; j < i && !dupe; ++j)
                    dupe = std::strcmp(rack_[i].id, rack_[j].id) == 0;
                if (!dupe) ++n;
            }
            return n;
        }
        case AchSeries::RackLines: {
            // Counted over the SHIPPED line list, not over the ids on the shelf, so this
            // measures the same set achievementSeriesTotal does (kCreatureLineCount).
            int n = 0;
            for (int i = 0; i < kCreatureLineCount; ++i) {
                for (const SaveStoredPet& p : rack_) {
                    const CreatureDef* c = registry_.creature(p.id);
                    if (c && c->line && std::strcmp(c->line, kCreatureLines[i].id) == 0) {
                        ++n;
                        break;
                    }
                }
            }
            return n;
        }
        case AchSeries::RackLineHeld: {
            // DISTINCT creatures of one line: a shelf of five Pingcubs is one of the
            // Ransomware line's thirteen, not five of them.
            int n = 0;
            for (size_t i = 0; i < rack_.size(); ++i) {
                const CreatureDef* c = registry_.creature(rack_[i].id);
                if (!c || !c->line || !d.key || std::strcmp(c->line, d.key) != 0) continue;
                bool dupe = false;
                for (size_t j = 0; j < i && !dupe; ++j)
                    dupe = std::strcmp(rack_[i].id, rack_[j].id) == 0;
                if (!dupe) ++n;
            }
            return n;
        }
        case AchSeries::RackTwins: {
            int best = 0;
            for (size_t i = 0; i < rack_.size(); ++i) {
                int n = 0;
                for (size_t j = 0; j < rack_.size(); ++j)
                    if (std::strcmp(rack_[i].id, rack_[j].id) == 0) ++n;
                if (n > best) best = n;
            }
            return best;
        }
        case AchSeries::StepsWalked:
            return static_cast<int>(lifetimeSteps_);
    }
    return 0;
}

void Game::sweepAchievements() {
    sweepCollectedItems();
    // Adjacent rungs of one ladder share a series/key/param, and the table is authored
    // that way, so a one-entry memo spares the repeated walk over creatures/items/stacks
    // that the set-counting series do. Nothing here is exact-timing-sensitive: a row
    // lands within kAchSweepIntervalMs of its condition, which is well under one frame's
    // worth of noticing.
    AchSeries memoSeries = AchSeries::Event;
    const char* memoKey = nullptr;
    int memoParam = 0;
    int memoValue = 0;
    bool memoValid = false;
    for (int i = 0; i < kAchievementCount; ++i) {
        const AchievementDef& d = kAchievements[i];
        if (d.series == AchSeries::Event) continue;      // fired at its own site
        if (achBit(achEarned_, d.wire)) continue;        // already earned — skip the work
        const int goal = achievementGoal(d);
        if (goal <= 0) continue;                          // kGoalAll on an unbounded series
        const bool sameAsMemo = memoValid && d.series == memoSeries &&
                                d.param == memoParam && d.key == memoKey;
        if (!sameAsMemo) {
            memoSeries = d.series;
            memoKey = d.key;
            memoParam = d.param;
            memoValue = achValue(d);
            memoValid = true;
        }
        if (memoValue >= goal) unlockAchievement(d.id);
    }
}

int Game::achPendingNotify() const {
    int n = 0;
    for (int i = 0; i < kAchievementCount; ++i) {
        const AchievementDef& d = kAchievements[i];
        if (achBit(achEarned_, d.wire) && !achBit(achNotified_, d.wire)) ++n;
    }
    return n;
}

const AchievementDef* Game::achBanner() const {
    if (achBannerWire_ < 0) return nullptr;
    for (int i = 0; i < kAchievementCount; ++i)
        if (kAchievements[i].wire == achBannerWire_) return &kAchievements[i];
    return nullptr;
}

const EggLineDef* Game::achEggLineUnlocked(const AchievementDef& d) const {
    if (!d.id) return nullptr;
    for (const EggLineDef* line : registry_.allEggLines())
        if (line && line->gatedBy && std::strcmp(line->gatedBy, d.id) == 0) return line;
    return nullptr;
}

bool Game::achBannerHeld() const {
    // A COLLAPSED burst is never held, however many egg lines are inside it: that banner
    // is already saying "a lot happened", and holding the home screen hostage on a
    // firmware update that retro-awarded the back catalogue is the opposite of a favour.
    if (achBannerCount_ != 1) return false;
    const AchievementDef* d = achBanner();
    return d && achEggLineUnlocked(*d) != nullptr;
}

void Game::dismissAchievementBanner() {
    if (achBannerWire_ < 0) return;
    setAchBit(achNotified_, achBannerWire_);
    achBannerWire_ = -1;
    achBannerCount_ = 0;
    markSaveDirty();
    dirty_ = true;
}

void Game::tickAchievementBanner() {
    // Retire the banner on screen, marking everything it spoke for as announced. Only
    // here — a banner that was never shown is never marked, so an unlock earned during a
    // fight waits for the player to come home rather than being spent on nobody.
    // A HELD banner has no deadline — it waits for dismissAchievementBanner() — so the
    // retire below must not see it. Everything after that point is unchanged: one at a
    // time, and only while the player is actually looking at the home screen.
    if (achBannerWire_ >= 0 && nowMs_ >= achBannerUntilMs_ && !achBannerHeld()) {
        if (achBannerCount_ > 1) {
            for (int i = 0; i < kAchievementCount; ++i)
                if (achBit(achEarned_, kAchievements[i].wire))
                    setAchBit(achNotified_, kAchievements[i].wire);
        } else {
            setAchBit(achNotified_, achBannerWire_);
        }
        achBannerWire_ = -1;
        achBannerCount_ = 0;
        markSaveDirty();
        dirty_ = true;
    }
    if (achBannerWire_ >= 0) return;                      // one at a time
    // The home screen, and only the home screen: the pet face, idle, nothing modal over
    // it. Anywhere else the player is mid-task and an interrupt would be noise.
    if (face_ != Face::Pet || nav_ != Nav::Idle) return;
    const int pending = achPendingNotify();
    if (pending == 0) return;
    // A burst too long to parade past one at a time — a firmware update retro-awarding a
    // back catalogue, most likely — collapses into a single "N ACHIEVEMENTS" banner, so
    // the home screen isn't unusable for several minutes. Under the threshold they are
    // announced properly, by name, one after another.
    if (pending > kAchBannerBurstMax) {
        achBannerWire_ = -1;
        for (int i = 0; i < kAchievementCount; ++i) {
            const AchievementDef& d = kAchievements[i];
            if (achBit(achEarned_, d.wire) && !achBit(achNotified_, d.wire)) {
                achBannerWire_ = d.wire;     // a name to show under the count
                break;
            }
        }
        achBannerCount_ = pending;
    } else {
        for (int i = 0; i < kAchievementCount; ++i) {
            const AchievementDef& d = kAchievements[i];
            if (achBit(achEarned_, d.wire) && !achBit(achNotified_, d.wire)) {
                achBannerWire_ = d.wire;
                achBannerCount_ = 1;
                break;
            }
        }
    }
    achBannerUntilMs_ = nowMs_ + kAchBannerMs;
    dirty_ = true;
}

bool Game::hatchProcessUnlocked(const CreatureDef* proc) const {
    if (!proc) return false;
    // A Process row states its own gate (CreatureDef::gatedBy); an ungated row is always
    // in its line's random hatch pool (rollHatchProcess). Phishlet is the one gated row
    // today — the deep-dive reward catch on the Phishing line — so a fresh Phishing egg
    // hatches Tadpoll until the player has dived deep, then Tadpoll-or-Phishlet at 50/50
    // (two equal-weight Process members). This mirrors eggLineUnlocked one tier down:
    // there the achievement earns a whole LINE, here the rarer species within one.
    return !proc->gatedBy || hasAchievement(proc->gatedBy);
}

bool Game::eggLineUnlocked(const EggLineDef* line) const {
    if (!line) return false;
    // A line states its own gate (EggLineDef::gatedBy); an ungated row is always on
    // line-select. Three of the four are earned today, and each asks for the kind of
    // play it is about: Phishing for the endless zone (the first DeepWeb-depth
    // milestone), Worm for REPLICATING (the archive holding two of one species at once,
    // fired from archStoreActive), Metamorphic for the MIRROR — the pet meeting its own
    // species over the LINK or in the arena. Ransomware, the default, is given.
    return !line->gatedBy || hasAchievement(line->gatedBy);
}

std::vector<const EggLineDef*> Game::availableEggLines() const {
    std::vector<const EggLineDef*> out;
    for (const EggLineDef* line : registry_.allEggLines())
        if (eggLineUnlocked(line)) out.push_back(line);
    return out;
}

}  // namespace mal
