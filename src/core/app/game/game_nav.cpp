// game_nav.cpp — where a button press lands, and where the cursor stands afterwards.
//
// Two halves of one concern. onButton is the dispatcher: one switch over Nav for the
// press edge and one for the release, routing to the per-state handler that owns each
// screen. What the buttons MEAN is fixed device-wide (CONTRIBUTING: A=Next, B=Accept,
// C=Cancel, A+C the Exploit chord), so a screen joins by gaining a case here rather
// than by inventing its own reading of a press.
//
// The navigation half below is the carousel and the menu tree the dispatcher moves
// through — which slot is entered, which are inert, and how the tree collapses back to
// the habitat. The tick's own half of the button contract (every hold gesture, resolved
// on a dwell rather than an edge) is Game::tickHeldGestures in game_core.cpp.
#include "core/app/game.h"

#include "tunables.h"
#include "core/ui/carousel.h"     // carouselSlots — the slot table the cursor walks
#include "core/ui/items_screen.h" // buildInventoryRows — the ITEMS list under the cursor

namespace mal {

void Game::onButton(const ButtonEvent& ev) {
    // A released B resolves whichever tap/hold gesture was armed. All four live on B and
    // all four are guarded to their own screen, so each release asks them in turn and at
    // most one claims it: the CFG hidden Factory Reset (releasing early aborts the
    // reveal/commit), the VAULT bulk-open (releasing before kBulkOpenHoldMs resolves as
    // the ordinary single-open), the ITEMS filter (a tap opens the focused item), and
    // the MOVES picker's show-all (a tap unequips or drills into the focused move).
    // C on a list is the same shape and settles here too (listBackRelease). Only A never
    // needs a release action: its hold REPEATS the step its press already made rather
    // than replacing it, so letting go only has to stop the clock tickListNav reads.
    if (!ev.pressed) {
        if (ev.button == Button::B) {
            const bool wasHeld = bHeld_;
            if (wasHeld) {
                vaultBulkReleaseB();
                rigServiceReleaseB();
                itemFilterReleaseB();
                moveFilterReleaseB();
                tourneyReleaseB();
            }
            bHeld_ = false;
        } else if (ev.button == Button::A) {
            aHeld_ = false;
        } else if (ev.button == Button::C) {
            // On a list this is where C decides what it was: listBackRelease cancels
            // for a tap and does nothing for a hold that already walked the cursor.
            // Elsewhere (the DECRYPTOGRAM's own repeat) it only disarms.
            listBackRelease();
            cHeld_ = false;
        }
        return;
    }
    lastInputMs_ = nowMs_;
    dirty_ = true;
    // A HELD announcement eats the first press, whatever it was. Only the NEW EGG LINE
    // banner holds (Game::achBannerHeld), and only on the home screen, where it is
    // telling the player about something they can now go and do — so the press that
    // clears it is the acknowledgement, and letting it ALSO open the carousel underneath
    // would mean the banner was dismissed by a gesture aimed at something else. The
    // chord is included deliberately: A+C is a press too, and a player mashing it at a
    // banner they want gone should get what they are asking for.
    if (achBannerHeld()) {
        dismissAchievementBanner();
        return;
    }
    // A+C is the reserved no-op stub everywhere pet-side EXCEPT combat — the one
    // place the Exploit override is live. The early-out is bypassed
    // only in the combat Nav state, which routes the chord to the override picker.
    if (ev.chordAC) {
        if (nav_ == Nav::Combat) onCombat(ev);
        // ROCK THE DOCK: the arena's BRIEFING. The chord is the device's one spare
        // gesture and the bracket screen has already spent A, B and C, so the explainer
        // — the only thing that tells an operator what an arena bout even is — rides
        // the same "chord opens a reader, then plain A/B/C drive it" shape the explore
        // control overlay uses.
        else if (nav_ == Nav::Tourney) onTourney(ev);
        // THE DECRYPTOGRAM: A and C are the cursor's two directions, so the chord is
        // already spent on handing a taken letter back to the pool. onCryptogram sorts
        // the two meanings out itself (a letter held drops it; nothing held opens the
        // RULES page instead), since only it knows which stage the board is in.
        else if (nav_ == Nav::Cryptogram) onCryptogram(ev);
        // The other game engines have nothing else living on the chord, so it is
        // always the RULES page here — open it, or close it if it's already up. The
        // egg-hatch entrants (Decrypt/Clutch/Isolation/Chroma land on their Nav straight
        // out of layEgg with no menu stop) have no other route to this at all, and the
        // CHROMATOPHORE needs it most: it is the one board whose three buttons are all
        // spoken for, so the chord is the only press left that can explain them.
        else if (nav_ == Nav::Stacker || nav_ == Nav::Isolation ||
                 nav_ == Nav::Decryption || nav_ == Nav::ModalEggPick ||
                 nav_ == Nav::Chroma)
            toggleGameBrief();
        // Hacker face: A+C at the top level flips PET ↔ HACKER. On the HACKER
        // face this takes priority so the player can ALWAYS return to the pet — nothing
        // else claims the chord there (you can't explore/combat/hatch from it).
        else if (face_ == Face::Hacker && (nav_ == Nav::Idle || nav_ == Nav::Cursor))
            toggleFace();
        // Explore-control chord: while explore-mode is running, A+C on the
        // habitat opens the control overlay (A ping / B warp / C stop) instead of the
        // Hacker face — a deliberate tradeoff (the Hacker face is unreachable until you
        // Cancel explore). An egg can't explore, so the hatch chord below is unaffected.
        else if (exploreActive_ && (nav_ == Nav::Idle || nav_ == Nav::Cursor)) {
            nav_ = Nav::ExploreControl;
            exploreCtlRow_ = 0;                     // always opens on the first action
        }
        // Egg home stretch (redesign): the Exploit chord IS how you hatch —
        // the ⚡ exploit symbol on the idle screen invites A+C to crack the egg. Which
        // screen that opens is the line's business: a Decrypt line gets its brute-force
        // minigame from the incubation half-way point, any other line gets the hatch
        // animation in the last kHatchRevealMs. (The first A/C of the chord may have
        // summoned the cursor; both openers re-check their own gate + park the modal
        // from any state.)
        else if (hatchRevealReady()) openHatchReveal();
        // Otherwise (pet face, idle/cursor, not exploring, past the egg phase): flip to
        // the Hacker face. The lowest-priority top-level action, so the existing chord
        // meanings above are all preserved. An egg's A+C stays the hatch exploit only —
        // the operator face belongs to a raised pet (and stays inert in the first half).
        else if ((nav_ == Nav::Idle || nav_ == Nav::Cursor) && !inEggPhase())
            toggleFace();
        return;
    }

    // The list contract (game_listnav.cpp). Both arms are scoped to there actually
    // being a list under the cursor, and deliberately so for A: arming aHeld_ on a
    // screen whose A can never repeat leaves a flag nothing will ever clear if the
    // release edge is lost, which is why tick()'s auto-defocus no longer consults it
    // and why this does not set it in the first place. C's press is
    // claimed outright — it settles on the release, as a tap (cancel) or a hold (the
    // backward walk) — and the screen's own C case is reached through leaveFocusedList.
    if (listFocus() != ListFocus::None) {
        if (ev.button == Button::A) {
            aHeld_ = true;
            aDownMs_ = nowMs_;
        } else if (listBackStep(ev)) {
            return;
        }
    }

    switch (nav_) {
        case Nav::Idle:
            // B cracks the egg once its clock is nearly out — the one idle action
            // for an egg, and the same thing the A+C chord does. A/C still summon the
            // carousel (an egg can still be carried around / walked to hatch faster).
            if (ev.button == Button::B && hatchRevealReady()) openHatchReveal();
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
                else if (!slotLocked(enteredId())) enterSubmenu();
            }
            break;
        case Nav::Submenu:
            // Hacker face reuses the L2 state but its own slot roster/dispatch.
            if (face_ == Face::Hacker) { onHackerSubmenu(ev); break; }
            switch (enteredId()) {
                case SubmenuId::Stat:
                    if (ev.button == Button::A) {
                        statPage_ = (statPage_ + 1) % 6;
                        statScroll_ = 0;         // fresh page -> scroll to the top
                    } else if (ev.button == Button::C) {
                        nav_ = Nav::Cursor; statPage_ = 0; statScroll_ = 0;
                    } else if (ev.button == Button::B) {
                        // B scrolls the three pages that flow prose rows (TIERS,
                        // LOADOUT and BUFFS) and is a no-op everywhere else — statScrollSpan
                        // reports {0, 0} for a page with nothing to scroll, and for
                        // one whose rows all fit. It advances by the window the page
                        // actually drew (rows are sized to their own text, so that
                        // count varies), and wraps back to the top past the end.
                        const StatScrollSpan span = statScrollSpan();
                        if (span.shown > 0 && span.shown < span.total) {
                            statScroll_ += span.shown;
                            if (statScroll_ >= span.total) statScroll_ = 0;
                        }
                    }
                    break;
                case SubmenuId::Items:
                    if (itemsScreen_ == ItemsScreen::Picker) onItemsPicker(ev);
                    else onItemsList(ev);
                    break;
                case SubmenuId::Maint: onMaintList(ev); break;
                case SubmenuId::Cfg: onCfgList(ev); break;
                case SubmenuId::Arch:
                    if (archScreen_ == ArchScreen::Picker) onArchPicker(ev);
                    else onArchList(ev);
                    break;
                // MODS is the LOADOUT hub: the same L2 fronts three pages, and which
                // one is open is loadoutTab_ (PRACTISE never rests here — it opens
                // straight into its L3).
                case SubmenuId::Mods:
                    if (loadoutTab_ == LoadoutTab::Mods) onModsList(ev);
                    else if (loadoutTab_ == LoadoutTab::Moves) onTrainList(ev);
                    else onLoadoutHub(ev);
                    break;
                case SubmenuId::Games: onArcadeList(ev); break;
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
                    if (loadoutTab_ == LoadoutTab::Practise) onSimTier(ev);
                    else if (loadoutTab_ == LoadoutTab::Moves) onTrainDetail(ev);
                    else if (modDetail_) onModDetail(ev);
                    else onModPicker(ev);
                    break;
                case SubmenuId::Games: onArcadeCabinet(ev); break;
                default: if (ev.button == Button::C) nav_ = Nav::Cursor; break;
            }
            break;
        case Nav::Combat: onCombat(ev); break;
        case Nav::ExploreControl: onExploreControl(ev); break;
        case Nav::Tourney: onTourney(ev); break;
        case Nav::Encounter: onEncounter(ev); break;
        case Nav::Wifi: onWifi(ev); break;
        case Nav::ShibbolethHail: onShibbolethHail(ev); break;
        case Nav::Shibboleth: onShibboleth(ev); break;
        case Nav::ShibbolethVerdict: onShibbolethVerdict(ev); break;
        case Nav::Shop: onShop(ev); break;
        case Nav::ModShop: onShop(ev); break;
        case Nav::WarpPicker: onWarpPicker(ev); break;
        case Nav::RollbackPicker: onRollbackPicker(ev); break;
        case Nav::CacheYield: onCacheYield(ev); break;
        case Nav::BulkYield: onBulkYield(ev); break;
        // Post-encounter status readout: informational only — ANY
        // button (A/B/C) dismisses it, unlike the standard A/B/C contract.
        case Nav::PostEncounter: dismissPostEncounter(); break;
        case Nav::Stacker: onStacker(ev); break;
        case Nav::Isolation: onIsolation(ev); break;
        case Nav::Chroma: onChroma(ev); break;
        // The arcade payout: informational, so ANY button dismisses it (the
        // PostEncounter contract, not the standard A/B/C one).
        case Nav::ArcadeResult: onArcadeResult(ev); break;
        case Nav::Process:
            // Non-interruptible: ignored while running; the outcome dismisses.
            if (processResolved_ && (ev.button == Button::B || ev.button == Button::C))
                nav_ = Nav::Detail;
            break;
        case Nav::ModalFeeding:
            if (ev.button == Button::B || ev.button == Button::C) endFeeding();
            break;
        case Nav::ModalLineSelect: {
            // Line-select: A cycles the highlighted line, B lays that line's egg
            // (-> idle). The A+C chord is the reserved no-op stub, already filtered above.
            //
            // C BACKS OUT TO ARCH — but only with something on the rack. The rule was
            // never "the hatch can't be cancelled", it was "an empty save has no pet to
            // return to", and reaching this modal from an ARCH Store makes that false:
            // the pet is right there on the shelf, frozen, one Deploy away. Cancelling
            // into a habitat with nothing living in it is still not offered, so a device
            // with an empty rack keeps the old no-cancel behaviour exactly.
            const auto lines = availableEggLines();   // only UNLOCKED lines (matches the modal)
            if (lines.empty()) break;
            const int n = static_cast<int>(lines.size());
            if (ev.button == Button::A) lineSelectRow_ = (lineSelectRow_ + 1) % n;
            else if (ev.button == Button::B) layEgg(lines[lineSelectRow_ % n]);
            else if (ev.button == Button::C && !rack_.empty()) archReturnFromLineSelect();
            break;
        }
        case Nav::Decryption: onDecryption(ev); break;
        case Nav::Cryptogram: onCryptogram(ev); break;
        case Nav::ModalHatchReveal:
            // The crack cinematic runs itself and hatches off the end — every button is
            // inert for its ~2 seconds, so a stray press can't skip the one animation
            // the screen exists to show.
            break;
        case Nav::ModalEggPick:
            // Clutch Pick: A and C AIM at the first/second half of the clutch (left/top
            // vs right/bottom, whichever way this round cuts) and B commits that half —
            // so C is not "back" here either, and the modal can't be cancelled out of.
            // The RULES overlay (opened on the chord, above) is the one exception: while
            // it's up, B/C read as its scroll/close instead, same as every other engine.
            if (onGameBriefInput(ev)) break;
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
                    cursor_ = carouselSlotOf(SubmenuId::Items);
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

// Out of line so game.h can name SubmenuId (ui_state.h) without pulling in the slot
// table that maps a cursor position to one.
SubmenuId Game::enteredId() const { return carouselSlots()[cursor_].id; }

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
        // ARCH always opens on its group picker: a 64-slot shelf plus a record tail is
        // not one list, and the picker is where NEW EGG lives.
        case SubmenuId::Arch:
            archScreen_ = ArchScreen::Picker; archPickRow_ = 0;
            archGroup_ = ArchGroup{};
            listRow_ = 0; archAction_ = ArchAction::Store; archConfirm_ = false; break;
        // MODS always opens on the hub, whichever of its three pages was last used.
        case SubmenuId::Mods:
            loadoutTab_ = LoadoutTab::Hub; loadoutHubRow_ = 0;
            listRow_ = 0; modConfirm_ = false; modDetail_ = false;
            modDetailId_ = nullptr;
            trainRow_ = 0; trainScreen_ = TrainScreen::MovePicker;
            moveConfirm_ = false; movePendingId_ = nullptr; break;
        case SubmenuId::Games: arcadeRow_ = 0; break;
        case SubmenuId::Expl: openExplList(); break;
        case SubmenuId::Stat: statPage_ = 0; statScroll_ = 0; break;
        default: break;
    }
}

