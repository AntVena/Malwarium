#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/content/areas/area_defs.h"
#include "core/content/content_riddles.h"
#include "core/model/cant.h"
#include "core/ui/combat_screen.h"       // kSwarmSeatW/H: the flock's cell in a fight
#include "core/ui/shiboleet_screen.h"   // ...and the one it takes on the meeting screens

// game_shiboleet.cpp: THE SHIBOLEET, the guardian encounter. Named for what is asking:
// every guardian is a node of the Shiboleet, the swarm the Silk Lode's chapters reveal.
//
// Something has been watching every network in the area for a very long time, and when
// the radio has nothing new to hand the pet, it is what the walk finds instead
// (game_net.cpp routes there on a dry sighting queue). It does not attack on sight. It
// SPEAKS, and it speaks the CANT, which the pet does not.
//
// What it does next is graded on how much of that tongue the pet can read
// (cantFluencyPct, core/model/cant.h), in three bands:
//
//   AFFRONT: under kShiboleetAffrontBelowPct. It will not hear an illiterate pet out,
//             and its fight follows from the refusal.
//   RIDDLE:  the middle, and where nearly all of the game lives. A riddle drawn in the
//             Cant with three replies drawn the same way. Answer it and the guardian is
//             satisfied; answer wrong, or say nothing for kShiboleetReplyHoldBeats, and
//             it takes the silence for an answer and the fight starts.
//   BOON:    over kShiboleetBoonAbovePct. Fluent enough that the two simply talk. No
//             riddle, no fight; the pet comes away rested, or with an escort.
//
// THREE SCREENS, AND THE RIDDLE IS THE MIDDLE ONE. A meeting is a HAIL
// (Nav::ShiboleetHail) (the thing arrives, does something the pet can see, and speaks),
// then whatever the band calls for, then a VERDICT (Nav::ShiboleetVerdict): what the
// guardian made of it, what that paid or cost, and what happens next. Every band passes
// through both, so an affront is a refusal the player watched rather than a boss that
// appeared, and a lost riddle is something the guardian DID rather than a fight with no
// stated cause. The encounter explains itself only on these two screens: the Cant
// strip, the sigil count and the purse are read on the hail, and a sigil earned lights
// its own cell on the verdict.
//
// WHAT A WIN ACTUALLY BUYS, and why it is two things. Answering correctly always pays
// the Happiness and the shed Fragmentation: that reward is for the answer, so a player
// with no radio hardware or with capture off earns it in full. Learning a SIGIL is
// separate and costs one unspent SHAKE (buySigil): a captured handshake is the only
// thing a guardian will take for a piece of its own language. So the radio paces the
// Cant without gating any walk reward, keeping docs/ORIENTATION.md's split between the
// two consent axes.
//
// Three files, the usual split: the rules of the tongue are core/model/cant.h (no Game,
// no framebuffer), the pool is core/content/content_riddles.h (no rules), and this file
// is the lifecycle, the fluency roll and the payout. The pages are ShiboleetScreen
// (game/shiboleet_screen.h), drawn by core/ui/shiboleet_screen.cpp.

