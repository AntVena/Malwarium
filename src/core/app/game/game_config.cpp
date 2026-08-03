#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "version.h"
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

// CFG -----------------------------------------------------------

// Open an L3 CFG screen, seeding whatever picker it opens on so the focus always
// starts at the applied value. The one seam both the top-level list and a group
// screen enter through, so a row behaves identically wherever it is reached from.
void Game::enterCfgScreen(CfgScreen target) {
    cfgScreen_ = target;
    switch (target) {
        case CfgScreen::Display:
        case CfgScreen::Radio:
            cfgGroupRow_ = 0;
            break;
        case CfgScreen::UiMode: cfgUiPick_ = static_cast<int>(uiMode_); break;
        case CfgScreen::Brightness: cfgBrightPick_ = brightness_; break;   // the applied level
        case CfgScreen::Titles: cfgTitlePick_ = equippedTitle_; break;     // the equipped one
        case CfgScreen::Audit: cfgAuditPick_ = static_cast<int>(auditMode()); break;
        case CfgScreen::PediaAp: cfgApPick_ = apEnabled_ ? 1 : 0; break;
        case CfgScreen::Link: cfgLinkPick_ = linkEnabled_ ? 1 : 0; break;
        case CfgScreen::HackerTag: enterHackerTagEditor(); break;
        default: break;
    }
    bHeld_ = false;
    nav_ = Nav::Detail;
}

// Leave the current L3 — back to its group screen if it has one (a radio toggle
// returns to RADIO, not to the top of CFG), otherwise out to the list. Applying a
// setting exits the same way, so B and C land in the same place.
void Game::leaveCfgScreen() {
    const CfgScreen parent = cfgParentGroup(cfgScreen_);
    if (parent == cfgScreen_) { nav_ = Nav::Submenu; return; }
    // Land the group's cursor on the row just left, so a return trip resumes where
    // the operator was rather than at the top.
    const CfgRow* rows = nullptr;
    const int n = cfgGroupRows(parent, rows);
    for (int i = 0; i < n; ++i)
        if (rows[i].target == cfgScreen_) { cfgGroupRow_ = i; break; }
    cfgScreen_ = parent;
    bHeld_ = false;
    dirty_ = true;
}

void Game::onCfgList(const ButtonEvent& ev) {
    const CfgRow* rows = nullptr;
    const int n = cfgRows(rows);
    if (ev.button == Button::A) {
        listRow_ = (listRow_ + 1) % n;
    } else if (ev.button == Button::B) {
        const CfgScreen target = rows[listRow_].target;
        if (target == CfgScreen::ResetHatch) {
            resetToHatch();                  // DEV quick reset — one press (dev_config.h)
            return;
        }
        enterCfgScreen(target);
    } else if (ev.button == Button::C) {
        nav_ = Nav::Cursor;
    }
}

// A group screen (DISPLAY / RADIO): a row list like the L2 above it, one level in.
// A walks it, B opens the focused setting, C returns to the CFG list.
void Game::onCfgGroup(const ButtonEvent& ev) {
    const CfgRow* rows = nullptr;
    const int n = cfgGroupRows(cfgScreen_, rows);
    if (n <= 0) { leaveCfgScreen(); return; }
    if (cfgGroupRow_ >= n) cfgGroupRow_ = 0;
    if (ev.button == Button::A) cfgGroupRow_ = (cfgGroupRow_ + 1) % n;
    else if (ev.button == Button::B) enterCfgScreen(rows[cfgGroupRow_].target);
    else if (ev.button == Button::C) leaveCfgScreen();
}