void Game::dropCursor() {
    nav_ = Nav::Idle;
    listRow_ = 0;
    detailItem_ = nullptr;
    statPage_ = 0;
    statScroll_ = 0;
}

bool Game::slotLocked(SubmenuId id) const {
#ifdef MALWARIUM_DEMO
    // CFG is Wi-Fi join / update install / SD mount / Factory Reset — nothing a
    // browser can act on, and a reset that would wipe the demo (demo_config.h).
    if (id == SubmenuId::Cfg) return true;
#endif
    if (!inEggPhase()) return false;
    switch (id) {
        case SubmenuId::Games:
        case SubmenuId::Maint:
        case SubmenuId::Mods:
        case SubmenuId::Expl:
            // Explore-mode is unavailable to a Boot-Sector egg — an egg
            // can't fight, so it can't explore (nor equip or maintain). The
            // old walk-to-accelerate-hatch path retires with the Walk screen; the
            // egg still hatches on its incubation clock + the Wi-Fi network accel.
            // GAMES is locked with them: the arcade pays a pet in Bits and
            // Happiness, and an egg is neither playing nor able to bank either. Its
            // own hatch minigame reaches it from the habitat, not from here.
            return true;
        default:
            // STAT + ITEMS (quest-only) + ARCH/CFG stay reachable for an egg.
            return false;
    }
}

int Game::carouselSlotOf(SubmenuId id) const {
    for (int i = 0; i < kCarouselSlots; ++i)
        if (carouselSlots()[i].id == id) return i;
    return 0;
}

} // namespace mal
