#include "core/app/game.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/render/canvas.h"
#include "core/render/font5x7.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/carousel.h"
#include "core/ui/combat_screen.h"
#include "core/ui/items_screen.h"
#include "core/ui/maint_screen.h"
#include "core/ui/modals.h"
#include "core/ui/stat_screen.h"
#include "core/ui/train_screen.h"
#include "generated/assets.h"

namespace mal {

Game::Game(StartMode mode, const char* hatchedCreature, ISaveStore* store)
    : registry_(ContentRegistry::embedded()), bits_(kStartBits), store_(store) {
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
        stampSlotKinds();      // #12: this is a pet install too — lock its slots
        // startingForLine's auto-equip may not match THIS creature's stamped slot 0
        // kind — drop it if it no longer fits (e.g. a Defend-first line vs. an
        // Attack-first slotKinds row; see embedded_content).
        enforceSlotKindInvariant();
    } else {
        // Real first boot: empty save -> Decryption Hatch.
        startHatch();
    }
}

// --- Tick ------------------------------------------------------------------

bool Game::tick(uint32_t nowMs) {
    nowMs_ = nowMs;
    bool changed = dirty_;
    dirty_ = false;

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
        if (pet_ && nav_ != Nav::ModalHatch && !inEggPhase())
            tickHungerAndAwardXp(elapsed);      // no pet (empty save / line-select): nothing decays
        // Bandwidth regenerates over real active time — a slow trickle
        // back to the cap so a walk can't be farmed in one sitting but recovers
        // on its own between sessions, independent of whether EXPL is open.
        if (bandwidth_ < bandwidthMax()) {
            bandwidthAccumMs_ += elapsed;
            const uint32_t step = kBandwidthRegenMinutesPerPoint * 60u * 1000u;
            while (bandwidthAccumMs_ >= step && bandwidth_ < bandwidthMax()) {
                bandwidthAccumMs_ -= step;
                ++bandwidth_;
            }
        }
        // Boot-Sector incubation clock (egg-at-idle): counts down in real time while
        // the egg sits at idle. Frozen inside either hatch modal — the decrypt runs its
        // own countdown, and the crack cinematic must be allowed to finish its frames
        // (it can legitimately be opened with seconds left, and the clock running out
        // underneath would hatch the pet mid-animation). Reaching 0 out here
        // auto-hatches straight to Process (no soft-lock if the player never opens
        // either one).
        if (inEggPhase() && nav_ != Nav::ModalHatch && nav_ != Nav::ModalHatchReveal) {
            bootHatchRemainMs_ = bootHatchRemainMs_ > elapsed
                                     ? bootHatchRemainMs_ - elapsed : 0;
            if (bootHatchRemainMs_ == 0) { completeHatch(); changed = true; }
        }
    }
    lastModelMs_ = nowMs_;

    // Audit-capture policy: expire the hot-broadcast window and
    // clear the re-arm cooldown on schedule (runtime-only timing; no radio here).
    auditCapture_.tick(nowMs_);

    // Decryption Hatch. Arm the egg timer on the first tick (real
    // game-ms, like Lockout), then hatch when it elapses. No fail state.
    if (nav_ == Nav::ModalHatch) {
        if (!hatchArmed_) {
            hatchDeadlineMs_ = nowMs_ + kHatchDurationMs;
            hatchArmed_ = true;
        }
        if (nowMs_ >= hatchDeadlineMs_) {
            completeHatch();
            changed = true;
        }
    }

    if (nowMs - lastBeatMs_ >= static_cast<uint32_t>(kHeartbeatMs)) {
        lastBeatMs_ = nowMs;
        beat_++;
        changed = true;
        // Resting motion: the pet drifts a little way around the habitat's shelf
        // anchor, the way its species moves (core/model/idle_wander.h). An egg is
        // parked instead — it sits where it was laid, with its incubation readout
        // drawn directly above it — and so is an empty save, which has no pet at all.
        if (pet_ && !inEggPhase()) petWander_.step(pet_->locomotion);
        else petWander_.park();
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
                ++combatBeat_;
                // Hands-off auto-explore: a wild fight's result is a REVEAL
                // with no human to press B, so hold it ~3s then auto-dismiss back to
                // the habitat so exploration keeps stepping (finishCombat advances the
                // streak / cancels on a loss). Boss/Sim fights stay player-driven.
                if (exploreActive_ && combatCaller_ == CombatCaller::Wild &&
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

    // Combat sprite motion ticks on its own faster cadence (kCombatAnimMs) instead of
    // the shared heartbeat above, so a feeding-frenzy streak's accelerated turn pacing
    // still shows real sprite motion between repaints rather than a stuck pose. Only
    // runs while a fight is actually on screen — zero cost otherwise.
    if (nav_ == Nav::Combat) {
        if (nowMs - lastCombatAnimMs_ >= static_cast<uint32_t>(kCombatAnimMs)) {
            lastCombatAnimMs_ = nowMs;
            combatAnimBeat_++;
            combatHitBeat_++;
            changed = true;
        }
    } else {
        lastCombatAnimMs_ = nowMs;   // stay primed so re-entering combat doesn't burst-catch-up
    }

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
    // (there's no pet), an Evolution, or a Critical System Failure (priority).
    if (pet_ && !lockoutActive_ && nav_ != Nav::ModalHatch && nav_ != Nav::ModalEvolve &&
        nav_ != Nav::ModalCSF && model_.isStarving()) {
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

    // d: the ITEMS hold-A gesture. Once owned, crossing kItemFilterHoldMs
    // while still resting on the ITEMS list cycles the type filter (ALL -> FOOD ->
    // BUFFS -> QUEST -> ALL, or the finer category axis once the type-picker is
    // owned too) and re-parks the cursor on the new list's first row; clearing
    // aHeld_ here means a release before this fires resolves as the ordinary
    // short-press step instead (onButton's release edge, itemFilterReleaseA).
    // Unowned players never arm aHeld_ (onItemsList steps immediately on press), so
    // this block never fires for them — nor does the picker screen, which arms nothing.
    if (aHeld_ && nav_ == Nav::Submenu && face_ == Face::Pet &&
        enteredId() == SubmenuId::Items && itemsScreen_ == ItemsScreen::List &&
        itemTabsUnlocked() && nowMs_ - aDownMs_ >= kItemFilterHoldMs) {
        aHeld_ = false;
        itemFilter_ = nextItemFilter(itemFilter_, itemPickerUnlocked());
        auto rows = buildInventoryRows(registry_, inventory_, lockoutItemsContext_, itemFilter_);
        listRow_ = firstSelectableRow(rows);
        if (listRow_ < 0) listRow_ = 0;
        lastInputMs_ = nowMs_;   // the cycle counts as activity (no defocus race)
        changed = true;
    }

    // TRAIN move picker's hold-A gesture (reuses aHeld_/aDownMs_): A already
    // stepped movePick_ on press (onMovePicker); crossing kMoveFilterHoldMs while
    // still held ADDITIONALLY toggles moveShowAll_ and re-parks the cursor at row 0
    // (the row set changes size). A release before this fires leaves the step as
    // the only effect. No unlock gate, unlike the ITEMS filter above.
    if (aHeld_ && nav_ == Nav::Detail && trainScreen_ == TrainScreen::MovePicker &&
        !moveConfirm_ && nowMs_ - aDownMs_ >= kMoveFilterHoldMs) {
        aHeld_ = false;
        moveShowAll_ = !moveShowAll_;
        movePick_ = 0;
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
            openAllCachesOfRarity(*rows[hackerVaultRow_]);
        }
        lastInputMs_ = nowMs_;
        changed = true;
    }

    // The global 5s idle timer collapses the menu tree — suspended inside modals
    // a running process / an active Lockout, and while a CFG /
    // ITEMS-filter / VAULT-bulk hold gesture is mid-flight (no new presses during
    // the hold).
    const bool suspended = lockoutActive_ || nav_ == Nav::ModalFeeding ||
                           nav_ == Nav::Process || nav_ == Nav::ModalHatch ||
                           nav_ == Nav::ModalLineSelect || nav_ == Nav::ModalEggPick ||
                           nav_ == Nav::ModalHatchReveal ||
                           nav_ == Nav::ModalEvolve || nav_ == Nav::ModalCSF ||
                           nav_ == Nav::Combat || nav_ == Nav::ExploreControl ||
                           nav_ == Nav::Encounter || nav_ == Nav::Wifi ||
                           nav_ == Nav::Shop || nav_ == Nav::ModShop ||
                           nav_ == Nav::CacheYield ||
                           nav_ == Nav::BulkYield || nav_ == Nav::PostEncounter ||
                           bHeld_ || aHeld_ ||
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

// --- Input -----------------------------------------------------------------

void Game::onButton(const ButtonEvent& ev) {
    // A released B ends any in-progress hold gesture — CFG hidden Factory Reset
    // (releasing early aborts the reveal/commit) or the e VAULT
    // bulk-open hold (releasing before kBulkOpenHoldMs resolves as the ordinary
    // single-open instead, vaultBulkReleaseB — a no-op for any other bHeld_ user).
    // A released A resolves the d ITEMS filter hold's short-press half the
    // same way (itemFilterReleaseA — a no-op unless that hold is armed). The TRAIN
    // move picker's hold (kMoveFilterHoldMs below) shares aHeld_/aDownMs_ too, but
    // has no release-time action of its own — A already stepped on press, so a
    // release just needs the arm cleared, which itemFilterReleaseA's unconditional
    // `aHeld_ = false` does regardless of which screen set it (its own context
    // check then no-ops harmlessly outside ITEMS).
    if (!ev.pressed) {
        if (ev.button == Button::B) {
            const bool wasHeld = bHeld_;
            bHeld_ = false;
            if (wasHeld) vaultBulkReleaseB();
        } else if (ev.button == Button::A) {
            itemFilterReleaseA();
        }
        return;
    }
    lastInputMs_ = nowMs_;
    dirty_ = true;
    // A+C is the reserved no-op stub everywhere pet-side EXCEPT combat — the one
    // place the Exploit override is live. The early-out is bypassed
    // only in the combat Nav state, which routes the chord to the override picker.
    if (ev.chordAC) {
        if (nav_ == Nav::Combat) onCombat(ev);
        // Hacker face: A+C at the top level flips PET ↔ HACKER. On the HACKER
        // face this takes priority so the player can ALWAYS return to the pet — nothing
        // else claims the chord there (you can't explore/combat/hatch from it).
        else if (face_ == Face::Hacker && (nav_ == Nav::Idle || nav_ == Nav::Cursor))
            toggleFace();
        // Explore-control chord: while explore-mode is running, A+C on the
        // habitat opens the control overlay (A ping / B warp / C stop) instead of the
        // Hacker face — a deliberate tradeoff (the Hacker face is unreachable until you
        // Cancel explore). An egg can't explore, so the hatch chord below is unaffected.
        else if (exploreActive_ && (nav_ == Nav::Idle || nav_ == Nav::Cursor))
            nav_ = Nav::ExploreControl;
        // Egg home stretch (redesign): the Exploit chord IS how you hatch —
        // the ⚡ exploit symbol on the idle screen invites A+C to crack the egg. Which
        // screen that opens is the line's business: a Decrypt line gets its brute-force
        // minigame from the incubation half-way point, any other line gets the hatch
        // animation in the last kHatchRevealMs. (The first A/C of the chord may have
        // summoned the cursor; both openers re-check their own gate + park the modal
        // from any state.)
        else if (hatchMinigameReady()) openDecryptMinigame();
        else if (hatchRevealReady()) openHatchReveal();
        // Otherwise (pet face, idle/cursor, not exploring, past the egg phase): flip to
        // the Hacker face. The lowest-priority top-level action, so the existing chord
        // meanings above are all preserved. An egg's A+C stays the hatch exploit only —
        // the operator face belongs to a raised pet (and stays inert in the first half).
        else if ((nav_ == Nav::Idle || nav_ == Nav::Cursor) && !inEggPhase())
            toggleFace();
        return;
    }

    switch (nav_) {
        case Nav::Idle:
            // B decrypts the egg once it's ready (second half of incubation,
            // redesign) — the one idle action for an egg. A/C still summon the
            // carousel (an egg can still be carried around / walked to hatch faster).
            if (ev.button == Button::B && hatchMinigameReady()) openDecryptMinigame();
            else if (ev.button == Button::A) summonCursor(0);
            else if (ev.button == Button::C) summonCursor(kCarouselSlots - 1);
            break;
        case Nav::Cursor:
            if (ev.button == Button::A) cursor_ = (cursor_ + 1) % kCarouselSlots;
            else if (ev.button == Button::C)
                cursor_ = (cursor_ + kCarouselSlots - 1) % kCarouselSlots;
            // B enters the focused submenu. The Hacker face routes to its own slots
            // (inaccessible ones are inert); pet-side, a locked egg slot is inert.
            else if (ev.button == Button::B) {
                if (face_ == Face::Hacker) enterHackerSubmenu();
                else if (!eggSlotLocked(enteredId())) enterSubmenu();
            }
            break;
        case Nav::Submenu:
            // Hacker face reuses the L2 state but its own slot roster/dispatch.
            if (face_ == Face::Hacker) { onHackerSubmenu(ev); break; }
            switch (enteredId()) {
                case SubmenuId::Stat:
                    if (ev.button == Button::A) {
                        statPage_ = (statPage_ + 1) % 5;
                        loadoutScroll_ = 0;         // fresh page -> scroll to the top
                    } else if (ev.button == Button::C) {
                        nav_ = Nav::Cursor; statPage_ = 0; loadoutScroll_ = 0;
                    } else if (ev.button == Button::B && statPage_ == 1) {
                        // B is a no-op on every other page (the `statPage_ == 1`
                        // guard) — it only scrolls the LOADOUT page's row window,
                        // and only when
                        // there's something to scroll (mirrors drawMovePicker's
                        // scroll math, train_screen.cpp). Wraps back to the top.
                        const auto rows = buildLoadoutRows(
                            registry_, moveLoadout_, loadout_,
                            pet_ ? pet_->stage : Stage::BootSector, inEggPhase());
                        const int total = static_cast<int>(rows.size());
                        if (total > kLoadoutVisibleRows) {
                            loadoutScroll_ += kLoadoutVisibleRows;
                            if (loadoutScroll_ >= total) loadoutScroll_ = 0;
                        }
                    }
                    break;
                case SubmenuId::Items:
                    if (itemsScreen_ == ItemsScreen::Picker) onItemsPicker(ev);
                    else onItemsList(ev);
                    break;
                case SubmenuId::Maint: onMaintList(ev); break;
                case SubmenuId::Cfg: onCfgList(ev); break;
                case SubmenuId::Arch: onArchList(ev); break;
                case SubmenuId::Mods: onModsList(ev); break;
                case SubmenuId::Train: onTrainList(ev); break;
                case SubmenuId::Expl: onExplList(ev); break;
                default: if (ev.button == Button::C) nav_ = Nav::Cursor; break;
            }
            break;
        case Nav::Detail:
            switch (enteredId()) {
                case SubmenuId::Items: onItemsDetail(ev); break;
                case SubmenuId::Maint: onMaintAction(ev); break;
                case SubmenuId::Cfg: onCfgDetail(ev); break;
                case SubmenuId::Arch: onArchRecord(ev); break;
                case SubmenuId::Mods:
                    if (modDetail_) onModDetail(ev); else onModPicker(ev);
                    break;
                case SubmenuId::Train: onTrainDetail(ev); break;
                default: if (ev.button == Button::C) nav_ = Nav::Cursor; break;
            }
            break;
        case Nav::Combat: onCombat(ev); break;
        case Nav::ExploreControl: onExploreControl(ev); break;
        case Nav::Encounter: onEncounter(ev); break;
        case Nav::Wifi: onWifi(ev); break;
        case Nav::Shop: onShop(ev); break;
        case Nav::ModShop: onShop(ev); break;
        case Nav::WarpPicker: onWarpPicker(ev); break;
        case Nav::RollbackPicker: onRollbackPicker(ev); break;
        case Nav::CacheYield: onCacheYield(ev); break;
        case Nav::BulkYield: onBulkYield(ev); break;
        // Post-encounter status readout: informational only — ANY
        // button (A/B/C) dismisses it, unlike the standard A/B/C contract.
        case Nav::PostEncounter: dismissPostEncounter(); break;
        case Nav::Process:
            // Non-interruptible: ignored while running; the outcome dismisses.
            if (processResolved_ && (ev.button == Button::B || ev.button == Button::C))
                nav_ = Nav::Detail;
            break;
        case Nav::ModalFeeding:
            if (ev.button == Button::B || ev.button == Button::C) endFeeding();
            break;
        case Nav::ModalLineSelect: {
            // Line-select: A cycles the highlighted line, B lays that line's
            // egg (-> idle), C is DISABLED — an empty save has no pet to return to, so
            // the choice can't be cancelled (mirrors the hatch's no-cancel rule). The
            // A+C chord is the reserved no-op stub, already filtered above.
            const auto lines = availableEggLines();   // only UNLOCKED lines (matches the modal)
            if (lines.empty()) break;
            const int n = static_cast<int>(lines.size());
            if (ev.button == Button::A) lineSelectRow_ = (lineSelectRow_ + 1) % n;
            else if (ev.button == Button::B) layEgg(lines[lineSelectRow_ % n]);
            break;
        }
        case Nav::ModalHatch:
            // Any A/B/C press accelerates the decrypt (-1 minute + jiggle); C is
            // not "back" here, it counts as a press too (the A+C chord is the
            // no-op stub, already filtered above). No fail state.
            hatchPress();
            break;
        case Nav::ModalHatchReveal:
            // The crack cinematic runs itself and hatches off the end — every button is
            // inert for its ~2 seconds, so a stray press can't skip the one animation
            // the screen exists to show.
            break;
        case Nav::ModalEggPick:
            // Clutch Pick: A and C AIM at the first/second half of the clutch (left/top
            // vs right/bottom, whichever way this round cuts) and B commits that half —
            // so C is not "back" here either, and the modal can't be cancelled out of.
            if (ev.button == Button::A) eggPickAim(false);
            else if (ev.button == Button::C) eggPickAim(true);
            else if (ev.button == Button::B) eggPickCommit();
            break;
        case Nav::ModalEvolve:
            // B continues once the reveal is up; A is a no-op and C is
            // disabled (nothing to cancel — the evolution already happened). The
            // A+C chord is the reserved no-op stub, already filtered above.
            if (ev.button == Button::B && evolveRevealed()) completeEvolution();
            break;
        case Nav::ModalCSF:
            // B acknowledges once the crash FX has held. A is a no-op and C
            // is DISABLED — death is not cancellable. A+C is the reserved no-op stub
            // (already filtered above).
            if (ev.button == Button::B && csfRevealed()) acknowledgeCSF();
            break;
        case Nav::ModalLockout:
            if (ev.button == Button::A) lockoutPayOption_ = !lockoutPayOption_;
            else if (ev.button == Button::B) {
                if (lockoutPayOption_) {
                    if (bits_ >= kLockoutBitsCost) { bits_ -= kLockoutBitsCost; resolveLockout(); }
                } else {                              // Open Items (Lockout context)
                    lockoutItemsContext_ = true;
                    itemFilter_ = ItemFilter::All;    // d: reset on every entry
                    // A crisis skips the type-picker even when it's owned — the
                    // Lockout list already floats the resolving items to the top.
                    itemsScreen_ = ItemsScreen::List;
                    cursor_ = itemsSlot();
                    auto rows = buildInventoryRows(registry_, inventory_, true, itemFilter_);
                    listRow_ = firstSelectableRow(rows);
                    nav_ = Nav::Submenu;
                }
            }
            // C is disabled — a crisis can't be cancelled.
            break;
    }
}

// --- Navigation ------------------------------------------------------------

void Game::summonCursor(int slot) { cursor_ = slot; nav_ = Nav::Cursor; }

void Game::enterSubmenu() {
    nav_ = Nav::Submenu;
    switch (enteredId()) {
        case SubmenuId::Items: {
            lockoutItemsContext_ = false;
            itemFilter_ = ItemFilter::All;    // d: reset on every entry
            // Owned, ITEMS opens on the category tiles; otherwise straight on the list.
            itemsScreen_ = itemPickerUnlocked() ? ItemsScreen::Picker : ItemsScreen::List;
            itemPickRow_ = 0;
            auto rows = buildInventoryRows(registry_, inventory_, false, itemFilter_);
            listRow_ = firstSelectableRow(rows);
            break;
        }
        case SubmenuId::Maint: listRow_ = 0; break;
        case SubmenuId::Cfg: listRow_ = 0; bHeld_ = false; break;
        case SubmenuId::Arch:
            listRow_ = 0; archAction_ = ArchAction::Store; archConfirm_ = false; break;
        case SubmenuId::Mods:
            listRow_ = 0; modConfirm_ = false; modDetail_ = false;
            modDetailId_ = nullptr; break;
        case SubmenuId::Train:
            trainRow_ = 0; trainScreen_ = TrainScreen::MovePicker;
            moveConfirm_ = false; movePendingId_ = nullptr; break;
        case SubmenuId::Expl:
            explNavArea_ = -1;                          // always open at the TOP level
            listRow_ = firstSelectableExplRow(); break;
        case SubmenuId::Stat: statPage_ = 0; loadoutScroll_ = 0; break;
        default: break;
    }
}

void Game::dropCursor() {
    nav_ = Nav::Idle;
    listRow_ = 0;
    detailItem_ = nullptr;
    statPage_ = 0;
    loadoutScroll_ = 0;
}

bool Game::eggSlotLocked(SubmenuId id) const {
    if (!inEggPhase()) return false;
    switch (id) {
        case SubmenuId::Train:
        case SubmenuId::Maint:
        case SubmenuId::Mods:
        case SubmenuId::Expl:
            // Explore-mode is unavailable to a Boot-Sector egg — an egg
            // can't fight, so it can't explore (nor train / maintain / mod). The
            // old walk-to-accelerate-hatch path retires with the Walk screen; the
            // egg still hatches on its incubation clock + the Wi-Fi network accel.
            return true;
        default:
            // STAT + ITEMS (quest-only) + ARCH/CFG stay reachable for an egg.
            return false;
    }
}

int Game::itemsSlot() const {
    for (int i = 0; i < kCarouselSlots; ++i)
        if (carouselSlots()[i].id == SubmenuId::Items) return i;
    return 1;
}


} // namespace mal