void Game::onCfgDetail(const ButtonEvent& ev) {
    switch (cfgScreen_) {
        case CfgScreen::Display:
        case CfgScreen::Radio:
            onCfgGroup(ev);
            break;
        case CfgScreen::SysInfo:
            // A re-checks the card: ask the device tier to unmount + remount so one
            // inserted after boot is picked up. The result lands on this screen's own
            // SD line (+ the idle SD-present icon), which is why the action lives
            // here. B arms the hidden hold-to-reveal (the timer fires in tick); C
            // backs to the list.
            if (ev.button == Button::A) requestSdRecheck();
            else if (ev.button == Button::B) { bHeld_ = true; bDownMs_ = nowMs_; }
            else if (ev.button == Button::C) leaveCfgScreen();
            break;
        case CfgScreen::HackerTag: {
            // On-device arcade editor: A cycles the focused cell's char, B
            // advances the caret (and at the ⏎ confirm cell, saves), C deletes the
            // previous char (or backs out from the first cell).
            static const char* kAlphabet =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
            if (ev.button == Button::A) {
                if (editCaret_ < kHackerTagMax) {
                    const char cur = editTag_[editCaret_];
                    const char* pos = std::strchr(kAlphabet, cur);
                    int idx = pos ? static_cast<int>(pos - kAlphabet) : -1;
                    idx = (idx + 1) % static_cast<int>(std::strlen(kAlphabet));
                    editTag_[editCaret_] = kAlphabet[idx];
                }
            } else if (ev.button == Button::B) {
                if (editCaret_ >= kHackerTagMax) saveHackerTag();  // ⏎ confirm
                else ++editCaret_;
            } else if (ev.button == Button::C) {
                if (editCaret_ == 0) leaveCfgScreen();          // back out
                else { --editCaret_; editTag_[editCaret_] = '_'; } // delete prev
            }
            break;
        }
        case CfgScreen::UiMode:
            if (ev.button == Button::A) cfgUiPick_ = (cfgUiPick_ + 1) % 3;
            else if (ev.button == Button::B) {        // apply + back out
                setUiMode(static_cast<UiMode>(cfgUiPick_));
                leaveCfgScreen();
            } else if (ev.button == Button::C) leaveCfgScreen();  // no change
            break;
        case CfgScreen::Brightness:
            // A cycles the level (wrapping); B applies (setBrightness persists +
            // marks the frame dirty, so the device tier re-reads brightness() and
            // drives the backlight); C backs out with no change.
            if (ev.button == Button::A)
                cfgBrightPick_ = (cfgBrightPick_ + 1) % kBrightnessLevels;
            else if (ev.button == Button::B) {        // apply + back out
                setBrightness(cfgBrightPick_);
                leaveCfgScreen();
            } else if (ev.button == Button::C) leaveCfgScreen();  // no change
            break;
        case CfgScreen::Titles:
            // A cycles the focus over the selectable choices only — NONE (-1) plus
            // each UNLOCKED sector Title, skipping locked ones. B equips the focus;
            // C backs out without changing the equip.
            if (ev.button == Button::A) cfgTitlePick_ = nextSelectableTitle(cfgTitlePick_);
            else if (ev.button == Button::B) {        // apply + back out
                equippedTitle_ = cfgTitlePick_;       // -1 (NONE) or an unlocked sector
                markSaveDirty();
                leaveCfgScreen();
            } else if (ev.button == Button::C) leaveCfgScreen();  // no change
            break;
        case CfgScreen::Audit:
            // Escalating picker: A cycles OFF -> SCAN -> SCAN+CAP -> OFF; B applies.
            if (ev.button == Button::A) cfgAuditPick_ = (cfgAuditPick_ + 1) % 3;
            else if (ev.button == Button::B) {        // apply + back out
                setAuditMode(static_cast<AuditMode>(cfgAuditPick_));
                leaveCfgScreen();
            } else if (ev.button == Button::C) leaveCfgScreen();  // no change
            break;
        case CfgScreen::PediaAp:
            if (ev.button == Button::A) cfgApPick_ ^= 1;             // OFF <-> ON
            else if (ev.button == Button::B) {
                setApEnabled(cfgApPick_ != 0);
                // Turning the AP ON leads straight to the QR to connect (also the
                // "see the QR" path when it's already on). Turning it OFF has no QR
                // to show, so it just backs out to the list.
                if (cfgApPick_ != 0) {
                    cfgScreen_ = CfgScreen::PediaQr;
                    pediaQrPage_ = 0;             // start on the join-network QR
                } else {
                    leaveCfgScreen();
                }
            } else if (ev.button == Button::C) leaveCfgScreen();  // no change
            break;
        case CfgScreen::Link:
            if (ev.button == Button::A) cfgLinkPick_ ^= 1;           // OFF <-> ON
            else if (ev.button == Button::B) {
                setLinkEnabled(cfgLinkPick_ != 0);
                leaveCfgScreen();
            } else if (ev.button == Button::C) leaveCfgScreen();  // no change
            break;
        case CfgScreen::Update:
            // Three faces on one screen: the row list, the yes/no confirm an
            // install row opens, and a job's progress. Leaving deliberately does
            // NOT cancel a job — it holds the radio itself, so a download runs to
            // the end and the outcome is waiting here on the way back.
            if (updateJobLive() || installStatus_.state != InstallState::Idle) {
                if (ev.button == Button::C) {
                    // Only a finished job's report is cleared on the way out; a live
                    // one keeps its progress to come back to.
                    if (!updateJobLive()) installStatus_ = InstallStatus{};
                    leaveCfgScreen();
                }
                break;
            }
            if (updateConfirm_ != UpdateTarget::None) {
                if (ev.button == Button::A) updateConfirmPick_ ^= 1;      // NO <-> YES
                else if (ev.button == Button::B) {
                    // The second yes, and the only path to a write.
                    if (updateConfirmPick_ == 1) requestUpdateInstall(updateConfirm_);
                    updateConfirm_ = UpdateTarget::None;
                    updateConfirmPick_ = 0;
                } else if (ev.button == Button::C) {
                    updateConfirm_ = UpdateTarget::None;
                    updateConfirmPick_ = 0;
                }
                dirty_ = true;
                break;
            }
            if (ev.button == Button::A) {
                updateRow_ = (updateRow_ + 1) % updateCheckRows(updateStatus_);
                dirty_ = true;
            } else if (ev.button == Button::B) {
                switch (updateCheckRowKind(updateStatus_, updateRow_)) {
                    case UpdateRowKind::Check:
                        requestUpdateCheck();       // no-ops unless ready
                        updateRow_ = 0;             // a fresh check invalidates the rows
                        break;
                    case UpdateRowKind::Install:
                        updateConfirm_ = updateCheckRowTarget(updateStatus_, updateRow_);
                        updateConfirmPick_ = 0;     // ask before anything is written,
                        dirty_ = true;              // ...starting on NO
                        break;
                    case UpdateRowKind::FlashQr:
                        // Needs no radio and no network — the code is drawn from an
                        // address this device already holds, so it opens even when a
                        // check cannot run, which is when it is most wanted.
                        if (updateSourceKnown_) {
                            cfgScreen_ = CfgScreen::UpdateQr;
                            dirty_ = true;
                        }
                        break;
                }
            } else if (ev.button == Button::C) leaveCfgScreen();
            break;
        case CfgScreen::UpdateQr:
            // One page, so A does nothing here. C returns to UPDATES with its cursor
            // still on the row that opened this. The 5s auto-defocus and the screen
            // sleep are both held off while it shows (qrScreenActive) — this code is
            // read by a phone, which takes longer than the usual idle window.
            if (ev.button == Button::C) leaveCfgScreen();
            break;
        case CfgScreen::PediaQr:
            // Two QR pages: A cycles join-network <-> 'Pedia-URL; C exits. The 5s
            // auto-defocus + the device screen-sleep are both suspended here
            // (qrScreenActive), and resume once C exits — scanning/connecting takes
            // longer than the usual idle window.
            if (ev.button == Button::A) {
                pediaQrPage_ = (pediaQrPage_ + 1) % pediaQrPages(netProvisioned_);
                dirty_ = true;
            } else if (ev.button == Button::C) leaveCfgScreen();
            break;
        case CfgScreen::FactoryReset:
            if (ev.button == Button::A) factoryScope_ ^= 1;          // cycle scope
            else if (ev.button == Button::B) { bHeld_ = true; bDownMs_ = nowMs_; }  // arm hold-to-commit
            else if (ev.button == Button::C) nav_ = Nav::Submenu;    // safe abort
            break;
        case CfgScreen::ResetHatch:
            break;  // never an L3 screen — handled inline in onCfgList
    }
}

