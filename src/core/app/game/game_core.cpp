#include "core/app/game.h"

#include <algorithm>
#include <cstdio>

#include "tunables.h"
#include "core/render/canvas.h"   // kHeartbeatMs/kCombatAnimMs: the tick cadences
#include "core/render/camo.h"
#include "core/render/sprite.h"
#include "core/ui/combat_screen.h"
#include "core/ui/items_screen.h"
#include "core/ui/stat_pages.h"    // StatSources: what the STAT index opens over

namespace mal {

Game::Game(StartMode mode, const char* hatchedCreature, ISaveStore* store)
    : registry_(ContentRegistry::embedded()), store_(store) {
    // Set the ambient copies' wanders apart before anything can return early. They are
    // otherwise identical objects, and identical wanders walk identical paths, so the
    // seed is what tells three copies apart.
    for (int i = 0; i < kWormReplicaSlots; ++i) {
        companionWander_[i].seed(0x2545f491u + 0x9e3779b9u * static_cast<uint32_t>(i + 1));
    }

    inventory_ = Inventory::starting();
    pet_.mods = ModSlots::starting();
    pet_.moves = MoveLoadout::startingForLine(registry_, nullptr);  // overwritten below

    // The palette index lives outside this object (core/render/palette.h) (a drawing
    // call reads it without being handed it), so a Game sets its theme explicitly. A
    // save read below re-states it with whatever the operator chose.
    setThemePick(settings_.theme);

    // Boot from a persisted save when one exists. A valid blob
    // always restores the lifetime/economy/rack state; if it also names a live
    // active pet, we go straight to idle on it. A blob with an empty active pet
    // (vacated by Store) restores the rack but resumes at the Hatch.
    if (store_) {
        SaveData d;
        if (deserializeSave(store_->load(), d)) {
            applySave(d);
            if (pet_.creature) { nav_ = Nav::Idle; return; }
            startHatch();   // active save was vacated mid-Store; rack survives
            return;
        }
    }

    if (mode == StartMode::Hatched) {
        // Test/dev seam: skip the egg, start on a raised pet.
        // Creature is a parameter so the start pet is flexible (dev_config.h).
        installPet(registry_.creature(hatchedCreature));
        awayDigestOn_ = false;   // see enableAwayDigest
        // A raised pet's operator has found the menu; set directly rather than through
        // markTipSeen, so starting on the seam does not itself dirty the save.
        // The same for every other first-run tip: a raised pet's operator has met all
        // of them, and a seam pet that stopped to explain itself would be a different
        // pet from the one every gate written against it expects.
        for (Tip t : {Tip::MenuOpened, Tip::CareFed, Tip::CareFrag,
                      Tip::CareHappy, Tip::FirstWalk, Tip::FirstError,
                      Tip::NearTheLine, Tip::FirstCache}) {
            const int w = static_cast<int>(t);
            op_.reveals.tipsSeen[w / 8] |= static_cast<uint8_t>(1u << (w % 8));
        }
        pet_.generation = 1;       // the seam pet is the first generation
        op_.petsRaised = 1;
        pet_.moves = MoveLoadout::startingForLine(registry_, pet_.creature ? pet_.creature->line : nullptr);
        stampSlotKinds();      // this is a pet install too: lock its slots
        // startingForLine's auto-equip may not match THIS creature's stamped slot 0
        // kind: drop it if it no longer fits (e.g. a Defend-first line vs. an
        // Attack-first slotKinds row; see embedded_content).
        enforceSlotKindInvariant();
    } else {
        // Real first boot: empty save -> Decryption Hatch.
        startHatch();
    }
}

bool Game::petReplicates() const {
    if (!pet_.creature || inEggPhase()) return false;   // an egg has no copies; nor has an empty save
    const CreatureLine* line = registry_.creatureLine(pet_.creature->line);
    return line && hasLinePassive(line->passives, LinePassive::Replication);
}

int Game::idleCompanionCount() const {
    if (!petReplicates()) return 0;
    // One more copy per stage raised, which is Nodeatode's own hint text made visible:
    // a Process worm has found one friend, and by the Daemon the family fills every
    // replication slot the line ever gets. The clamp is what keeps the two agreeing if
    // a fifth stage or a fourth slot ever lands.
    const int byStage = stageIndex(pet_.creature->stage);
    return byStage < kWormReplicaSlots ? byStage : kWormReplicaSlots;
}

// --- Tick ------------------------------------------------------------------

bool Game::tick(uint32_t nowMs) {
    nowMs_ = nowMs;
    bool changed = dirty_;
    dirty_ = false;
    // All six run, in this order, every tick. None is skipped on an early answer,
    // because each step feeds the next (the model clock feeds the heartbeat, the
    // heartbeat feeds the lifecycle gates). Keep this order. Each reports
    // separately whether the screen has to be repainted.
    if (tickModelClocks(nowMs))  changed = true;
    if (tickHeartbeat(nowMs))    changed = true;
    if (tickAnimClocks(nowMs))   changed = true;
    if (tickLifecycle(nowMs))    changed = true;
    if (tickHeldGestures(nowMs)) changed = true;
    if (tickIdleDefocus(nowMs))  changed = true;
    return changed;
}

// The pet clock and the sweeps that do not answer to a beat: the achievement sweep and
// its banner queue, the duel handshake, the model decay this tick pays for, Bandwidth
// regen, the Boot-Sector incubation countdown, and the audit-capture policy windows.
// Everything here is measured in real elapsed ms against its own anchor.
bool Game::tickModelClocks(uint32_t nowMs) {
    bool changed = false;
    // Achievements: one rate-limited sweep catches every countable row without a
    // dedicated call at each of the dozens of sites that could move a counter (a Bits
    // credit, a boss round, a network resolve, a rig purchase). Then the announcement
    // queue, which only ever raises a banner while the home screen is up.
    if (lastAchSweepMs_ == 0 || nowMs_ - lastAchSweepMs_ >= kAchSweepIntervalMs) {
        lastAchSweepMs_ = nowMs_;
        sweepAchievements();
    }
    tickAchievementBanner();

    // Duel session upkeep (game_pvp.cpp): repeat whatever frame is holding the
    // handshake open, and time an unanswered one out. A no-op whenever no duel is in
    // flight, which is nearly always.
    pvpTick();

    // Decay the pet clock by the real elapsed time (event-driven, fractional
    // accumulation is carried in the model). The raising loop now owns Hunger.
    // Vitals are frozen while the pet is an egg (an egg doesn't get hungry) and
    // during the decrypt modal, but lastModelMs_ still advances so post-hatch
    // decay starts clean, and Bandwidth still regenerates (so the egg can be
    // walked to accelerate its hatch).
    if (nowMs_ > lastModelMs_) {
        const uint32_t elapsed = nowMs_ - lastModelMs_;
        if (pet_.creature && !inEggPhase())
            tickHungerAndAwardXp(elapsed);      // no pet (empty save / line-select): nothing decays
        // Bandwidth regenerates slowly over real active time, whether or not
        // EXPL is open, so a walk cannot be farmed in one sitting but recovers
        // between sessions.
        if (bandwidth_ < bandwidthMax()) {
            bandwidthAccumMs_ += elapsed;
            // Per-PET interval: the shared one, less any permanent shave this pet has
            // eaten (bandwidthRegenMinutes. Tiramisudo's upgrade).
            const uint32_t step = bandwidthRegenMinutes() * 60u * 1000u;
            while (bandwidthAccumMs_ >= step && bandwidth_ < bandwidthMax()) {
                bandwidthAccumMs_ -= step;
                // Per-TICK amount: a flat point, or the Link Aggregator's slice of
                // the spent pool (bandwidthRegenAmount). Read inside the loop because
                // the slice shrinks as the pool it is measured against fills.
                bandwidth_ += bandwidthRegenAmount();
                if (bandwidth_ > bandwidthMax()) bandwidth_ = bandwidthMax();
            }
        }
        // Boot-Sector incubation clock (egg-at-idle): counts down in real time while the
        // egg sits at idle. Frozen on every screen that is part of the hatch: the
        // decrypt runs its own countdown; the crack cinematic must be allowed to finish
        // its frames (it can legitimately be opened with seconds left, and the clock
        // running out underneath would hatch the pet mid-animation); and the Isolation
        // Protocol priced its goal off the clock as it stood when the run began, so
        // draining it underneath would quietly discount the run. Reaching 0 out here
        // auto-hatches straight to Process (no soft-lock if the player never opens any
        // of them).
        //
        // Note what inEggPhase() means for anything that shaves the clock from OUTSIDE
        // this block: it requires a remainder above zero, so an egg shaved straight to 0
        // elsewhere leaves the phase in the same instant and this will never see it.
        // Such a caller has to hatch the egg itself: Game::finishIsolation does.
        if (inEggPhase() && nav_ != Nav::Decryption &&
            nav_ != Nav::ModalHatchReveal && nav_ != Nav::Isolation &&
            nav_ != Nav::Chroma) {
            pet_.bootHatchRemainMs = pet_.bootHatchRemainMs > elapsed
                                     ? pet_.bootHatchRemainMs - elapsed : 0;
            if (pet_.bootHatchRemainMs == 0) { completeHatch(); changed = true; }
        }
    }
    lastModelMs_ = nowMs_;

    // Audit-capture policy: expire the hot-broadcast window and
    // clear the re-arm cooldown on schedule (runtime-only timing; no radio here).
    auditCapture_.tick(nowMs_);
    return changed;
}

// The shared ~4fps heartbeat, and everything paced by it: the resting wander and colour
// drift, and the per-Nav modal/process/combat timers. One beat repaints, so this always
// reports a change when it fires.
bool Game::tickHeartbeat(uint32_t nowMs) {
    if (nowMs - lastBeatMs_ < static_cast<uint32_t>(kHeartbeatMs)) return false;
    lastBeatMs_ = nowMs;
    beat_++;
    const CreatureLine* petLine = pet_.creature ? registry_.creatureLine(pet_.creature->line) : nullptr;
    const RestingTrick trick =
        petLine && !inEggPhase() ? petLine->restingTrick : RestingTrick::None;
    stepRestingMotion(trick);
    stepRestingTricks(petLine, trick);
    stepScreenBeat();
    return true;
}

// Resting motion: the pet drifts a little way around the habitat's shelf anchor, the way
// its species moves (core/model/idle_wander.h). An EGG moves the way its row says too,
// which for almost every egg is Locomotion::Static, so it does not move: it sits where it
// was laid, with its incubation readout drawn directly above it. The exception is an egg
// genuinely adrift in water, which says Swim and drifts, and the rule is the row rather
// than the stage so a hypothetical egg that flies needs no code here either. An empty
// save parks, having no pet at all. A worm's ambient copies walk the same shelf on the
// same beat, each off its own stream; the ones past the live count are parked so a
// shrinking family (an evolution, a fresh egg) leaves nothing mid-stride to walk back in.
// A pet passing itself off as a prop holds still: a prop that strolls is no disguise, and
// nor does an angler with its bait out. One skimming mail goes where the scrap it is
// after is.
void Game::stepRestingMotion(RestingTrick trick) {
    // The skim and the lure take turns: each runs only while the other rests.
    const bool skims = trick == RestingTrick::Skim;
    if (!petLure_.active())
        petFeeding_.step(skims, petWander_.offsetX(), petWander_.offsetY());
    if (!petFeeding_.active()) petLure_.step(skims);
    if (!pet_.creature) {
        petWander_.park();
    } else if (const Mail* m = petFeeding_.target()) {
        petWander_.chase(pet_.creature->locomotion, IdleFeeding::standX(*m),
                         IdleFeeding::standY(*m), IdleFeeding::faceRight(*m));
    } else if (petDisguise_.stage() == DisguiseStage::Rest && !petLure_.active()) {
        petWander_.step(pet_.creature->locomotion);
    }
    const int companions = idleCompanionCount();
    for (int i = 0; i < kWormReplicaSlots; ++i) {
        if (i < companions) companionWander_[i].step(pet_.creature->locomotion);
        else companionWander_[i].park();
    }
    petFork_.step(petReplicates());
    petRelay_.step(petFork_.stage() == ForkStage::Rest ? companions : 0);
}

// The line's resting trick, and the resting colour. A line that wears borrowed colours
// drifts into another family's palette now and then and comes back
// (core/model/idle_camo.h). An egg is left out, because the Metamorphic hatch board
// (game_chroma.cpp) is a minigame about this effect and the player should first see it
// there.
void Game::stepRestingTricks(const CreatureLine* petLine, RestingTrick trick) {
    petDisguise_.step(trick == RestingTrick::Disguise);
    petHarvest_.step(trick == RestingTrick::Harvest);
    // A shapeshifter's colour drift, vanish and mimic take turns: each runs only while
    // the others rest. The creature a mimic wears is resolved as it begins, like the
    // drift's.
    const bool shifts = trick == RestingTrick::Shapeshift;
    const bool drifting = petCamo_.level() > 0;
    if (!drifting && !petMimic_.active()) petVanish_.step(shifts);
    if (!drifting && !petVanish_.active()) petMimic_.step(shifts);
    if (!petMimic_.active()) idleMimicWorn_ = nullptr;
    else if (!idleMimicWorn_) idleMimicWorn_ = idleCamoSprite(petMimic_.slot());
    if (!petVanish_.active() && !petMimic_.active())
        petCamo_.step(petLine && petLine->wearsBorrowedColours && !inEggPhase());
    // The art is resolved once per drift, on the beat the level leaves zero: which
    // creature answers for a family is a registry walk, and the level moving is not a
    // change of who is being sampled.
    if (petCamo_.level() == 0) idleCamoWorn_ = nullptr;
    else if (!idleCamoWorn_) idleCamoWorn_ = idleCamoSprite(petCamo_.slot());
}

// The timer the screen that is up runs on the heartbeat: its row's beat column
// (game_screens.cpp), or nothing.
void Game::stepScreenBeat() {
    if (const auto beat = screenRow(nav_).beat) beat(*this);
}

// A hands-off hold on a walk event's screen: `then` once it has stood `beats` heartbeats
// unanswered. Only while explore mode is armed; any button press on those screens
// restarts it (walk_.eventBeat).
void Game::stepEventHold(int beats, void (Game::*then)()) {
    if (walk_.active && ++walk_.eventBeat >= beats) (this->*then)();
}

void Game::stepFeedingBeat() {
    if (feeding_ && ++feeding_->beat >= kFeedBeats) endFeeding();
}

// The MAINT process bar: it fills over kProcessBeats, then the process resolves once.
void Game::stepProcessBeat() {
    if (maint_ && !maint_->resolved && ++maint_->beat >= kProcessBeats) resolveMaint();
}

// The habitat's own beat: the walk's auto-step while explore mode is armed.
void Game::stepHomeBeat() {
    if (walk_.active) stepWalkPacer();
}

// Autonomous auto-battle: one turn every combatBeatsForTurn() heartbeats so each exchange
// is readable, paused while the override picker is open (the fight waits for the human).
// A same-actor streak (a lopsided speed edge, e.g. a Phishing speed siphon) shortens that
// wait each consecutive hit, so the extra turns it buys visibly speed up into a feeding
// frenzy instead of ticking at one flat cadence. The A "SKIP" fast-forward steps
// immediately, bypassing this pace. Once resolved it parks on the result beat.
void Game::stepCombatBeat() {
    if (fight_.combat.outcome() == Combat::Outcome::Ongoing) {
        if (!fight_.combat.overrideOpen() && ++fight_.turnBeat >= combatBeatsForTurn())
            advanceCombatTurn();
        return;
    }
    // The result beat's own clock, held back while a dissolve is still playing over the
    // beaten rival: the verdict banner shares that space and waits for it (drawCombat),
    // so starting the auto-dismiss underneath would leave the words a fraction of a
    // second before the screen went.
    if (!combatDissolveRunning()) ++fight_.resultBeat;
    // Hands-off auto-explore: a wild or boss fight's result is a REVEAL with no human to
    // press B, so hold it ~3s then auto-dismiss back to the habitat (finishCombat
    // advances the streak / cancels on a loss) or straight into the gauntlet's next round
    // (finishBossRound). Sim fights stay player-driven, with nothing automatic.
    if (walk_.active &&
        (fight_.caller == FightCaller::Wild || fight_.caller == FightCaller::Boss) &&
        fight_.resultBeat >= kExploreRevealHoldBeats) {
        finishCombat();
    }
}

// Explore-mode: while resting on the BARE idle habitat, the game auto-steps a guaranteed
// event ~every kWalkAutoStepBeats. No cap, hands-free. Summoning the carousel or opening
// a submenu leaves Idle, so stepping PAUSES; it resumes when the player returns here.
// doExploreStep() re-arms the pacer, and if it types a full-screen event
// (Encounter/Wifi/Shop/Combat) this stops firing until it returns. Stepping PAUSES (mode
// stays armed) once battle fatigue has fragmented the pet into the danger band: a defrag
// drops frag below the gate and stepping resumes. Manual Network Ping (A+C then A) still
// forces a step.
void Game::stepWalkPacer() {
    if (pet_.model.fragmentation() >= kBattleFatigueAutoPauseFrag) {
        std::snprintf(walk_.flavor, sizeof(walk_.flavor), "TOO FRAGMENTED - DEFRAG");
    } else if (++walk_.stepBeat >= kWalkAutoStepBeats) {
        doExploreStep();
    }
}

// The cadences faster than the heartbeat, each gated on the one screen that needs it and
// each PRIMED to `nowMs` while that screen is closed, so arriving on one never
// burst-catches-up: combat sprite motion (and the FX_CAMO it drives), the dissolve sweep,
// the Decryptogram cursor repeat, and the arcade boards that move on their own clock.
// Zero cost off those screens.
bool Game::tickAnimClocks(uint32_t nowMs) {
    // Evaluated left to right: a braced list fixes the order.
    const Cadence cadences[] = {
        tickCombatAnim(nowMs),     tickFxSweep(nowMs),       tickCryptogramRepeat(nowMs),
        tickStackerSlide(nowMs),   tickFraggerFall(nowMs),   tickIsolationCrawl(nowMs),
        tickChromaSweep(nowMs),
    };
    bool changed = false;
    // Latched into fastCadenceActive_ for Game::highFrameRate() (device CPU-clock
    // scaling). Each cadence reports its own gate, so this tracks the real fast-frame set
    // with no parallel list.
    bool fast = false;
    for (Cadence c : cadences) {
        fast = fast || c != Cadence::Closed;
        changed = changed || c == Cadence::Fired;
    }
    fastCadenceActive_ = fast;
    return changed;
}

// The Combat screen's stage clock (FightScreen::tick): sprite motion, the hit punch, a
// guardian's swarm and the camo, on kCombatAnimMs.
Game::Cadence Game::tickCombatAnim(uint32_t nowMs) {
    if (nav_ != Nav::Combat) {
        fightScreen_.closed(nowMs);
        return Cadence::Closed;
    }
    return fightScreen_.tick(*this, nowMs) ? Cadence::Fired : Cadence::Open;
}

// The dissolve clock (FX_ABSORB / FX_SHRED). Fastest tick on the device, and the only one
// gated on a specific effect being on screen: it runs for the ~2s a sweep takes and costs
// nothing the rest of the time. Three hosts, one counter, only one dissolve is ever up.
// Each resets it at its own start (startFeeding / startWifiEvent / every fight start), so
// this only has to decide when to ADVANCE.
//
// The guardian's SWARM rides the same clock, and is the one thing on it that is not a
// dissolve: a flock is redrawn every frame it is up for the reason a sweep is, and at
// 4fps a boid moving a pixel a step reads as a blink rather than as flight. It runs on
// the two screens that DRAW the body (game_render.cpp) and skips the riddle board between
// them, where only the patience bar moves and a 16fps repaint would be wasted.
Game::Cadence Game::tickFxSweep(uint32_t nowMs) {
    const bool swarmLive = nav_ == Nav::ShiboleetHail || nav_ == Nav::ShiboleetVerdict;
    const bool fxSweeping =
        nav_ == Nav::ModalFeeding || nav_ == Nav::Wifi || swarmLive ||
        (nav_ == Nav::Combat && fight_.combat.outcome() != Combat::Outcome::Ongoing);
    if (!fxSweeping) {
        lastFxMs_ = nowMs;
        return Cadence::Closed;
    }
    if (nowMs - lastFxMs_ < static_cast<uint32_t>(kFxAnimMs)) return Cadence::Open;
    lastFxMs_ = nowMs;
    fxBeat_++;
    if (swarmLive) shib_.flock.step(guardianFlockMood());
    return Cadence::Fired;
}

// THE DECRYPTOGRAM's cursor repeats while A or C is held, on its own cadence for the same
// reason the boards below have one: a board opens with thirty-odd closed cells, and
// stepping those on the shared 4fps heartbeat would take ten seconds to lap. Exactly one
// of the two held. Both held is the drop chord, which has already fired and must not move
// the cursor.
Game::Cadence Game::tickCryptogramRepeat(uint32_t nowMs) {
    if (nav_ != Nav::Cryptogram || !cryptogramRun_ || !cryptogramRun_->board.running() || aHeld_ == cHeld_ ||
        gameBriefOpen_) {
        cryptoRepeatLastMs_ = nowMs;
        return Cadence::Closed;
    }
    const uint32_t downMs = aHeld_ ? aDownMs_ : cDownMs_;
    if (nowMs - downMs < kCryptogramRepeatDelayMs ||
        nowMs - cryptoRepeatLastMs_ < kCryptogramRepeatMs)
        return Cadence::Open;
    cryptoRepeatLastMs_ = nowMs;
    if (aHeld_) cryptogramRun_->board.cycle();
    else cryptogramRun_->board.cycleBack();
    return Cadence::Fired;
}

// The Stacker's run slides on its own faster cadence for the same reason combat's sprites
// do: the shared 4fps heartbeat is legible for a progress bar and far too slow for a
// timing test.
Game::Cadence Game::tickStackerSlide(uint32_t nowMs) {
    if (nav_ != Nav::Stacker || gameBriefOpen_) {
        lastStackerStepMs_ = nowMs;
        return Cadence::Closed;
    }
    if (nowMs - lastStackerStepMs_ < static_cast<uint32_t>(playStepMs(kStackerStepMs)))
        return Cadence::Open;
    lastStackerStepMs_ = nowMs;
    stepStacker();
    return Cadence::Fired;
}

// FRAGGER's fall, likewise. The model sets its cadence from the run's score, so the game
// speeds up as it is played. The dial is not read here: FRAGGER's difficulty is its lane
// count (beginFraggerRun). A finished board holds still.
Game::Cadence Game::tickFraggerFall(uint32_t nowMs) {
    if (nav_ != Nav::Fragger || gameBriefOpen_) {
        lastFraggerStepMs_ = nowMs;
        return Cadence::Closed;
    }
    if (nowMs - lastFraggerStepMs_ < static_cast<uint32_t>(fragger_.stepMs()))
        return Cadence::Open;
    lastFraggerStepMs_ = nowMs;
    if (!fragger_.running()) return Cadence::Open;
    stepFragger();
    return Cadence::Fired;
}

// The Isolation Protocol's worm moves on its own cadence for the same reason, and more
// so: it is the one screen where standing still is never an option, so the beat it walks
// to IS the difficulty. Only while the run is live: a crashed or finished board holds
// still under the verdict.
Game::Cadence Game::tickIsolationCrawl(uint32_t nowMs) {
    if (nav_ != Nav::Isolation || gameBriefOpen_) {
        lastIsolationStepMs_ = nowMs;
        return Cadence::Closed;
    }
    if (nowMs - lastIsolationStepMs_ < static_cast<uint32_t>(playStepMs(kIsolationStepMs)))
        return Cadence::Open;
    lastIsolationStepMs_ = nowMs;
    if (isolation_.running()) {   // a parked board must not replay its verdict
        const int ate = isolation_.dots();
        isolation_.step();
        if (isolation_.clean()) playSound(Sound::Clear);
        else if (!isolation_.running()) playSound(Sound::Crash);
        else if (isolation_.dots() > ate) playSound(Sound::Point);
    }
    return Cadence::Fired;
}

// The CHROMATOPHORE's sweep runs on real time because it measures time: how much of the
// window a repaint had left to finish in. It ticks at the dissolve cadence (kFxAnimMs)
// for the reason a dissolve does: the scatter crossing the creature is the readout, and
// at the 4fps heartbeat a kChromaSettleMs change would land on four frames total. Deltas,
// not a fixed step, so the model always sees the time that actually passed.
Game::Cadence Game::tickChromaSweep(uint32_t nowMs) {
    if (nav_ != Nav::Chroma || gameBriefOpen_ || !chroma_.running()) {
        lastChromaMs_ = nowMs;
        return Cadence::Closed;
    }
    if (nowMs - lastChromaMs_ < static_cast<uint32_t>(kFxAnimMs)) return Cadence::Open;
    const int passed = chroma_.passes();
    chroma_.tick(nowMs - lastChromaMs_);
    if (chroma_.clean()) playSound(Sound::Clear);
    else if (!chroma_.running()) playSound(Sound::Crash);
    else if (chroma_.passes() > passed) playSound(Sound::Point);
    lastChromaMs_ = nowMs;
    return Cadence::Fired;
}

// The deadlines that end things: the post-encounter readout timing out, the Critical
// System Failure window, the evolution boundary, the Lockout crisis firing and expiring,
// and the autosave. Ordered by modal priority (CSF > Lockout > Evolution), and each
// gate is written so the one above it wins outright.
bool Game::tickLifecycle(uint32_t nowMs) {
    bool changed = false;
    // Post-encounter status readout: a real-ms auto-dismiss window, like
    // kSdIconRevealMs. Informational only, so it times out on its own rather
    // than parking forever like a modal.
    if (nav_ == Nav::PostEncounter && nowMs_ >= fight_.result.readoutUntilMs) {
        dismissPostEncounter();
        changed = true;
    }

    // A STORY panel turning itself over (kStoryPanelMs, the same real-ms shape as the
    // readout above). It is what lets a chapter fire on a HANDS-OFF walk: auto-progress
    // runs the ladder with nobody watching, so a reader that waited for a thumb would
    // stop an unattended device dead at the first area it reached. Every press re-arms
    // the clock (StoryRun::armPanel), so it only ever overtakes a page nobody is
    // reading, and the countdown rule above the hint band is what says so before it
    // happens.
    if (nav_ == Nav::Story && story_ && nowMs_ >= story_->deadlineMs) {
        turnStoryPage();
        changed = true;
    }

    // Critical System Failure: the only death path, and the highest modal priority (CSF > Lockout >
    // Evolution). The 5/5 dying state is recoverable by dropping below 5 within the recovery
    // window, which a Yubi-Cookie does (RemoveCareMistakeOnce), and so does a Daemon sold for a
    // HOTFIX (game_arch_sale.cpp); the Backup Drive is a combat shield and touches no error, and
    // once the window expires the pet is permanently lost. The habitat counts the window down
    // (game_render.cpp's FAILING banner). Never during the Hatch (no pet) or once already in the
    // modal. The window accumulates and the total persists (save v42), so a power cycle cannot
    // refund seconds spent at 5/5. It is the only window that survives a reboot, because it ends in
    // permanent loss and the others end in penalties. What it counts is time AWAKE at 5/5: with no
    // RTC (config.h HAS_HARDWARE_RTC) the device cannot see how long it was off, so being switched
    // off neither kills the pet nor buys it a reprieve.
    if (pet_.creature && nav_ != Nav::ModalCSF) {
        if (pet_.model.careBranch() == CareBranch::Dying) {
            if (!pet_.dyingArmed) {
                pet_.dyingArmed = true;
                pet_.dyingEnteredMs = nowMs_;
                pet_.failingAlertAtMs = pet_.dyingElapsedMs;
                markSaveDirty();   // the moment it starts is worth writing at once
            }
            pet_.dyingElapsedMs += nowMs_ - pet_.dyingEnteredMs;
            pet_.dyingEnteredMs = nowMs_;
            if (pet_.dyingElapsedMs >= pet_.failingAlertAtMs && pet_.dyingElapsedMs < kCsfDyingGraceMs) {
                playSound(Sound::Failing);
                pet_.failingAlertAtMs = pet_.dyingElapsedMs + kFailingAlertEveryMs;
            }
            // Between the transitions the periodic autosave (kSaveAutosaveMs) keeps the
            // written figure close enough: it is a sixtieth of the window, so a reboot
            // refunds at most that much. Marking the save dirty every tick instead would
            // write flash every kSaveDebounceMs for the whole half hour.
            if (pet_.dyingElapsedMs >= kCsfDyingGraceMs) {
                fireCSF();
                changed = true;
            }
        } else if (pet_.dyingArmed || pet_.dyingElapsedMs) {
            // Recovered below 5/5: disarm, and forget what was burned. A pet pulled
            // back from the edge gets the full window again next time.
            pet_.dyingArmed = false;
            pet_.dyingElapsedMs = 0;
            markSaveDirty();       // ...and so is the rescue, which zeroes the window
        }
    }

    // Evolution boundary. Fires from rest once the time-in-stage + care gate
    // is met; preempts like the other Area-5 modals (below CSF/Lockout in priority).
    if (nav_ == Nav::Idle && evolveEligible()) {
        fireEvolution();
        changed = true;
    }

    // An incubating egg, back at rest after its hatch game, with a Boot Accelerator in
    // the bag: offer it now, once per egg (game_offer.cpp). This is the only stretch of
    // a pet's life the item does anything, so it is asked here rather than left for the
    // player to go and find in ITEMS.
    if (nav_ == Nav::Idle && inEggPhase() && !pet_.eggOfferMade) {
        openItemOffer(OfferFor::Egg);
        changed = true;
    }

    // Lockout crisis fires when Hunger bottoms out. Never during the Hatch (there's
    // no pet), an Evolution, or a Critical System Failure (priority), nor over a
    // DECRYPTOGRAM board: the board cannot be left and its ticket is already spent,
    // so firing across it would destroy a found item. The crisis loses nothing by
    // waiting: its deadline starts when it fires, and this check runs again the
    // moment the board is left.
    if (pet_.creature && !pet_.armed.lockoutActive && nav_ != Nav::Decryption && nav_ != Nav::Cryptogram &&
        nav_ != Nav::ModalEvolve && nav_ != Nav::ModalCSF && pet_.model.isStarving()) {
        fireLockout();
        changed = true;
    }
    // Expiry is global while the crisis is live: even if the player is in the
    // ITEMS-in-Lockout list, the deadline resolves the modal.
    if (pet_.armed.lockoutActive && nowMs_ >= pet_.armed.lockoutDeadlineMs) {
        expireLockout();
        changed = true;
    }
    // No reminder while ITEMS is open from the modal: the owner is already answering.
    if (pet_.armed.lockoutActive && !pet_.armed.lockoutReminded && !lockoutItemsContext_ &&
        pet_.armed.lockoutDeadlineMs - nowMs_ <= kLockoutReminderMs) {
        pet_.armed.lockoutReminded = true;
        playSound(Sound::Lockout);
    }

    // Autosave. Persist a debounced write after a meaningful
    // change, plus a slower periodic write so passive decay is captured. Only
    // once there's something worth saving (a raised pet or a populated rack): a
    // bare first-boot Hatch leaves the save empty, so the next boot re-hatches.
    if (store_ && (pet_.creature || !op_.arch.rack.empty())) {
        const bool debounced = saveDirty_ && nowMs_ - lastSaveMs_ >= kSaveDebounceMs;
        const bool periodic = nowMs_ - lastSaveMs_ >= kSaveAutosaveMs;
        if (debounced || periodic) persistSave();
    }
    return changed;
}

// Every held-button gesture, resolved on the dwell time: A's list repeat and C's step-back
// (tickListNav, game_listnav.cpp), and B's second actions (tickHoldB, game_holds.cpp).
bool Game::tickHeldGestures(uint32_t /*nowMs*/) {
    bool changed = false;
    if (tickListNav()) changed = true;
    if (tickHoldB()) changed = true;
    return changed;
}

// The global idle timer collapsing the menu tree back to the habitat. Last, because what
// counts as activity is re-stamped by several of the blocks above.
bool Game::tickIdleDefocus(uint32_t nowMs) {
    bool changed = false;
    // The global idle timer collapses the menu tree: suspended inside modals,
    // a running process / an active Lockout, the CFG screens and the played MAINT
    // boards, and while an ITEMS-filter / VAULT-bulk hold gesture is
    // mid-flight (no new presses during the hold). UPDATES is carved back out of
    // the CFG exemption: it already gets the long hands-off radio budget below
    // (it holds a live association) but must still eventually collapse an
    // abandoned check, or a forgotten screen pins the radio open forever.
    const bool inCfgScreen = nav_ == Nav::Detail && enteredId() == SubmenuId::Cfg &&
                              !updateScreenOpen();
    const bool suspended = pet_.armed.lockoutActive || nav_ == Nav::ModalFeeding ||
                           nav_ == Nav::Process || nav_ == Nav::Decryption ||
                           nav_ == Nav::ModalLineSelect || nav_ == Nav::ModalEggPick ||
                           nav_ == Nav::ModalHatchReveal || nav_ == Nav::Isolation ||
                           nav_ == Nav::Chroma || nav_ == Nav::Cryptogram ||
                           nav_ == Nav::ModalEvolve || nav_ == Nav::ModalCSF ||
                           nav_ == Nav::Combat || nav_ == Nav::ExploreControl ||
                           nav_ == Nav::Encounter || nav_ == Nav::Wifi ||
                           nav_ == Nav::ShiboleetHail ||
                           nav_ == Nav::Shiboleet ||
                           nav_ == Nav::ShiboleetVerdict ||
                           nav_ == Nav::Shop || nav_ == Nav::ModShop ||
                           nav_ == Nav::CacheYield ||
                           nav_ == Nav::BulkYield || nav_ == Nav::PostEncounter ||
                           nav_ == Nav::Stacker || nav_ == Nav::Fragger ||
                           nav_ == Nav::ArcadeResult ||
                           // ROCK THE DOCK's bracket is a READING screen: eight entrants,
                           // their levels, and the next opponent's species and Exploit
                           // are what a loadout is chosen against, and five seconds is
                           // shorter than reading them. Collapsing it would also drop
                           // the operator onto the habitat mid-decision with a run
                           // still in play, which reads as the screen crashing.
                           nav_ == Nav::Tourney ||
                           // A STORY chapter and the CHAPTERS list are both READING
                           // screens, for the reason the bracket above is: fifteen
                           // seconds is shorter than a page of prose, and a reader
                           // dropped onto the habitat mid-sentence reads as a crash.
                           // The chapter has a deadline of its own that ends it
                           // properly (kStoryPanelMs), and the archive is a menu the
                           // operator opened on purpose.
                           nav_ == Nav::Story || nav_ == Nav::StoryArchive ||
                           inCfgScreen ||
                           // A held B is one of the four hold gestures mid-flight and
                           // must not be collapsed under. A held A is only the list
                           // repeat, which is not in the same position: every step it
                           // fires re-stamps lastInputMs_ (Game::tickListNav), so a
                           // genuinely held A keeps the tree open by USING it. Listing
                           // it here too would let one lost release edge (a bounce, a
                           // screen change under the thumb) hold the menu tree open
                           // until the next press.
                           bHeld_ ||
                           qrScreenActive() ||  // scanning a QR takes longer than 5s
                           tagEditorActive() || // ...so does composing a tag
                           // Travel sleep is on its way down: the notice frame has to
                           // stay put until the device tier powers the panel off, or
                           // the last thing the operator sees is the idle screen and
                           // not the gesture that brings the device back.
                           travelSleepRequested_;

    // A radio screen (CREW's picker, PEERS) keeps the menu tree open far longer
    // (kRadioScreenDefocusMs): both are hands-off screens you walk around with, and
    // an open one arms the radio (radioScanWanted / linkWanted), so the standard 5s
    // would power it down before a sweep or a peer's beacon lands. The screen would
    // starve itself. UPDATES takes the same budget for the matching reason: it is a
    // screen you wait in front of, and five seconds is shorter than an association
    // plus a fetch, let alone reading the verdict one leaves behind and deciding
    // whether to install it.
    const uint32_t defocusMs =
        (radioScreenOpen() || updateScreenOpen()) ? kRadioScreenDefocusMs : kAutoDefocusMs;
    if (!suspended && nav_ != Nav::Idle && nowMs - lastInputMs_ >= defocusMs) {
        dropCursor();
        changed = true;
    }
    return changed;
}


void Game::tickHungerAndAwardXp(uint32_t elapsedMs) {
    // Passive XP Farming (j/k, Rig Shop): count hunger points lost to decay
    // while Hunger stayed above hungerXpThreshold() and pay hungerXpRateLevel()
    // XP per point. Only the portion of this decay step that fell above the
    // threshold counts: e.g. threshold 90, hunger 92 -> 88 pays for 2 points
    // (92..91), not all 4.
    const int hungerBefore = pet_.model.hunger();
    pet_.model.tick(elapsedMs);
    // A Hunger point lost to decay also ends any stacking-food run
    // (ItemEffect::Kind::HungerStacking): the pet has moved on, so the next
    // Polltatoes starts counting from one again.
    if (pet_.model.hunger() < hungerBefore) pet_.armed.stackingFoodRun = 0;
    const int rate = hungerXpRateLevel();
    if (rate <= 0) return;
    const int pointsAboveThreshold =
        hungerBefore - std::max(pet_.model.hunger(), hungerXpThreshold());
    if (pointsAboveThreshold > 0) addCombatXp(pointsAboveThreshold * rate);
}

} // namespace mal
