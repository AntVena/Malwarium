// game_nav.cpp: where a button press lands, and where the cursor stands afterwards.
//
// Two halves of one concern. onButton is the dispatcher: the release edge settles the
// held gestures, and the press edge goes to the screen that is up through the per-screen
// table (game_screens.cpp). What the buttons MEAN is fixed device-wide (CONTRIBUTING: A=Next, B=Accept,
// C=Cancel, A+C the Exploit chord), so a screen joins by gaining a row in that table
// rather than by inventing its own reading of a press.
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
    // A+C goes to what the chord means on the screen that is up (its row's chord
    // column; null where it means nothing), and never on to the list contract or the
    // plain press below.
    if (ev.chordAC) {
        if (const auto chord = screenRow(nav_).chord) chord(*this, ev);
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

    screenRow(nav_).press(*this, ev);
}

// --- The carousel's own presses ----------------------------------------------
// The home screen, the cursor and the menu tree's two levels: the screens Game itself
// draws, reached through the per-screen table like every other (game_screens.cpp).

void Game::onIdle(const ButtonEvent& ev) {
    // B cracks the egg once its clock is nearly out, like the A+C chord. It is an egg's
    // only idle action. A/C still summon the carousel (an egg can still be carried
    // around / walked to hatch faster).
    if (ev.button == Button::B && hatchRevealReady()) openHatchReveal();
    // A failing pet with a cookie that would save it: B feeds it, as the habitat's
    // FAILING banner says. Nothing else at idle is on B.
    else if (ev.button == Button::B && yubiWouldSave()) feedYubiToSave();
    else if (ev.button == Button::A) summonCursor(0);
    else if (ev.button == Button::C) summonCursor(kCarouselSlots - 1);
}

void Game::onCursor(const ButtonEvent& ev) {
    if (ev.button == Button::A) cursor_ = (cursor_ + 1) % kCarouselSlots;
    else if (ev.button == Button::C) cursor_ = (cursor_ + kCarouselSlots - 1) % kCarouselSlots;
    // B enters the focused submenu. The Hacker face routes to its own slots
    // (inaccessible ones are inert); pet-side, a locked egg slot is inert.
    else if (ev.button == Button::B) {
        if (face_ == Face::Hacker) enterHackerSubmenu();
        else if (!slotLocked(enteredId())) enterSubmenu();
    }
}

// The home screen's chord, highest precedence first.
void Game::onHomeChord() {
    // Hacker face: A+C flips PET <-> HACKER. On the Hacker face this takes priority so
    // the player can always return to the pet; nothing else uses the chord there.
    if (face_ == Face::Hacker) toggleFace();
    // Explore-control chord: while explore-mode is running, A+C opens the control
    // overlay (A ping / B warp / C stop) instead of the Hacker face. A deliberate
    // tradeoff (the Hacker face is unreachable until you Cancel explore). An egg can't
    // explore, so the hatch chord below is unaffected.
    else if (walk_.active) {
        nav_ = Nav::ExploreControl;
        exploreControl_.open();
    }
    // Egg home stretch: the Exploit chord IS how you hatch. The exploit symbol on the
    // idle screen invites A+C to crack the egg. Which screen that opens is the line's
    // business: a Decrypt line gets its brute-force minigame from the incubation
    // half-way point, any other line gets the hatch animation in the last
    // kHatchRevealMs.
    else if (hatchRevealReady()) openHatchReveal();
    // Otherwise (pet face, not exploring, past the egg phase): flip to the Hacker face.
    // The lowest-priority action, so the chord meanings above are all preserved. An
    // egg's A+C is only the hatch exploit, because the operator face belongs to a raised
    // pet (and the chord does nothing in the first half).
    else if (!inEggPhase()) toggleFace();
}

void Game::onSubmenu(const ButtonEvent& ev) {
    // Hacker face reuses the L2 state but its own slot roster/dispatch.
    if (face_ == Face::Hacker) { onHackerSubmenu(ev); return; }
    switch (enteredId()) {
        // STAT is two screens behind one slot (game_stat.cpp): the paged reader, and the
        // INDEX that jumps between its pages and sections.
        case SubmenuId::Stat: stat_.onButton(*this, ev); break;
        case SubmenuId::Items: items_.onButton(*this, ev); break;
        case SubmenuId::Maint: maintScreen_.onButton(*this, ev); break;
        case SubmenuId::Cfg: cfg_.onButton(*this, ev); break;
        case SubmenuId::Arch: arch_.onButton(*this, ev); break;
        // MODS is the LOADOUT hub: the same L2 fronts three pages, and which one is open
        // is loadoutTab_ (PRACTISE never rests here. It opens straight into its L3).
        case SubmenuId::Mods: loadout_.onButton(*this, ev); break;
        case SubmenuId::Games: arcadeScreen_.onList(*this, ev); break;
        case SubmenuId::Expl: expl_.onButton(*this, ev); break;
        default: if (ev.button == Button::C) nav_ = Nav::Cursor; break;
    }
}

void Game::onDetail(const ButtonEvent& ev) {
    switch (enteredId()) {
        case SubmenuId::Items: items_.onDetail(*this, ev); break;
        case SubmenuId::Maint: maintScreen_.onDetail(*this, ev); break;
        case SubmenuId::Cfg: cfg_.onDetail(*this, ev); break;
        case SubmenuId::Arch: arch_.onDetail(*this, ev); break;
        case SubmenuId::Mods: loadout_.onDetail(*this, ev); break;
        case SubmenuId::Games: arcadeScreen_.onCabinet(*this, ev); break;
        default: if (ev.button == Button::C) nav_ = Nav::Cursor; break;
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