void Game::executeFactoryReset() {
    // The two scopes differ in exactly what the screen promises. RESET PET lays a
    // fresh egg over the same operator — resetToHatch() leaves every player-level
    // tally standing, which IS "keeps 'Pedia progress". WIPE EVERYTHING clears that
    // standing state first, so the reset that follows lands on a device with no
    // history. Order matters: the wipe runs BEFORE resetToHatch, so the fresh egg it
    // lays is tallied as raised (a wiped device still knows its own current pet).
    if (factoryScope_ == 1) wipeDeviceProgress();
    resetToHatch();
}

void Game::cycleUiMode() {
    uiMode_ = static_cast<UiMode>((static_cast<int>(uiMode_) + 1) % 3);
    dirty_ = true;
}

void Game::setBrightness(int level) {
    if (level < 0) level = 0;
    if (level >= kBrightnessLevels) level = kBrightnessLevels - 1;
    if (level == brightness_) return;
    brightness_ = level;
    dirty_ = true;
    markSaveDirty();   // a persisted CFG pref (save v14) — survives a reboot
}

void Game::drawCfg(Framebuffer& fb) const {
    switch (cfgScreen_) {
        case CfgScreen::SysInfo: {
            const float hf = bHeld_
                ? static_cast<float>(nowMs_ - bDownMs_) / kFactoryRevealMs
                : 0.0f;
            // RANK reads "R{n} {title}" so the endless NUMERIC rank shows alongside the
            // (threshold-unlocked, growable) title — same "R%d %s" form as STAT.
            char rankLine[28];
            std::snprintf(rankLine, sizeof(rankLine), "R%d %s", hackerRank_,
                          hackerRankTitle(hackerRank_));
            drawSysInfo(fb, nowMs_, uiMode_, kFirmwareVersion,
                       rankLine, networksSeen_, power_,
                       sdStatus_, auditCapture_.enabled(), handshakesSeen_,
                       equippedTitleName(), radioOwner_, hf);
            break;
        }
        case CfgScreen::Display:
            drawCfgDisplay(fb, cfgGroupRow_, uiMode_, brightness_); break;
        case CfgScreen::Radio:
            drawCfgRadio(fb, cfgGroupRow_, radioOwner_,
                         static_cast<int>(auditMode()), linkEnabled_, apEnabled_,
                         updateJobLive());
            break;
        case CfgScreen::HackerTag: drawHackerTag(fb, editTag_, editCaret_); break;
        case CfgScreen::Titles:
            drawTitles(fb, cfgTitlePick_, titlesUnlocked_, equippedTitle_); break;
        case CfgScreen::UiMode: drawUiModeToggle(fb, cfgUiPick_, uiMode_); break;
        case CfgScreen::Brightness: drawBrightness(fb, cfgBrightPick_, brightness_); break;
        case CfgScreen::Audit:
            drawAuditMode(fb, cfgAuditPick_, static_cast<int>(auditMode())); break;
        case CfgScreen::PediaAp: drawApToggle(fb, cfgApPick_, apEnabled_); break;
        case CfgScreen::Link:
            drawLinkToggle(fb, cfgLinkPick_, linkEnabled_, linkAmbientStarved());
            break;
        case CfgScreen::Update:
            // Same three faces as the input handler, in the same order — a live or
            // just-finished job outranks the confirm, which outranks the list.
            if (updateJobLive() || installStatus_.state != InstallState::Idle) {
                if (updateJobTarget() == UpdateTarget::None &&
                    installStatus_.state == InstallState::Idle) {
                    drawUpdateCheck(fb, updateCheckReady(), netProvisioned_,
                                    updateSourceKnown_, updateStatus_, netStatus_,
                                    kFirmwareVersion, updateManifestUrl_, updateRow_);
                } else {
                    drawUpdateProgress(fb, installStatus_, netStatus_);
                }
            } else if (updateConfirm_ != UpdateTarget::None) {
                const bool fw = updateConfirm_ == UpdateTarget::Firmware;
                drawUpdateConfirm(fb, updateConfirm_,
                                  fw ? kFirmwareVersion : webBundleVersion_,
                                  fw ? updateStatus_.firmwareVersion
                                     : updateStatus_.webVersion,
                                  updateConfirmPick_);
            } else {
                drawUpdateCheck(fb, updateCheckReady(), netProvisioned_,
                                updateSourceKnown_, updateStatus_, netStatus_,
                                kFirmwareVersion, updateManifestUrl_, updateRow_);
            }
            break;
        case CfgScreen::UpdateQr: drawUpdateQr(fb, updateManifestUrl_); break;
        case CfgScreen::PediaQr: drawPediaQr(fb, pediaQrPage_, netProvisioned_); break;
        case CfgScreen::FactoryReset: {
            const float hf = bHeld_
                ? static_cast<float>(nowMs_ - bDownMs_) / kFactoryCommitMs
                : 0.0f;
            drawFactoryReset(fb, factoryScope_, hf);
            break;
        }
        case CfgScreen::ResetHatch: break;
    }
}