namespace mal {

namespace {
// A guardian's own seed. Mixed from the walk's position, so the encounter re-renders
// identically while it is on screen: the cipher is built once, in startShiboleet, and
// everything after reads it.
uint32_t cipherSeed(uint32_t rng, int area, int steps) {
    return rng ^ (static_cast<uint32_t>(area) << 24) ^ static_cast<uint32_t>(steps);
}
}  // namespace

namespace {
// The guardian's line pair for this meeting, bounds-safe. A row that authored fewer than
// kGuardianLines leaves null cells, and a null `cant`/`seen` would be a blank line rather
// than a crash, but the content gate rejects an under-filled row, so this is only a
// defensive floor.
const GuardianLine& guardianLine(const AreaDef& a, int i) {
    static const GuardianLine kNone{"", ""};
    if (i < 0 || i >= kGuardianLines) return kNone;
    const GuardianLine& l = a.guardian.lines[i];
    return (l.cant && l.seen) ? l : kNone;
}

// The same pair for how the meeting ENDED (GuardianOutcome, area_defs.h), and bounds-safe
// for the same reason: the content gate rejects an under-filled row, so an empty cell here
// draws a blank verdict rather than reading off the end of an area that shipped short.
const GuardianLine& guardianOutcomeLine(const AreaDef& a, GuardianOutcome o) {
    static const GuardianLine kNone{"", ""};
    const int i = static_cast<int>(o);
    if (i < 0 || i >= kGuardianOutcomes) return kNone;
    const GuardianLine& l = a.guardian.outcomes[i];
    return (l.cant && l.seen) ? l : kNone;
}
}  // namespace

const char* Game::guardianName() const {
    // The guardian of wherever the pet is standing. The DeepWeb has no AreaDef and so no
    // guardian of its own; nothing routes a dive here, and the clamp is what makes that
    // a dull fallback rather than a read off the end of the ladder.
    const int a = (walk_.sector >= 0 && walk_.sector < kAreaCount) ? walk_.sector : 0;
    return area(a).guardian.name;
}

void Game::startShiboleet() {
    // Grade the welcome first. All three bands start with the same hail, because the
    // pet cannot tell which band it is in until the guardian asks, refuses, or talks.
    //
    // A ROLL against fluency, not a threshold on it. The two edge bands are chances that
    // move in opposite directions as the Cant is learned (refusal thins out, a quiet
    // word thickens), so a pet with no sigils is still mostly asked, which gives the
    // ladder its first rung. See tunables.h.
    const int fluency = cantFluencyPct(op_.cant.sigils);
    const int affront = kShiboleetAffrontBasePct * (100 - fluency) / 100;
    const int boon = kShiboleetBoonMaxPct * fluency / 100;
    rng_ = rng_ * 1664525u + 1013904223u;
    const int welcome = static_cast<int>((rng_ >> 16) % 100);
    shibWelcome_ = (welcome < affront)          ? ShiboleetWelcome::Affront
                 : (welcome >= 100 - boon)      ? ShiboleetWelcome::Boon
                                                : ShiboleetWelcome::Riddle;
    shibReply_ = ShiboleetReply::Pending;
    shiboleet_.open();
    shibFlavor_[0] = '\0';
    shibVerdictLine_[0] = '\0';

    // Which of this guardian's greeting/demeanour pairs it meets the pet with.
    rng_ = rng_ * 1664525u + 1013904223u;
    shibLine_ = static_cast<int>((rng_ >> 16) % kGuardianLines);

    if (shibWelcome_ == ShiboleetWelcome::Riddle) {
        const int n = riddleCount();
        if (n <= 0) {
            // An empty pool has nothing to ask, so the meeting becomes a refusal,
            // which needs no content beyond the guardian.
            shibWelcome_ = ShiboleetWelcome::Affront;
        } else {
            rng_ = rng_ * 1664525u + 1013904223u;
            shibRiddle_ = static_cast<int>((rng_ >> 16) % static_cast<unsigned>(n));

            // Shuffle which authored reply sits on which shown row. The pool always
            // authors the true reply first (content_riddles.h), so without this the
            // answer would be row 0 every time, and with it, authoring position carries
            // no information at all.
            for (int i = 0; i < kRiddleReplies; ++i) shibOrder_[i] = static_cast<uint8_t>(i);
            for (int i = kRiddleReplies - 1; i > 0; --i) {
                rng_ = rng_ * 1664525u + 1013904223u;
                const int j = static_cast<int>((rng_ >> 16) % static_cast<unsigned>(i + 1));
                const uint8_t t = shibOrder_[i]; shibOrder_[i] = shibOrder_[j]; shibOrder_[j] = t;
            }
        }
    }

    // One cipher for the whole meeting: the greeting on the hail, the riddle and its
    // replies, and whatever the guardian says about the answer at the end are all drawn
    // in it, so a letter the pet cannot read is the same letter on every screen. Built
    // for every band and not just the asking one: a refusal is still spoken in the Cant.
    shibCipher_.build(op_.cant.sigils, cipherSeed(rng_, walk_.sector, walk_.steps));

    // The BODY. Reset per meeting and seeded off the same walk position the cipher is,
    // so a guardian re-renders identically while it is on screen and takes a different
    // shape the next time it is met. The cell is the screen's (shiboleet_screen.h): a
    // flock knows how big the box is and nothing about where the box is.
    guardianFlock_.seed(cipherSeed(rng_, walk_.sector, walk_.steps) | 1u);
    guardianFlock_.reset(kGuardianSwarmMarks, kGuardianCellW, kGuardianCellH);

    walk_.eventBeat = 0;   // start the hail's hold (game_core.cpp's tick)
    fxBeat_ = 0;
    nav_ = Nav::ShiboleetHail;
    dirty_ = true;
}

FlockMood Game::guardianFlockMood() const {
    // What the swarm is doing, which is the same question as what the guardian is doing.
    // Read from the nav, not a timer: deciding on the hail, waiting on the riddle board,
    // settled on the verdict. Each mood holds until the meeting moves on.
    if (nav_ == Nav::ShiboleetHail) return FlockMood::Watching;
    if (nav_ == Nav::Shiboleet) return FlockMood::Attending;
    if (nav_ == Nav::Combat) {
        // THE FIGHT, where the mood is the one readout the screen does not otherwise
        // have: a guardian has a Health gauge like any rival, and the swarm says the same
        // thing in the body. Beaten, it comes apart. Still standing, it is read off how
        // much of it is left: appraising while it is barely touched, fighting through
        // the middle, and drawn into a knot when it is nearly gone.
        //
        // A HEALTH BAND and not the hit clock, deliberately: hitBeat names the most
        // recent blow by EITHER fighter (RENDER_PIPELINE.md says what that did to
        // FX_CAMO), so a mood keyed to it would flicker with the exchange instead of
        // describing the creature. This holds until the fight actually changes it.
        if (fight_.combat.outcome() != Combat::Outcome::Ongoing)
            return fight_.combat.outcome() == Combat::Outcome::Win ? FlockMood::Scattering
                                                             : FlockMood::Watching;
        const int max = fight_.combat.enemy().maxHealth;
        const int pct = max > 0 ? fight_.combat.enemy().health * 100 / max : 100;
        if (pct <= kGuardianSwarmFailingPct) return FlockMood::Withdrawn;
        if (pct <= kGuardianSwarmPressedPct) return FlockMood::Agitated;
        return FlockMood::Watching;
    }
    switch (shiboleetOutcome()) {
        case GuardianOutcome::Pleased:    return FlockMood::Pleased;
        case GuardianOutcome::Displeased: return FlockMood::Agitated;
        case GuardianOutcome::Affront:    return FlockMood::Withdrawn;
        case GuardianOutcome::Boon:       return FlockMood::Open;
    }
    return FlockMood::Watching;
}

void Game::openShiboleetWelcome() {
    // What the hail was the front of. Two of the three bands were decided by the fluency
    // roll before the pet ever saw the screen, so they resolve straight into their
    // verdict; only the middle one has a question in it.
    switch (shibWelcome_) {
        case ShiboleetWelcome::Affront:
            // A refusal costs nothing and pays nothing, so the ledger slot carries the
            // REASON instead: the affront chance is fluency scaled down, and a pet that
            // reads none of the Cant is the pet this happens to. The line under it says
            // where the meeting goes from here, which is the part with a button on it.
            std::snprintf(shibVerdictLine_, sizeof(shibVerdictLine_),
                          "YOU READ %d OF %d SIGILS", sigilsKnown(), kCantSigils);
            std::snprintf(shibFlavor_, sizeof(shibFlavor_), "IT SETTLES THIS ITSELF");
            enterShiboleetVerdict();
            return;
        case ShiboleetWelcome::Boon:
            grantBoon();
            enterShiboleetVerdict();
            return;
        case ShiboleetWelcome::Riddle:
            shiboleet_.open();
            walk_.eventBeat = 0;   // start the ~15s answer clock (game_core.cpp's tick)
            fxBeat_ = 0;
            nav_ = Nav::Shiboleet;
            dirty_ = true;
            return;
    }
}

int Game::shiboleetTrueRow() const {
    // replies[0] is the true one; find where the shuffle put it.
    for (int i = 0; i < kRiddleReplies; ++i)
        if (shibOrder_[i] == 0) return i;
    return 0;
}

void Game::answerShiboleet(int row) {
    settleShiboleet(row == shiboleetTrueRow() ? ShiboleetReply::Answered : ShiboleetReply::Wrong);
}

void Game::settleShiboleet(ShiboleetReply reply) {
    if (shibReply_ != ShiboleetReply::Pending || reply == ShiboleetReply::Pending) return;
    if (reply != ShiboleetReply::Answered) {
        shibReply_ = reply;
        // The cost of getting it wrong, which is the same cost as not trying: a guardian
        // does not distinguish between an insult and a silence. The fight that follows
        // carries the rest of it, and the verdict screen is where the two are joined up.
        pet_.model.setHappiness(pet_.model.happiness() - kShiboleetLoseHappy);
        pet_.model.setFragmentation(pet_.model.fragmentation() + kShiboleetLoseFrag);
        // Names WHICH silence this was. The guardian's own reaction is the same either
        // way (GuardianOutcome::Displeased): this line says what the pet did, which the
        // player can act on next time.
        std::snprintf(shibFlavor_, sizeof(shibFlavor_), "%s",
                      reply == ShiboleetReply::Wrong ? "YOU ANSWERED WRONG" : "YOU SAID NOTHING");
        std::snprintf(shibVerdictLine_, sizeof(shibVerdictLine_), "-%d HAPPY  +%d FRAG",
                      kShiboleetLoseHappy, kShiboleetLoseFrag);
        markSaveDirty();
        enterShiboleetVerdict();
        return;
    }

    shibReply_ = ShiboleetReply::Answered;
    const int happyBefore = pet_.model.happiness();
    pet_.model.setHappiness(happyBefore + kShiboleetWinHappy);
    const int fragBefore = pet_.model.fragmentation();
    pet_.model.setFragmentation(fragBefore - kShiboleetWinFragCut);
    // Reports what actually moved, so a pet already at full Happiness or already clean
    // is never told it gained more than it had room for. grantBoon's line does the
    // same.
    std::snprintf(shibVerdictLine_, sizeof(shibVerdictLine_), "+%d HAPPY  -%d FRAG",
                  pet_.model.happiness() - happyBefore, fragBefore - pet_.model.fragmentation());

    // The sigil is the SEPARATE half, and the only part a shake buys. A pet that answered
    // correctly with an empty purse still keeps everything above: it only lacked a SHAKE
    // to trade for the sigil.
    if (buySigil()) {
        std::snprintf(shibFlavor_, sizeof(shibFlavor_), "LEARNED A SIGIL - %d/%d",
                      sigilsKnown(), kCantSigils);
    } else if (sigilsKnown() >= kCantSigils) {
        std::snprintf(shibFlavor_, sizeof(shibFlavor_), "SPEAKS THE WHOLE CANT");
    } else {
        // Says what is missing, not merely that something is: the purse is the one part
        // of this the player can go and do something about.
        std::snprintf(shibFlavor_, sizeof(shibFlavor_), "ANSWERED - NEEDS A SHAKE");
    }
    markSaveDirty();
    enterShiboleetVerdict();
}

GuardianOutcome Game::shiboleetOutcome() const {
    // The fluency band and the reply folded onto the ONE axis the content is authored
    // against. A riddle that was never reached keeps its band's outcome, which is what
    // makes an affront and a boon expressible as things the guardian DID.
    if (shibWelcome_ == ShiboleetWelcome::Affront) return GuardianOutcome::Affront;
    if (shibWelcome_ == ShiboleetWelcome::Boon) return GuardianOutcome::Boon;
    return shibReply_ == ShiboleetReply::Answered ? GuardianOutcome::Pleased
                                                   : GuardianOutcome::Displeased;
}

bool Game::shiboleetVerdictFights() const {
    // The two outcomes the guardian answers itself. Read by the screen to name the
    // button, so a player is never told "B CONTINUE" and handed a boss.
    const GuardianOutcome o = shiboleetOutcome();
    return o == GuardianOutcome::Displeased || o == GuardianOutcome::Affront;
}

void Game::shiboleetOutcomeSpeech(char* out, int cap) const {
    if (!out || cap <= 0) return;
    out[0] = '\0';
    const int a = (walk_.sector >= 0 && walk_.sector < kAreaCount) ? walk_.sector : 0;
    // The SAME cipher as the greeting and the riddle. The verdict stays in the Cant, so
    // it is exactly as legible as the question was.
    shibCipher_.applyTo(guardianOutcomeLine(area(a), shiboleetOutcome()).cant, out, cap);
}

const char* Game::shiboleetOutcomeSeen() const {
    const int a = (walk_.sector >= 0 && walk_.sector < kAreaCount) ? walk_.sector : 0;
    return guardianOutcomeLine(area(a), shiboleetOutcome()).seen;
}

void Game::enterShiboleetVerdict() {
    walk_.eventBeat = 0;   // start the verdict's hold (game_core.cpp's tick)
    fxBeat_ = 0;
    nav_ = Nav::ShiboleetVerdict;
    dirty_ = true;
}

void Game::finishShiboleet() {
    // Where a meeting actually ends, and the only place it does. A displeased or refusing
    // guardian answers for itself; anything else hands back to the walk carrying the
    // consequence on the flavor line, so the encounter is still legible one screen later.
    if (shiboleetVerdictFights()) { startGuardianCombat(); return; }
    if (shibFlavor_[0]) {
        std::strncpy(walk_.flavor, shibFlavor_, sizeof(walk_.flavor) - 1);
        walk_.flavor[sizeof(walk_.flavor) - 1] = '\0';
    }
    returnToExplore();
}

bool Game::buySigil() {
    if (sigilsKnown() >= kCantSigils) return false;   // nothing left to learn
    if (shakesUnspent() <= 0) return false;           // nothing to trade
    ++op_.cant.shakesSpent;
    op_.cant.sigils = learnSigil(op_.cant.sigils);
    log_.push(LogEventType::ItemGained, "LEARNED A SIGIL");
    markSaveDirty();
    return true;
}

void Game::grantBoon() {
    // What fluency is FOR. Two shapes, because a guardian that only ever handed out the
    // same lump would stop being a character: most of the time it simply sits with the
    // pet a while, and sometimes it sends something along with it.
    //
    // Pays here without handing back, because finishShiboleet is the only exit, so
    // the pet reads what it was given on the verdict before the walk resumes.
    rng_ = rng_ * 1664525u + 1013904223u;
    const bool escort = static_cast<int>((rng_ >> 16) % 100) < kShiboleetBoonEscortPct;
    if (escort) {
        allyBuffBattlesLeft_ = kShiboleetEscortBattles;
        std::snprintf(shibFlavor_, sizeof(shibFlavor_), "ESCORTED X%d",
                      kShiboleetEscortBattles);
        std::snprintf(shibVerdictLine_, sizeof(shibVerdictLine_), "%d BATTLES AT YOUR SIDE",
                      kShiboleetEscortBattles);
    } else {
        const int happyBefore = pet_.model.happiness();
        pet_.model.setHappiness(happyBefore + kShiboleetBoonHappy);
        const int fragBefore = pet_.model.fragmentation();
        pet_.model.setFragmentation(fragBefore - kShiboleetBoonFragCut);
        // Reports the Happiness and Fragmentation that actually moved, so a pet already
        // near clean is never told it lost more than it had. resolveSafeRestEvent's
        // line does the same.
        std::snprintf(shibFlavor_, sizeof(shibFlavor_), "A QUIET WORD (-%d FRAG)",
                      fragBefore - pet_.model.fragmentation());
        std::snprintf(shibVerdictLine_, sizeof(shibVerdictLine_), "+%d HAPPY  -%d FRAG",
                      pet_.model.happiness() - happyBefore,
                      fragBefore - pet_.model.fragmentation());
    }
    markSaveDirty();
}

void Game::startGuardianCombat() {
    // Re-home the body into the FIGHTER seat before the stage is drawn. Same flock and
    // same seed (it is the creature the pet was just talking to), but the stage's seat is
    // a different box from the meeting screens' (kSwarmSeatW/H, core/ui/combat_screen.h),
    // and a flock knows only its box's size.
    guardianFlock_.reset(kGuardianSwarmMarks, kSwarmSeatW, kSwarmSeatH);

    // The guardian's fight, on the ordinary wild path, so a loss ends the walk like
    // any other, and the post-encounter readout, streak and loot rolls work as usual.
    // What makes it a guardian is the enemy (guardianEnemy, combat_factory.cpp), not a
    // second set of combat rules.
    startWildCombat(guardianEnemy(walk_.sector, walk_.sub), /*forceEnemyFirst=*/false);
}

void Game::shiboleetRiddleText(char* out, int cap) const {
    if (!out || cap <= 0) return;
    out[0] = '\0';
    if (shibRiddle_ < 0 || shibRiddle_ >= riddleCount()) return;
    shibCipher_.applyTo(riddles()[shibRiddle_].text, out, cap);
}

void Game::shiboleetGreeting(char* out, int cap) const {
    if (!out || cap <= 0) return;
    out[0] = '\0';
    const int a = (walk_.sector >= 0 && walk_.sector < kAreaCount) ? walk_.sector : 0;
    // The SAME cipher the riddle is drawn in, deliberately: a letter the pet cannot read
    // has to be unreadable everywhere on the screen, or the greeting would quietly leak
    // the mapping the riddle is asking it to work without.
    shibCipher_.applyTo(guardianLine(area(a), shibLine_).cant, out, cap);
}

const char* Game::guardianDemeanour() const {
    const int a = (walk_.sector >= 0 && walk_.sector < kAreaCount) ? walk_.sector : 0;
    return guardianLine(area(a), shibLine_).seen;
}

void Game::shiboleetReplyText(int row, char* out, int cap) const {
    if (!out || cap <= 0) return;
    out[0] = '\0';
    if (row < 0 || row >= kRiddleReplies) return;
    if (shibRiddle_ < 0 || shibRiddle_ >= riddleCount()) return;
    shibCipher_.applyTo(riddles()[shibRiddle_].replies[shibOrder_[row]], out, cap);
}

}  // namespace mal
