#include "core/app/game.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/render/canvas.h"   // kHeartbeatMs/kCombatAnimMs — the tick cadences
#include "core/render/camo.h"
#include "core/render/sprite.h"
#include "core/ui/combat_screen.h"
#include "core/ui/items_screen.h"

namespace mal {

Game::Game(StartMode mode, const char* hatchedCreature, ISaveStore* store)
    : registry_(ContentRegistry::embedded()), bits_(kStartBits), store_(store) {
    // Set the ambient copies' wanders apart before anything can return early. They are
    // otherwise identical objects, and identical wanders walk identical paths — the
    // seed is the only thing that makes three copies read as three creatures.
    for (int i = 0; i < kWormReplicaSlots; ++i) {
        companionWander_[i].seed(0x2545f491u + 0x9e3779b9u * static_cast<uint32_t>(i + 1));
    }

    inventory_ = Inventory::starting();
    loadout_ = Loadout::starting();
    moveLoadout_ = MoveLoadout::startingForLine(registry_, nullptr);  // overwritten below

    // Boot from a persisted save when one exists. A valid blob
    // always restores the lifetime/economy/rack state; if it also names a live
    // active pet, we go straight to idle on it. A blob with an empty active pet
    // (vacated by Store) restores the rack but resumes at the Hatch.
    if (store_) {
        SaveData d;
        if (deserializeSave(store_->load(), d)) {
            applySave(d);
            if (pet_) { nav_ = Nav::Idle; return; }
            startHatch();   // active save was vacated mid-Store; rack survives
            return;
        }
    }

    if (mode == StartMode::Hatched) {
        // Test/dev seam: skip the egg, start on a raised pet.
        // Creature is a parameter so the start pet is flexible (dev_config.h).
        installPet(registry_.creature(hatchedCreature));
        generation_ = 1;       // the seam pet is the first generation
        petsRaised_ = 1;
        moveLoadout_ = MoveLoadout::startingForLine(registry_, pet_ ? pet_->line : nullptr);
        stampSlotKinds();      // this is a pet install too — lock its slots
        // startingForLine's auto-equip may not match THIS creature's stamped slot 0
        // kind — drop it if it no longer fits (e.g. a Defend-first line vs. an
        // Attack-first slotKinds row; see embedded_content).
        enforceSlotKindInvariant();
    } else {
        // Real first boot: empty save -> Decryption Hatch.
        startHatch();
    }
}

int Game::idleCompanionCount() const {
    if (!pet_ || inEggPhase()) return 0;   // an egg has no copies; nor has an empty save
    if (!pet_->line || std::strcmp(pet_->line, "worm") != 0) return 0;
    // One more copy per stage raised, which is Nodeatode's own hint text made visible:
    // a Process worm has found one friend, and by the Daemon the family fills every
    // replication slot the line ever gets. The clamp is what keeps the two agreeing if
    // a fifth stage or a fourth slot ever lands.
    const int byStage = stageIndex(pet_->stage);
    return byStage < kWormReplicaSlots ? byStage : kWormReplicaSlots;
}

// --- Tick ------------------------------------------------------------------

bool Game::tick(uint32_t nowMs) {
    nowMs_ = nowMs;
    bool changed = dirty_;
    dirty_ = false;
    // All six run, in this order, every tick. None is skipped on an early answer —
    // what one does is routinely what the next reads (the model clock feeds the
    // heartbeat, the heartbeat feeds the lifecycle gates), so the order IS the
    // contract. Each reports separately whether the screen has to be repainted.
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
    // Vitals are FROZEN while the pet is still an egg (Boot-Sector incubation, 04
    // redesign — an egg doesn't get hungry) and during the decrypt modal; but
    // lastModelMs_ still advances so post-hatch decay starts clean, and Bandwidth
    // still regenerates (so the egg can be walked to accelerate its hatch).
    if (nowMs_ > lastModelMs_) {
        const uint32_t elapsed = nowMs_ - lastModelMs_;
        if (pet_ && !inEggPhase())
            tickHungerAndAwardXp(elapsed);      // no pet (empty save / line-select): nothing decays
        // Bandwidth regenerates over real active time — a slow trickle
        // back to the cap so a walk can't be farmed in one sitting but recovers
        // on its own between sessions, independent of whether EXPL is open.
        if (bandwidth_ < bandwidthMax()) {
            bandwidthAccumMs_ += elapsed;
            // Per-PET interval: the shared one, less any permanent shave this pet has
            // eaten (bandwidthRegenMinutes — Tiramisudo's upgrade).
            const uint32_t step = bandwidthRegenMinutes() * 60u * 1000u;
            while (bandwidthAccumMs_ >= step && bandwidth_ < bandwidthMax()) {
                bandwidthAccumMs_ -= step;
                ++bandwidth_;
            }
        }
        // Boot-Sector incubation clock (egg-at-idle): counts down in real time while
        // the egg sits at idle. Frozen inside every screen that is itself about the
        // hatch — the decrypt runs its own countdown; the crack cinematic must be
        // allowed to finish its frames (it can legitimately be opened with seconds
        // left, and the clock running out underneath would hatch the pet
        // mid-animation); and the Isolation Protocol priced its goal off the clock as
        // it stood when the run began, so draining it underneath would quietly discount
        // the run. Reaching 0 out here auto-hatches straight to Process (no soft-lock if
        // the player never opens any of them).
        //
        // Note what inEggPhase() means for anything that shaves the clock from OUTSIDE
        // this block: it requires a remainder above zero, so an egg shaved straight to 0
        // elsewhere leaves the phase in the same instant and this will never see it.
        // Such a caller has to hatch the egg itself — Game::finishIsolation does.
        if (inEggPhase() && nav_ != Nav::Decryption &&
            nav_ != Nav::ModalHatchReveal && nav_ != Nav::Isolation &&
            nav_ != Nav::Chroma) {
            bootHatchRemainMs_ = bootHatchRemainMs_ > elapsed
                                     ? bootHatchRemainMs_ - elapsed : 0;
            if (bootHatchRemainMs_ == 0) { completeHatch(); changed = true; }
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
    bool changed = false;
    if (nowMs - lastBeatMs_ >= static_cast<uint32_t>(kHeartbeatMs)) {
        lastBeatMs_ = nowMs;
        beat_++;
        changed = true;
        // Resting motion: the pet drifts a little way around the habitat's shelf
        // anchor, the way its species moves (core/model/idle_wander.h). An EGG moves
        // the way its row says too, which for almost every egg is Locomotion::Static
        // and therefore not at all — it sits where it was laid, with its incubation
        // readout drawn directly above it. The exception is an egg genuinely adrift in
        // water, which says Swim and drifts, and the rule is the row rather than the
        // stage so a hypothetical egg that flies needs no code here either. An empty
        // save parks, having no pet at all.
        // A worm's ambient copies walk the same shelf on the same beat, each off its
        // own stream; the ones past the live count are parked so a shrinking family
        // (an evolution, a fresh egg) leaves nothing mid-stride to walk back in.
        if (pet_) petWander_.step(pet_->locomotion);
        else petWander_.park();
        const int companions = idleCompanionCount();
        for (int i = 0; i < kWormReplicaSlots; ++i) {
            if (i < companions) companionWander_[i].step(pet_->locomotion);
            else companionWander_[i].park();
        }
        // Resting colour: a line that wears borrowed colours drifts into another
        // family's palette now and then and comes back (core/model/idle_camo.h). An EGG
        // is left out — the Metamorphic hatch board is a whole minigame about exactly
        // this (game_chroma.cpp), and a shell rehearsing it in the background while the
        // board is what the player is about to be scored on says the trick twice.
        const CreatureLine* petLine =
            pet_ ? registry_.creatureLine(pet_->line) : nullptr;
        petCamo_.step(petLine && petLine->wearsBorrowedColours && !inEggPhase());
        // The art is resolved once per drift, on the beat the level leaves zero: which
        // creature answers for a family is a registry walk, and the level moving is not
        // a change of who is being sampled.
        if (petCamo_.level() == 0) idleCamoWorn_ = nullptr;
        else if (!idleCamoWorn_) idleCamoWorn_ = idleCamoSprite(petCamo_.slot());
        // Modal / process timers run on the heartbeat (their own lifecycle).
        if (nav_ == Nav::ModalFeeding) {
            if (++feedBeat_ >= kFeedBeats) endFeeding();
        } else if (nav_ == Nav::Process && !processResolved_) {
            if (++processBeat_ >= kProcessBeats) resolveMaint();
        } else if (nav_ == Nav::ModalEvolve) {
            // Cinematic advances on the heartbeat: hold -> flash -> reveal, then
            // it parks at the reveal and waits for B (completeEvolution).
            if (evolveBeat_ < kEvoHoldBeats + kEvoFlashBeats) evolveBeat_++;
        } else if (nav_ == Nav::ModalHatchReveal) {
            // Non-interactive: the shell's one-shot walks a frame per heartbeat, holds
            // on the last one, then hatches itself. Nothing to press.
            const SpriteData* egg = hatchEggSprite();
            const int last = (egg ? egg->frames : 1) - 1;
            if (++hatchRevealBeat_ >= last + kHatchRevealHoldBeats) completeHatch();
        } else if (nav_ == Nav::ModalCSF) {
            // The crash FX holds a few beats, then parks and waits for B.
            if (csfBeat_ < kCsfHoldBeats) csfBeat_++;
        } else if (nav_ == Nav::Combat) {
            // Autonomous auto-battle: one turn every combatBeatsForTurn()
            // heartbeats so each exchange is readable, paused while the override
            // picker is open (the fight waits for the human). A same-actor streak
            // (a lopsided speed edge, e.g. a Phishing speed siphon) shortens that
            // wait each consecutive hit, so the extra turns it buys visibly speed
            // up into a feeding frenzy instead of ticking at one flat cadence. The
            // A "SKIP" fast-forward steps immediately, bypassing this pace. Once
            // resolved it parks on the result beat.
            if (combat_.outcome() == Combat::Outcome::Ongoing &&
                !combat_.overrideOpen()) {
                if (++combatTurnBeat_ >= combatBeatsForTurn()) advanceCombatTurn();
            } else if (combat_.outcome() != Combat::Outcome::Ongoing) {
                // The result beat's own clock, held back while a dissolve is still
                // playing over the beaten rival: the verdict banner shares that space
                // and waits for it (drawCombat), so starting the auto-dismiss underneath
                // would leave the words a fraction of a second before the screen went.
                if (!combatDissolveRunning()) ++combatBeat_;
                // Hands-off auto-explore: a wild or boss fight's result is a REVEAL
                // with no human to press B, so hold it ~3s then auto-dismiss back to
                // the habitat (finishCombat advances the streak / cancels on a loss)
                // or straight into the gauntlet's next round (finishBossRound). Sim
                // fights stay player-driven — there's no auto-anything to keep moving.
                if (exploreActive_ &&
                    (combatCaller_ == CombatCaller::Wild ||
                     combatCaller_ == CombatCaller::Boss) &&
                    combatBeat_ >= kExploreRevealHoldBeats) {
                    finishCombat();
                }
            }
        } else if (nav_ == Nav::Wifi && exploreActive_) {
            // Hands-off REVEAL: the Wi-Fi outcome is a rolled reveal, not a
            // choice — hold ~3s so it's readable, then auto-play it (an awakened
            // guardian enters a fight, which then auto-dismisses; the rest resolve to
            // the habitat). Any button press restarts the hold (onWifi resets it).
            if (++exploreEventBeat_ >= kExploreRevealHoldBeats) resolveWifiOutcome();
        } else if ((nav_ == Nav::Shop || nav_ == Nav::ModShop) && exploreActive_) {
            // Hands-off DECISION: a shop (item OR mod) is a real buy/leave choice, so
            // hold ~10s to let a watching player act; if no button is pressed by then,
            // auto-leave and keep exploring. Any press restarts the hold (onShop
            // resets it), so an engaged player is never rushed.
            if (++exploreEventBeat_ >= kExploreDecisionHoldBeats) {
                std::snprintf(exploreFlavor_, sizeof(exploreFlavor_), "LEFT THE SHOP");
                returnToExplore();
            }
        } else if (nav_ == Nav::Idle && exploreActive_) {
            // Explore-mode: while resting on the BARE idle habitat,
            // the game auto-steps a guaranteed event ~every kWalkAutoStepBeats — no
            // cap, hands-free. Summoning the carousel or opening a submenu leaves
            // Idle, so stepping PAUSES; it resumes when the player returns here.
            // doExploreStep() re-arms the pacer, and if it types a full-screen event
            // (Encounter/Wifi/Shop/Combat) this branch stops firing until it returns.
            // PAUSE stepping (mode stays armed) once battle fatigue has
            // fragmented the pet into the danger band — a defrag drops frag below the
            // gate and stepping resumes. Manual Network Ping (A+C→A) still forces a step.
            if (model_.fragmentation() >= kBattleFatigueAutoPauseFrag) {
                std::snprintf(exploreFlavor_, sizeof(exploreFlavor_),
                              "TOO FRAGMENTED - DEFRAG");
            } else if (++exploreStepBeat_ >= kWalkAutoStepBeats) {
                doExploreStep();
            }
        }
    }
    return changed;
}

// The four cadences faster than the heartbeat, each gated on the one screen that needs
// it and each PRIMED to `nowMs` while that screen is closed, so arriving on one never
// burst-catches-up: combat sprite motion (and the FX_CAMO it drives), the dissolve
// sweep, the Decryptogram cursor repeat, and the three arcade boards that move on their
// own clock. Zero cost off those screens.
bool Game::tickAnimClocks(uint32_t nowMs) {
    bool changed = false;
    // Combat sprite motion ticks on its own faster cadence (kCombatAnimMs) instead of
    // the shared heartbeat above, so a feeding-frenzy streak's accelerated turn pacing
    // still shows real sprite motion between repaints rather than a stuck pose. Only
    // runs while a fight is actually on screen — zero cost otherwise.
    if (nav_ == Nav::Combat) {
        if (nowMs - lastCombatAnimMs_ >= static_cast<uint32_t>(kCombatAnimMs)) {
            lastCombatAnimMs_ = nowMs;
            combatAnimBeat_++;
            combatHitBeat_++;
            // FX_CAMO. Whose colours the pet is in is read fresh every tick rather than
            // latched at the swing: a borrowed move is still the pet's last cast while
            // the rival takes its turn, so the colours hold until the pet itself casts
            // something else. A beat driving this instead would hand the change to
            // whichever fighter swung last, and a counter-attack would strip it.
            //
            // Both seats are passed, because the question is about the PAIR: which source
            // a cast names depends on what the fighter opposite is carrying (camoTarget).
            // Only the TARGET is resolved here — turning it into tones means ranking a
            // sprite's colours, which the draw already does once per repaint.
            const bool flip = combatLocalIsEnemySide();
            const Combatant& self = flip ? combat_.enemy() : combat_.player();
            const Combatant& rival = flip ? combat_.player() : combat_.enemy();
            const SpriteData* wear =
                camoSpriteForTarget(camoTarget(self, rival), rival.spriteName,
                                    self.stage);
            // Trading one borrowed palette for ANOTHER restarts the scatter with the old
            // one held behind it (drawCombatScreen passes it as camo.h's `from`), so the
            // change reads as one disguise dissolving into the next. Gated on the ART and
            // not on the cast: a run of casts that keep naming the same creature — the
            // fighter opposite, or the line it belongs to — is one unbroken disguise, and
            // re-running the dissolve into the colour already being worn would flicker the
            // pet for no change at all. Only while it is actually wearing something, too:
            // from bare there is nothing to dissolve out of and the level simply rises.
            if (wear && wear != combatCamoWorn_ && combatCamoLevel_ > 0) {
                combatCamoLeaving_ = combatCamoWorn_;
                combatCamoLevel_ = 0;
            }
            if (wear) combatCamoWorn_ = wear;
            combatCamoLevel_ = camoAdvance(combatCamoLevel_, wear != nullptr);
            // The palette being left is only true mid-dissolve. Once the front is past
            // every pixel, or the pet is returning to its own colours, what the unflipped
            // pixels wear is the creature itself.
            if (!wear || combatCamoLevel_ == 255) combatCamoLeaving_ = nullptr;
            changed = true;
        }
    } else {
        lastCombatAnimMs_ = nowMs;   // stay primed so re-entering combat doesn't burst-catch-up
        // Off the fight, off the disguise — every fight opens bare.
        combatCamoLevel_ = 0;
        combatCamoWorn_ = nullptr;
        combatCamoLeaving_ = nullptr;
    }

    // The dissolve clock (FX_ABSORB / FX_SHRED). Fastest tick on the device, and the
    // only one gated on a specific effect being on screen: it runs for the ~2s a sweep
    // takes and costs nothing the rest of the time. Three hosts, one counter — only one
    // dissolve is ever up. Each reset its own start (startFeeding / startWifiEvent /
    // the fight's own combatBeat_ reset), so this only has to decide when to ADVANCE.
    const bool fxSweeping =
        nav_ == Nav::ModalFeeding || nav_ == Nav::Wifi ||
        (nav_ == Nav::Combat && combat_.outcome() != Combat::Outcome::Ongoing);
    if (fxSweeping) {
        if (nowMs - lastFxMs_ >= static_cast<uint32_t>(kFxAnimMs)) {
            lastFxMs_ = nowMs;
            fxBeat_++;
            changed = true;
        }
    } else {
        lastFxMs_ = nowMs;   // primed, so opening a sweep doesn't burst-catch-up
    }

    // THE DECRYPTOGRAM's cursor repeats while A or C is held, on its own cadence for the
    // same reason the two below have one: a board opens with thirty-odd closed cells, and
    // stepping those on the shared 4fps heartbeat would take ten seconds to lap. EXACTLY
    // one of the two held — both is the drop chord, which has already fired and must not
    // also scrub the cursor out from under it.
    if (nav_ == Nav::Cryptogram && cryptogram_.running() && (aHeld_ != cHeld_) &&
        !gameBriefOpen_) {
        const uint32_t downMs = aHeld_ ? aDownMs_ : cDownMs_;
        if (nowMs - downMs >= kCryptogramRepeatDelayMs &&
            nowMs - cryptoRepeatLastMs_ >= kCryptogramRepeatMs) {
            cryptoRepeatLastMs_ = nowMs;
            if (aHeld_) cryptogram_.cycle();
            else cryptogram_.cycleBack();
            changed = true;
        }
    } else {
        cryptoRepeatLastMs_ = nowMs;   // primed, so arriving on the board can't burst
    }

    // The Stacker's run slides on its own faster cadence for the same reason combat's
    // sprites do: the shared 4fps heartbeat is legible for a progress bar and far too
    // slow for a timing test. Only runs while the board is up, and only while the run is
    // still live — a finished board holds still.
    if (nav_ == Nav::Stacker && !gameBriefOpen_) {
        if (nowMs - lastStackerStepMs_ >=
            static_cast<uint32_t>(arcadeStepMs(kStackerStepMs))) {
            lastStackerStepMs_ = nowMs;
            stacker_.step();
            changed = true;
        }
    } else {
        lastStackerStepMs_ = nowMs;   // primed, so entering the game doesn't burst-catch-up
    }

    // The Isolation Protocol's worm moves on its own cadence for the same reason, and
    // more so: it is the one screen where standing still is never an option, so the beat
    // it walks to IS the difficulty. Only while the run is live — a crashed or finished
    // board holds still under the verdict.
    if (nav_ == Nav::Isolation && !gameBriefOpen_) {
        if (nowMs - lastIsolationStepMs_ >=
            static_cast<uint32_t>(arcadeStepMs(kIsolationStepMs))) {
            lastIsolationStepMs_ = nowMs;
            isolation_.step();
            changed = true;
        }
    } else {
        lastIsolationStepMs_ = nowMs;
    }

    // The CHROMATOPHORE's sweep runs on real time rather than on a step, because what
    // it is measuring IS time — how much of the window a repaint had left to finish in.
    // It ticks at the dissolve cadence (kFxAnimMs) for the reason a dissolve does: the
    // scatter crossing the creature is the readout, and at the 4fps heartbeat a
    // kChromaSettleMs change would land on four frames total. Deltas, not a fixed step,
    // so the model always sees the time that actually passed.
    if (nav_ == Nav::Chroma && !gameBriefOpen_ && chroma_.running()) {
        if (nowMs - lastChromaMs_ >= static_cast<uint32_t>(kFxAnimMs)) {
            chroma_.tick(nowMs - lastChromaMs_);
            lastChromaMs_ = nowMs;
            changed = true;
        }
    } else {
        lastChromaMs_ = nowMs;
    }
    return changed;
}

// The deadlines that end things: the post-encounter readout timing out, the Critical
// System Failure window, the evolution boundary, the Lockout crisis firing and expiring,
// and the autosave. Ordered by modal priority — CSF > Lockout > Evolution — and each
// gate is written so the one above it wins outright.
bool Game::tickLifecycle(uint32_t nowMs) {
    bool changed = false;
    // Post-encounter status readout: a real-ms auto-dismiss window
    // (the kSdIconRevealMs pattern, not a beat count) — informational only, so it
    // times out on its own rather than parking forever like a modal.
    if (nav_ == Nav::PostEncounter && nowMs_ >= postEncounterDeadlineMs_) {
        dismissPostEncounter();
        changed = true;
    }

    // Critical System Failure — the ONLY death path, and the HIGHEST modal
    // priority (CSF > Lockout > Evolution). The 5/5 dying state is
    // recoverable by dropping below 5 (Backup Drive / Yubi-Cookie) within the
    // ageing/recovery window; once the window expires the pet is permanently
    // lost. Never during the Hatch (no pet) or once already in the modal.
    // The window ACCUMULATES rather than anchoring, and the total persists (save v42),
    // so a power cycle cannot refund seconds already spent at 5/5 — this is the one
    // window that survives a reboot, because it is the last step before permanent loss
    // and the rest are penalties. What it counts is time AWAKE at 5/5: with no RTC
    // (config.h HAS_HARDWARE_RTC) the device cannot see how long it was off, so being
    // switched off neither kills the pet nor buys it a reprieve.
    if (pet_ && nav_ != Nav::ModalCSF) {
        if (model_.careBranch() == CareBranch::Dying) {
            if (!dyingArmed_) { dyingArmed_ = true; dyingEnteredMs_ = nowMs_; }
            dyingElapsedMs_ += nowMs_ - dyingEnteredMs_;
            dyingEnteredMs_ = nowMs_;
            // Keep the written figure close to the live one. The autosave's slow
            // periodic write (kSaveAutosaveMs) is a quarter of this whole window, so
            // without this a reboot still refunds everything since the last write —
            // most of the cheese, back. The kSaveDebounceMs floor caps the churn, and
            // the window is short enough that the extra writes are bounded by it.
            markSaveDirty();
            if (dyingElapsedMs_ >= kCsfDyingGraceMs) {
                fireCSF();
                changed = true;
            }
        } else if (dyingArmed_ || dyingElapsedMs_) {
            // Recovered below 5/5: disarm, and forget what was burned. A pet pulled
            // back from the edge gets the whole window again next time, not a
            // shortening one — the grace is per-brush-with-death, not per-lifetime.
            dyingArmed_ = false;
            dyingElapsedMs_ = 0;
        }
    }

    // Evolution boundary. Fires from rest once the time-in-stage + care gate
    // is met; preempts like the other Area-5 modals (below CSF/Lockout in priority).
    if (nav_ == Nav::Idle && evolveEligible()) {
        fireEvolution();
        changed = true;
    }

    // Lockout crisis fires when Hunger bottoms out. Never during the Hatch
    // (there's no pet), an Evolution, or a Critical System Failure (priority) — nor
    // over a DECRYPTOGRAM board, which is the one preemption that would DESTROY
    // something: the board cannot be walked out of and the ticket into it is already
    // spent, so firing across it would take a found item and give nothing back. The
    // crisis loses nothing by waiting — its deadline starts when it fires, and this
    // check runs again the moment the board is left.
    if (pet_ && !lockoutActive_ && nav_ != Nav::Decryption && nav_ != Nav::Cryptogram &&
        nav_ != Nav::ModalEvolve && nav_ != Nav::ModalCSF && model_.isStarving()) {
        fireLockout();
        changed = true;
    }
    // Expiry is global while the crisis is live — even if the player drifted into
    // the ITEMS-in-Lockout list, the deadline still resolves the modal.
    if (lockoutActive_ && nowMs_ >= lockoutDeadlineMs_) {
        expireLockout();
        changed = true;
    }

    // Autosave. Persist a debounced write after a meaningful
    // change, plus a slower periodic write so passive decay is captured. Only
    // once there's something worth saving (a raised pet or a populated rack) — a
    // bare first-boot Hatch leaves the save empty, so the next boot re-hatches.
    if (store_ && (pet_ || !rack_.empty())) {
        const bool debounced = saveDirty_ && nowMs_ - lastSaveMs_ >= kSaveDebounceMs;
        const bool periodic = nowMs_ - lastSaveMs_ >= kSaveAutosaveMs;
        if (debounced || periodic) persistSave();
    }
    return changed;
}

// Every held-button gesture, resolved on the DWELL rather than on an edge: A's list
// repeat and C's step-back (tickListNav, game_listnav.cpp), and the five hold-B second
// actions — the CFG factory reset, the ITEMS type filter, the MOVES show-all, ROCK THE
// DOCK's scout sheet and the VAULT bulk open. They share bHeld_/bDownMs_, so each is
// scoped tightly enough that no two can be in flight at once, and each clears bHeld_ as
// it fires so the release resolves as nothing rather than as the tap underneath it.
bool Game::tickHeldGestures(uint32_t nowMs) {
    bool changed = false;
    // CFG hidden Factory Reset: a held B reveals the reset screen from
    // System Info, then commits the wipe — each a deliberate ~5s hold. bHeld_ is
    // set on the B press and cleared on release (or here once a stage fires).
    if (nav_ == Nav::Detail && enteredId() == SubmenuId::Cfg && bHeld_) {
        const uint32_t held = nowMs_ - bDownMs_;
        if (cfgScreen_ == CfgScreen::SysInfo && held >= kFactoryRevealMs) {
            cfgScreen_ = CfgScreen::FactoryReset;
            factoryScope_ = 0;
            bHeld_ = false;
            lastInputMs_ = nowMs_;   // the reveal counts as activity (no defocus race)
            changed = true;
        } else if (cfgScreen_ == CfgScreen::FactoryReset &&
                   held >= kFactoryCommitMs) {
            executeFactoryReset();
            changed = true;
        }
    }

    // The list contract's two held-button behaviours: A's repeat, and C's step-back
    // repeat resolving into the back-out. Both are gated on there being a list under
    // the cursor at all (game_listnav.cpp).
    if (tickListNav()) changed = true;

    // The ITEMS hold-B gesture. Once owned, crossing kItemFilterHoldMs while still
    // resting on the ITEMS list cycles the type filter (ALL -> FOOD -> BUFFS -> QUEST ->
    // ALL, or the finer category axis once the type-picker is owned too) and re-parks
    // the cursor on the new list's first row; clearing bHeld_ here means a release
    // before this fires resolves as the ordinary tap instead, opening the focused item
    // (onButton's release edge, itemFilterReleaseB). Unowned players never arm bHeld_
    // (onItemsList opens immediately on press), so this block never fires for them —
    // nor does the picker screen, which arms nothing.
    if (bHeld_ && nav_ == Nav::Submenu && face_ == Face::Pet &&
        enteredId() == SubmenuId::Items && itemsScreen_ == ItemsScreen::List &&
        itemTabsUnlocked() && nowMs_ - bDownMs_ >= kItemFilterHoldMs) {
        bHeld_ = false;
        itemFilter_ = nextItemFilter(itemFilter_, itemPickerUnlocked());
        auto rows = buildInventoryRows(registry_, inventory_, lockoutItemsContext_, itemFilter_);
        listRow_ = firstSelectableRow(rows);
        if (listRow_ < 0) listRow_ = 0;
        lastInputMs_ = nowMs_;   // the cycle counts as activity (no defocus race)
        changed = true;
    }

    // The MOVES picker's hold-B gesture (the same shape as the ITEMS one above):
    // crossing kMoveFilterHoldMs toggles moveShowAll_ and re-parks the cursor at row 0,
    // since the row set changes size. A release before this fires resolves as the tap
    // instead (moveFilterReleaseB — unequip, or drill into the focused move). No unlock
    // gate, unlike the ITEMS filter. The tab check is load-bearing: trainScreen_ keeps
    // its value across a hub page change, so the MODS picker sits at the same
    // Nav::Detail with a stale MovePicker beside it.
    if (bHeld_ && nav_ == Nav::Detail && loadoutTab_ == LoadoutTab::Moves &&
        trainScreen_ == TrainScreen::MovePicker &&
        !moveConfirm_ && nowMs_ - bDownMs_ >= kMoveFilterHoldMs) {
        bHeld_ = false;
        moveShowAll_ = !moveShowAll_;
        movePick_ = 0;
        lastInputMs_ = nowMs_;
        changed = true;
    }

    // ROCK THE DOCK's hold-B gesture, the same shape as the two above: crossing
    // kTourneyScoutHoldMs on the bracket opens the focused entrant's SCOUT sheet — its
    // full kit, read on the very page the operator reads their own kit on. A release
    // before it fires resolves as the ordinary tap instead (tourneyReleaseB), which is
    // "fight my own bout". Both are ways of engaging with the fighter under the cursor,
    // which is what keeps the hold related to the tap beneath it.
    if (bHeld_ && nav_ == Nav::Tourney && tourneyView_ == TourneyView::Bracket &&
        nowMs_ - bDownMs_ >= kTourneyScoutHoldMs) {
        bHeld_ = false;
        openTourneyScout();
        lastInputMs_ = nowMs_;
        changed = true;
    }

    // e: the Hacker VAULT hold-B gesture (reuses bHeld_/bDownMs_ — VAULT
    // doesn't otherwise hold-B). Once owned, crossing kBulkOpenHoldMs bulk-opens
    // every owned cache sharing the focused row's rarity in one action; a release
    // before this fires resolves as the ordinary single-open instead (onButton's
    // release edge, vaultBulkReleaseB). Guarded to the Hacker VAULT submenu so it can
    // never fire while the (also bHeld_-driven) CFG Factory-Reset hold is in flight.
    if (bHeld_ && face_ == Face::Hacker && nav_ == Nav::Submenu &&
        enteredHackerId() == HackerSlotId::Vault && bulkOpenUnlocked() &&
        nowMs_ - bDownMs_ >= kBulkOpenHoldMs) {
        bHeld_ = false;
        const ItemDef* rows[16];
        const int n = vaultOwnedRows(rows, 16);
        if (n > 0) {
            if (hackerVaultRow_ >= n) hackerVaultRow_ = 0;
            // Caches only — the row can have moved under a held B, and "open every one
            // of these" is not a thing a Decryptogram can do.
            if (rows[hackerVaultRow_]->use == ItemDef::Use::OpenContainer)
                openAllCachesOfRarity(*rows[hackerVaultRow_]);
        }
        lastInputMs_ = nowMs_;
        changed = true;
    }
    return changed;
}

// The global idle timer collapsing the menu tree back to the habitat. Last, because what
// counts as activity is re-stamped by several of the blocks above.
bool Game::tickIdleDefocus(uint32_t nowMs) {
    bool changed = false;
    // The global idle timer collapses the menu tree — suspended inside modals,
    // a running process / an active Lockout, the CFG screens and the Stacker
    // defrag board, and while an ITEMS-filter / VAULT-bulk hold gesture is
    // mid-flight (no new presses during the hold). UPDATES is carved back out of
    // the CFG exemption: it already gets the long hands-off radio budget below
    // (it holds a live association) but must still eventually collapse an
    // abandoned check, or a forgotten screen pins the radio open forever.
    const bool inCfgScreen = nav_ == Nav::Detail && enteredId() == SubmenuId::Cfg &&
                              !updateScreenOpen();
    const bool suspended = lockoutActive_ || nav_ == Nav::ModalFeeding ||
                           nav_ == Nav::Process || nav_ == Nav::Decryption ||
                           nav_ == Nav::ModalLineSelect || nav_ == Nav::ModalEggPick ||
                           nav_ == Nav::ModalHatchReveal || nav_ == Nav::Isolation ||
                           nav_ == Nav::Chroma || nav_ == Nav::Cryptogram ||
                           nav_ == Nav::ModalEvolve || nav_ == Nav::ModalCSF ||
                           nav_ == Nav::Combat || nav_ == Nav::ExploreControl ||
                           nav_ == Nav::Encounter || nav_ == Nav::Wifi ||
                           nav_ == Nav::Shop || nav_ == Nav::ModShop ||
                           nav_ == Nav::CacheYield ||
                           nav_ == Nav::BulkYield || nav_ == Nav::PostEncounter ||
                           nav_ == Nav::Stacker || nav_ == Nav::ArcadeResult ||
                           // ROCK THE DOCK's bracket is a READING screen: eight entrants,
                           // their levels, and the next opponent's species and Exploit
                           // are what a loadout is chosen against, and five seconds is
                           // shorter than reading them. Collapsing it would also drop
                           // the operator onto the habitat mid-decision with a run
                           // still in play, which reads as the screen crashing.
                           nav_ == Nav::Tourney ||
                           inCfgScreen ||
                           // A held B is one of the four hold gestures mid-flight and
                           // must not be collapsed under. A held A is only the list
                           // repeat, which is not in the same position: every step it
                           // fires re-stamps lastInputMs_ (Game::tickListNav), so a
                           // genuinely held A keeps the tree open by USING it. Listing
                           // it here as well would mean a release edge lost anywhere —
                           // a bounce, a screen change under the thumb — pins the whole
                           // menu tree open until the next press.
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
    // since holding one open is what arms the radio (radioScanWanted / linkWanted),
    // the standard 5s would power it down before a sweep or a peer's beacon could
    // land — the screen would starve itself. UPDATES takes the same budget for the
    // matching reason: it is a screen you wait in front of, and five seconds is
    // shorter than an association plus a fetch, let alone reading the verdict one
    // leaves behind and deciding whether to install it.
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
    // threshold counts — e.g. threshold 90, hunger 92 -> 88 pays for 2 points
    // (92..91), not all 4.
    const int hungerBefore = model_.hunger();
    model_.tick(elapsedMs);
    // A Hunger point lost to decay also ends any stacking-food run
    // (ItemEffect::Kind::HungerStacking) — the pet has moved on, so the next
    // Polltatoes starts counting from one again.
    if (model_.hunger() < hungerBefore) stackingFoodRun_ = 0;
    const int rate = hungerXpRateLevel();
    if (rate <= 0) return;
    const int pointsAboveThreshold =
        hungerBefore - std::max(model_.hunger(), hungerXpThreshold());
    if (pointsAboveThreshold > 0) addCombatXp(pointsAboveThreshold * rate);
}

} // namespace mal