// ARCH ----------------------------------------------------------

void Game::onArchList(const ButtonEvent& ev) {
    // Row 0 = active pet; the next rows = frozen rack entries; the trailing rows =
    // RETIRED/CORRUPTED records (read-only). A walks (wraps), B opens the focused
    // entry, C backs to the carousel.
    const int rows = 1 + static_cast<int>(rack_.size()) +
                     static_cast<int>(records_.size());
    if (ev.button == Button::A) {
        listRow_ = (listRow_ + 1) % rows;
    } else if (ev.button == Button::B) {
        // Records open a read-only detail; live pets open the action set.
        if (!archRowIsRecord())
            archAction_ = archRowIsActive() ? ArchAction::Store : ArchAction::Deploy;
        archConfirm_ = false;
        nav_ = Nav::Detail;
    } else if (ev.button == Button::C) {
        nav_ = Nav::Cursor;
    }
}

void Game::onArchRecord(const ButtonEvent& ev) {
    // A RETIRED/CORRUPTED record is read-only: no actions, C backs.
    if (archRowIsRecord()) {
        if (ev.button == Button::C) nav_ = Nav::Submenu;
        return;
    }
    // Inline confirm for Store/Deploy: A toggles, B commits the choice, C aborts.
    if (archConfirm_) {
        if (ev.button == Button::A) archConfirmChoice_ ^= 1;
        else if (ev.button == Button::B) {
            if (archConfirmChoice_ == 1) {
                if (archAction_ == ArchAction::Store) archStoreActive();
                else if (archAction_ == ArchAction::Deploy)
                    archDeployStored(listRow_ - 1);
                else if (archAction_ == ArchAction::Release)
                    archReleaseStored(listRow_ - 1);
                return;   // the commit set nav_/state itself
            }
            archConfirm_ = false;     // Cancel -> stay in the record
        } else if (ev.button == Button::C) {
            archConfirm_ = false;
        }
        return;
    }

    const bool active = archRowIsActive();
    if (ev.button == Button::A) {
        // Cycle within the pet's action set. Active: Store → Sell → Store. Stored adds a
        // no-reward Release valve: Deploy → Sell → Release → Deploy.
        if (active) {
            archAction_ = (archAction_ == ArchAction::Store) ? ArchAction::Sell
                                                             : ArchAction::Store;
        } else {
            archAction_ = (archAction_ == ArchAction::Deploy)  ? ArchAction::Sell
                        : (archAction_ == ArchAction::Sell)    ? ArchAction::Release
                                                               : ArchAction::Deploy;
        }
    } else if (ev.button == Button::B) {
        if (archAction_ == ArchAction::Sell) return;   // Daemon-only gate (no Daemon yet)
        if (archAction_ == ArchAction::Store &&
            static_cast<int>(rack_.size()) >= rackSlots())
            return;                                     // rack full -> blocked (gate shown)
        archConfirm_ = true;                            // Store/Deploy/Release -> confirm
        archConfirmChoice_ = 0;                         // default Cancel
    } else if (ev.button == Button::C) {
        nav_ = Nav::Submenu;
    }
}

