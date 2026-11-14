// game_nav.cpp: where a button press lands, and where the cursor stands afterwards.
//
// Two halves of one concern. onButton is the dispatcher: one switch over Nav for the
// press edge and one for the release, routing to the per-state handler that owns each
// screen. What the buttons MEAN is fixed device-wide (CONTRIBUTING: A=Next, B=Accept,
// C=Cancel, A+C the Exploit chord), so a screen joins by gaining a case here rather
// than by inventing its own reading of a press.
//
// The navigation half below is the carousel and the menu tree the dispatcher moves
// through, which slot is entered, which are inert, and how the tree collapses back to
// the habitat. The tick's own half of the button contract (every hold gesture, resolved
// on a dwell rather than an edge) is Game::tickHeldGestures in game_core.cpp.
#include "core/app/game.h"

#include "tunables.h"
#include "core/ui/carousel.h"     // carouselSlots: the slot table the cursor walks

namespace mal {

void Game::onButton(const ButtonEvent& ev) {
    // A released B resolves the tap/hold gesture it armed, on the screen that is up
    // (releaseHoldB, game_holds.cpp): a hold that already landed has disarmed it, and
    // anything shorter is the tap.
    // C on a list is the same shape and settles here too (listBackRelease). Only A never
    // needs a release action: its hold REPEATS the step its press already made rather
    // than replacing it, so letting go only has to stop the clock tickListNav reads.
    if (!ev.pressed) {
        if (ev.button == Button::B) {
            if (bHeld_) releaseHoldB();
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
    const uint32_t quietMs = nowMs_ - lastInputMs_;
    lastInputMs_ = nowMs_;
    dirty_ = true;
    playSound(ev.chordAC                ? Sound::KeyChord
              : ev.button == Button::A ? Sound::KeyNext
              : ev.button == Button::B ? Sound::KeyAccept
                                       : Sound::KeyBack);
    // The first press after a long quiet reports what happened meanwhile, and is spent
    // on it, because the pet may have changed since the player last looked and their
    // press was aimed at the old state.
    if (nav_ != Nav::AwayDigest && openAwayDigestIfDue(quietMs)) return;
    // A HELD announcement eats the first press, whatever it was. Only the NEW EGG LINE
    // banner holds (Game::achBannerHeld), and only on the home screen, where it is
    // telling the player about something they can now go and do, so the press that
    // clears it is the acknowledgement, and letting it ALSO open the carousel underneath
    // would mean the banner was dismissed by a gesture aimed at something else. The
    // chord is included deliberately: A+C is a press too, and a player mashing it at a
    // banner they want gone should get what they are asking for.
    if (achBannerHeld()) {
        dismissAchievementBanner();
        return;
    }
    // A+C does nothing pet-side except in combat, where it opens the Exploit
    // override. The early-out is bypassed only in the combat Nav state, which
    // routes the chord to the override picker.
    if (ev.chordAC) {
        if (nav_ == Nav::Combat) fightScreen_.onButton(*this, ev);
        // ROCK THE DOCK: the arena's BRIEFING. The chord is the device's one spare
        // gesture and the bracket screen has already spent A, B and C, so the explainer
        // (the only thing that tells an operator what an arena bout even is) rides
        // the same "chord opens a reader, then plain A/B/C drive it" shape the explore
        // control overlay uses.
        else if (nav_ == Nav::Tourney) arena_.onButton(*this, ev);
        // THE DECRYPTOGRAM: A and C are the cursor's two directions, so the chord is
        // already spent on handing a taken letter back to the pool. Its screen sorts
        // the two meanings out itself (a letter held drops it; nothing held opens the
        // RULES page instead), since only it knows which stage the board is in.
        else if (nav_ == Nav::Cryptogram) cryptogramScreen_.onButton(*this, ev);
        // Nothing else in these engines uses the chord, so it toggles the RULES page.
        // The egg-hatch entrants (Decrypt/Clutch/Isolation/Chroma land on their Nav
        // straight out of layEgg with no menu stop) have no other route to this at all,
        // and the CHROMATOPHORE needs it most: it is the one board whose three buttons
        // are all spoken for, so the chord is the only press left that can explain them.
        else if (nav_ == Nav::Stacker || nav_ == Nav::Fragger || nav_ == Nav::Isolation ||
                 nav_ == Nav::Decryption || nav_ == Nav::ModalEggPick ||
                 nav_ == Nav::Chroma)
            toggleGameBrief();
        // Hacker face: A+C at the top level flips PET ↔ HACKER. On the Hacker face
        // this takes priority so the player can always return to the pet; nothing else
        // uses the chord there.
        else if (face_ == Face::Hacker && (nav_ == Nav::Idle || nav_ == Nav::Cursor))
            toggleFace();
        // Explore-control chord: while explore-mode is running, A+C on the
        // habitat opens the control overlay (A ping / B warp / C stop) instead of the
        // Hacker face. A deliberate tradeoff (the Hacker face is unreachable until you
        // Cancel explore). An egg can't explore, so the hatch chord below is unaffected.
        else if (walk_.active && (nav_ == Nav::Idle || nav_ == Nav::Cursor)) {
            nav_ = Nav::ExploreControl;
            exploreControl_.open();
        }
        // Egg home stretch (redesign): the Exploit chord IS how you hatch.
        // The ⚡ exploit symbol on the idle screen invites A+C to crack the egg. Which
        // screen that opens is the line's business: a Decrypt line gets its brute-force
        // minigame from the incubation half-way point, any other line gets the hatch
        // animation in the last kHatchRevealMs. (The first A/C of the chord may have
        // summoned the cursor; both openers re-check their own gate + park the modal
        // from any state.)
        else if (hatchRevealReady()) openHatchReveal();
        // Otherwise (pet face, idle/cursor, not exploring, past the egg phase): flip to
        // the Hacker face. The lowest-priority top-level action, so the existing chord
        // meanings above are all preserved. An egg's A+C is only the hatch exploit,
        // because the operator face belongs to a raised pet (and the chord does nothing
        // in the first half).
        else if ((nav_ == Nav::Idle || nav_ == Nav::Cursor) && !inEggPhase())
            toggleFace();
        return;
    }

    // The list contract (game_listnav.cpp). Both arms are scoped to there actually
    // being a list under the cursor, and deliberately so for A: arming aHeld_ on a
    // screen whose A can never repeat leaves a flag nothing will ever clear if the
    // release edge is lost, which is why tick()'s auto-defocus does not consult it
    // and why this does not set it in the first place. C's press is
    // claimed outright. It settles on the release, as a tap (cancel) or a hold (the
    // backward walk), and the screen's own C case is reached through listTarget's leave.
    if (listTarget()) {
        if (ev.button == Button::A) {
            aHeld_ = true;
            aDownMs_ = nowMs_;
        } else if (listBackStep(ev)) {
            return;
        }
    }

    switch (nav_) {
        case Nav::Idle:
            // B cracks the egg once its clock is nearly out, like the A+C chord. It
            // is an egg's only idle action. A/C still summon the carousel (an egg can
            // still be carried around / walked to hatch faster).
            if (ev.button == Button::B && hatchRevealReady()) openHatchReveal();
            // A failing pet with a cookie that would save it: B feeds it, as the
            // habitat's FAILING banner says. Nothing else at idle is on B.
            else if (ev.button == Button::B && yubiWouldSave()) feedYubiToSave();
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
                // STAT is two screens behind one slot (game_stat.cpp): the paged
                // reader, and the INDEX that jumps between its pages and sections.
                case SubmenuId::Stat: stat_.onButton(*this, ev); break;
                case SubmenuId::Items: items_.onButton(*this, ev); break;
                case SubmenuId::Maint: maintScreen_.onButton(*this, ev); break;
                case SubmenuId::Cfg: cfg_.onButton(*this, ev); break;
                case SubmenuId::Arch: arch_.onButton(*this, ev); break;
                // MODS is the LOADOUT hub: the same L2 fronts three pages, and which
                // one is open is loadoutTab_ (PRACTISE never rests here. It opens
                // straight into its L3).
                case SubmenuId::Mods:
                    loadout_.onButton(*this, ev);
                    break;
                case SubmenuId::Games: arcadeScreen_.onList(*this, ev); break;
                case SubmenuId::Expl: expl_.onButton(*this, ev); break;
                default: if (ev.button == Button::C) nav_ = Nav::Cursor; break;
            }
            break;
        case Nav::Detail:
            switch (enteredId()) {
                case SubmenuId::Items: items_.onDetail(*this, ev); break;
                case SubmenuId::Maint: maintScreen_.onDetail(*this, ev); break;
                case SubmenuId::Cfg: cfg_.onDetail(*this, ev); break;
                case SubmenuId::Arch: arch_.onDetail(*this, ev); break;
                case SubmenuId::Mods:
                    loadout_.onDetail(*this, ev);
                    break;
                case SubmenuId::Games: arcadeScreen_.onCabinet(*this, ev); break;
                default: if (ev.button == Button::C) nav_ = Nav::Cursor; break;
            }
            break;
        case Nav::Combat: fightScreen_.onButton(*this, ev); break;
        case Nav::ExploreControl: exploreControl_.onButton(*this, ev); break;
        case Nav::Tourney: arena_.onButton(*this, ev); break;
        case Nav::Story: storyReader_.onButton(*this, ev); break;
        case Nav::TipCard: tipCardScreen_.onButton(*this, ev); break;
        case Nav::AwayDigest: awayDigest_.onButton(*this, ev); break;
        case Nav::StoryArchive: chapters_.onButton(*this, ev); break;
        case Nav::Encounter: encounter_.onButton(*this, ev); break;
        case Nav::Wifi: wifiScreen_.onButton(*this, ev); break;
        case Nav::ShiboleetHail: shiboleet_.onHail(*this, ev); break;
        case Nav::Shiboleet: shiboleet_.onRiddle(*this, ev); break;
        case Nav::ShiboleetVerdict: shiboleet_.onVerdict(*this, ev); break;
        case Nav::Shop:
        case Nav::ModShop: shop_.onButton(*this, ev); break;
        case Nav::WarpPicker: warpPicker_.onButton(*this, ev); break;
        case Nav::ItemOffer: itemOffer_.onButton(*this, ev); break;
        case Nav::RollbackPicker: statPicker_.onRollback(*this, ev); break;
        case Nav::RepartitionPicker: statPicker_.onRepartition(*this, ev); break;
        case Nav::CacheYield: vault_.onReveal(*this, ev); break;
        case Nav::BulkYield: vault_.onBulkReveal(*this, ev); break;
        case Nav::PostEncounter: postEncounter_.onButton(*this, ev); break;
        case Nav::Stacker: stackerScreen_.onButton(*this, ev); break;
        case Nav::Fragger: fraggerScreen_.onButton(*this, ev); break;
        case Nav::Isolation: isolationScreen_.onButton(*this, ev); break;
        case Nav::Chroma: chromaScreen_.onButton(*this, ev); break;
        // The arcade payout: informational, so ANY button dismisses it (the
        // PostEncounter contract, not the standard A/B/C one).
        case Nav::ArcadeResult: arcadeScreen_.onResult(*this, ev); break;
        case Nav::Process:
            // Non-interruptible: ignored while running; the outcome dismisses.
            if (maint_ && maint_->resolved &&
                (ev.button == Button::B || ev.button == Button::C)) {
                maint_.reset();
                nav_ = Nav::Detail;
            }
            break;
        case Nav::ModalFeeding: feedingScreen_.onButton(*this, ev); break;
        case Nav::ModalLineSelect: lineSelect_.onButton(*this, ev); break;
        case Nav::Decryption: decryptionScreen_.onButton(*this, ev); break;
        case Nav::Cryptogram: cryptogramScreen_.onButton(*this, ev); break;
        case Nav::ModalHatchReveal: hatchReveal_.onButton(*this, ev); break;
        case Nav::ModalEggPick: eggPickScreen_.onButton(*this, ev); break;
        // B continues once the reveal is up and acknowledges once the crash has held.
        // C is disabled on both: the evolution already happened, and death is not
        // cancellable. The A+C chord is the reserved no-op stub, already filtered above.
        case Nav::ModalEvolve: evolve_.onButton(*this, ev); break;
        case Nav::ModalCSF: csf_.onButton(*this, ev); break;
        case Nav::ModalLockout: lockoutScreen_.onButton(*this, ev); break;
    }
}

// --- Navigation ------------------------------------------------------------

// Out of line so game.h can name SubmenuId (ui_state.h) without pulling in the slot
// table that maps a cursor position to one.
SubmenuId Game::enteredId() const { return carouselSlots()[cursor_].id; }

void Game::summonCursor(int slot) {
    cursor_ = slot;
    nav_ = Nav::Cursor;
    markTipSeen(Tip::MenuOpened);   // the idle menu nudge has done its job
    // The player has just reached for the menu, so they are looking: the moment to say
    // WHY a slot is marked, once per need (game_onboard.cpp). Dismissing the card lands
    // back here on the carousel.
    if (face_ == Face::Pet) openCareTipIfDue();
}

void Game::enterSubmenu() {
    nav_ = Nav::Submenu;
    switch (enteredId()) {
        case SubmenuId::Items: {
            lockoutItemsContext_ = false;
            items_.enter(*this);
            break;
        }
        case SubmenuId::Maint: maintScreen_.enter(); break;
        case SubmenuId::Cfg: cfg_.enter(); bHeld_ = false; break;
        case SubmenuId::Arch: arch_.enter(); break;
        // MODS always opens on the hub, whichever of its three pages was last used.
        case SubmenuId::Mods:
            loadout_.enter();
            break;
        case SubmenuId::Games: arcadeScreen_.enter(); break;
        case SubmenuId::Expl: openExplList(); break;
        // Entering STAT always lands on VITALS, never on the index or on the page last
        // read: the slot is entered to check on the pet, and where the last visit
        // happened to end is not that.
        case SubmenuId::Stat: stat_.reset(); break;
        default: break;
    }
}

void Game::dropCursor() {
    nav_ = Nav::Idle;
    stat_.reset();
}

bool Game::slotLocked(SubmenuId id) const {
#ifdef MALWARIUM_DEMO
    // CFG is Wi-Fi join / update install / SD mount / Factory Reset: a browser
    // can do none of them, and a reset would wipe the demo (demo_config.h).
    if (id == SubmenuId::Cfg) return true;
#endif
    if (!inEggPhase()) return false;
    switch (id) {
        case SubmenuId::Games:
        case SubmenuId::Maint:
        case SubmenuId::Mods:
        case SubmenuId::Expl:
            // A Boot-Sector egg cannot fight, so it cannot explore, equip or
            // maintain. The old walk-to-accelerate-hatch path retires with the Walk
            // screen; the egg still hatches on its incubation clock + the Wi-Fi
            // network accel. GAMES is locked with them: the arcade pays a pet in
            // Bits and Happiness, and an egg is neither playing nor able to bank
            // either. Its own hatch minigame reaches it from the habitat, not from
            // here.
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