// Freeze a SaveStoredPet snapshot of a live pet (active save → rack, or the
// outgoing active during a Deploy swap): vitals, creature-level state
// (combatLevel/combatXp/statPoints/slotKinds), and the pet's own move + mod
// loadout (owned/equipped moves, installed mod slots) — a pet's TRAIN/MODS state
// travels with it through the rack, like its level.
static SaveStoredPet freezePet(const CreatureDef* pet, const PetModel& m, int gen,
                               int defragCount, int combatLevel, int combatXp,
                               const int (&statPoints)[kLevelStatCount],
                               const Game::SlotKind (&slotKinds)[kMaxMoveSlots],
                               const MoveLoadout& moveLoadout, const Loadout& loadout,
                               uint32_t timeInStageMs, int bestDeepWebDepth,
                               uint32_t dyingElapsedMs) {
    SaveStoredPet p;
    std::strncpy(p.id, pet->id, kSaveIdCap - 1);
    p.hunger = m.hunger();
    p.frag = m.fragmentation();
    p.happy = m.happiness();
    p.mistakes = m.careMistakes();
    p.debuffs = m.debuffs();
    p.ghost = m.hasGhost() ? 1 : 0;
    p.generation = gen;
    p.defragCount = defragCount;   // the defrag tally survives freeze/thaw
    p.combatLevel = combatLevel;
    p.combatXp = combatXp;
    for (int i = 0; i < kLevelStatCount; ++i) p.statPoints[i] = statPoints[i];
    for (int i = 0; i < kMaxMoveSlots; ++i)
        p.slotKinds[i] = static_cast<uint8_t>(slotKinds[i]);
    for (const char* mv : moveLoadout.owned()) {
        SaveId id; std::strncpy(id.id, mv, kSaveIdCap - 1);
        p.ownedMoves.push_back(id);
    }
    for (int i = 0; i < kMaxMoveSlots; ++i)
        if (const char* e = moveLoadout.equipped(i)) std::strncpy(p.equippedMoves[i].id, e, kSaveIdCap - 1);
    for (int i = 0; i < kModSlots; ++i)
        if (const char* e = loadout.equipped(i)) std::strncpy(p.equippedMods[i].id, e, kSaveIdCap - 1);
    p.timeInStageMs = timeInStageMs;   // evolution-timer progress survives freeze/thaw
    p.bestDeepWebDepth = bestDeepWebDepth;  // this pet's own DeepWeb record survives freeze/thaw
    p.dyingElapsedMs = dyingElapsedMs;      // a 5/5 pet thaws mid-window, not with a fresh one
    return p;
}

void Game::archStoreActive() {
    if (!pet_ || static_cast<int>(rack_.size()) >= rackSlots()) return;
    // Set the active pet aside into a free slot, then vacate the active save → the
    // contextual new-egg Hatch fires. Persist immediately so the stored pet
    // survives a reboot during the egg timer (the save now has an empty active +
    // a populated rack).
    rack_.push_back(freezePet(pet_, model_, generation_, defragCount_, combatLevel_,
                               combatXp_, statPoints_, slotKinds_, moveLoadout_, loadout_,
                               nowMs_ - stageEnteredMs_, bestDeepWebDepth_,
                               dyingElapsedMs_));
    archConfirm_ = false;
    listRow_ = 0;
    startHatch();        // pet_ = nullptr, nav_ = ModalHatch
    persistSave();
}

void Game::archDeployStored(int storedIdx) {
    if (storedIdx < 0 || storedIdx >= static_cast<int>(rack_.size()) || !pet_) return;
    // Slot-neutral swap: the deployed pet becomes active; the current active
    // freezes into the slot it vacated.
    const SaveStoredPet incoming = rack_[storedIdx];
    const CreatureDef* next = registry_.creature(incoming.id);
    if (!next) return;                              // unknown id — abort the swap
    rack_[storedIdx] = freezePet(pet_, model_, generation_, defragCount_, combatLevel_,
                                  combatXp_, statPoints_, slotKinds_, moveLoadout_, loadout_,
                                  nowMs_ - stageEnteredMs_, bestDeepWebDepth_,
                                  dyingElapsedMs_);

    installPet(next);
    model_ = PetModel();
    model_.setHunger(incoming.hunger);
    model_.setFragmentation(incoming.frag);
    model_.setHappiness(incoming.happy);
    model_.setCareMistakes(incoming.mistakes);
    model_.setDebuffs(incoming.debuffs);
    model_.setGhost(incoming.ghost != 0);
    generation_ = incoming.generation;
    defragCount_ = incoming.defragCount;            // thaw the defrag tally
    bestDeepWebDepth_ = incoming.bestDeepWebDepth;  // thaw this pet's own DeepWeb record
    // Thaw the incoming pet's dying window mid-flight. dyingArmed_ is deliberately NOT
    // restored: it anchors against nowMs_, so the next tick re-arms it against the
    // current clock while this accumulator carries the only figure that means anything.
    dyingElapsedMs_ = incoming.dyingElapsedMs;
    dyingArmed_ = false;
    // v26: thaw the incoming pet's creature-level state (was previously never
    // restored — the deployed pet silently inherited the outgoing pet's level).
    combatLevel_ = incoming.combatLevel;
    combatXp_ = incoming.combatXp;
    for (int i = 0; i < kLevelStatCount; ++i) statPoints_[i] = incoming.statPoints[i];
    for (int i = 0; i < kMaxMoveSlots; ++i)
        slotKinds_[i] = static_cast<SlotKind>(incoming.slotKinds[i]);
    // The incoming pet's own move + mod loadout: has-a, not shared. Empty
    // ownedMoves marks a pre-v29 stored pet with no recorded TRAIN state, so it
    // seeds its line's starting kit instead. The mod SPARE pool stays untouched
    // (player-level, like the ITEMS inventory) — only the installed slots are per-pet.
    if (!incoming.ownedMoves.empty()) {
        moveLoadout_ = MoveLoadout{};
        for (const auto& mv : incoming.ownedMoves)
            if (const MoveDef* def = registry_.move(mv.id)) moveLoadout_.grant(def->id);
        for (int i = 0; i < kMaxMoveSlots; ++i)
            if (const MoveDef* def = registry_.move(incoming.equippedMoves[i].id))
                moveLoadout_.equip(i, def->id);
    } else {
        moveLoadout_ = MoveLoadout::startingForLine(registry_, next->line);
    }
    loadout_.resetSlots();
    for (int i = 0; i < kModSlots; ++i)
        if (const ModDef* def = registry_.mod(incoming.equippedMods[i].id))
            loadout_.setEquipped(i, def->id);
    // the re-farm curve is per-pet — a swapped-in pet farms cleared areas
    // fresh (it isn't carried in the rack blob; a redeployed pet starts undepleted).
    for (auto& row : subRefarmCount_) for (auto& c : row) c = 0;
    stageEnteredMs_ = nowMs_ - incoming.timeInStageMs;  // thaw the evolution-timer progress
    lastModelMs_ = nowMs_;                          // no decay jump on the swap
    // Backfill any still-Unset slot from the newly-active pet's layout (covers a
    // pre-v26 stored pet with no recorded slot typing), then drop any equip that no
    // longer matches its slot's stamped kind.
    stampSlotKinds();
    enforceSlotKindInvariant();
    archConfirm_ = false;
    listRow_ = 0;
    nav_ = Nav::Idle;                              // show the newly-active pet
    persistSave();
}

void Game::archReleaseStored(int storedIdx) {
    // A no-reward Release valve: drop a stored pet from the rack entirely to
    // free a slot (e.g. a full rack of non-Daemon pets that can't be Sold). Leaves no
    // record (a plain release, not a CSF/retire) — the pet is simply gone.
    if (storedIdx < 0 || storedIdx >= static_cast<int>(rack_.size())) return;
    rack_.erase(rack_.begin() + storedIdx);
    archConfirm_ = false;
    listRow_ = 0;
    nav_ = Nav::Submenu;   // back to the (now shorter) rack list
    persistSave();
}

void Game::debugSeedRack(const char* creatureId) {
    const CreatureDef* c = registry_.creature(creatureId);
    if (!c || static_cast<int>(rack_.size()) >= rackSlots()) return;
    SaveStoredPet p;
    std::strncpy(p.id, c->id, kSaveIdCap - 1);
    p.hunger = 60; p.frag = 10; p.happy = 70; p.mistakes = 1;
    p.debuffs = 0; p.ghost = 0; p.generation = 1;
    rack_.push_back(p);
}

// HackerTag editor --------------------------------------------

void Game::enterHackerTagEditor() {
    // Seed the working buffer from the live tag, padding to the fixed cell count
    // with '_' (a valid alphabet char). Caret starts at the first cell.
    for (int i = 0; i < kHackerTagMax; ++i) {
        const char c = hackerTag_[i];
        editTag_[i] = (c == '\0') ? '_' : c;
    }
    editTag_[kHackerTagMax] = '\0';
    editCaret_ = 0;
}

void Game::saveHackerTag() {
    std::strncpy(hackerTag_, editTag_, sizeof(hackerTag_) - 1);
    hackerTag_[sizeof(hackerTag_) - 1] = '\0';
    markSaveDirty();
    nav_ = Nav::Submenu;
}

// Web 'Pedia rename ('s "one safe write") — the same charset the on-device
// arcade editor cycles through, validated up front so a bad POST never mutates
// the live tag.
bool Game::setHackerTag(const char* s) {
    if (!s) return false;
    const size_t len = std::strlen(s);
    if (len == 0 || len > static_cast<size_t>(kHackerTagMax)) return false;
    for (size_t i = 0; i < len; ++i) {
        const char c = s[i];
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        if (!ok) return false;
    }
    std::strncpy(hackerTag_, s, sizeof(hackerTag_) - 1);
    hackerTag_[sizeof(hackerTag_) - 1] = '\0';
    markSaveDirty();
    return true;
}


}  // namespace mal
