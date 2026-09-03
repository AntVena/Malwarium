#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/content/areas/area_defs.h"
#include "core/content/content_riddles.h"
#include "core/model/cant.h"
#include "core/ui/shibboleth_screen.h"   // the swarm cell the guardian's flock is reset into

// game_shibboleth.cpp — THE SHIBBOLETH, the guardian encounter.
//
// Something has been watching every network in the area for a very long time, and when
// the radio has nothing new to hand the pet, it is what the walk finds instead
// (game_net.cpp routes there on a dry sighting queue). It does not attack on sight. It
// SPEAKS — and it speaks the CANT, which the pet does not.
//
// What it does next is graded on how much of that tongue the pet can read
// (cantFluencyPct, core/model/cant.h), in three bands:
//
//   AFFRONT — under kShibbolethAffrontBelowPct. It will not hear an illiterate pet out,
//             and its fight follows from the refusal.
//   RIDDLE  — the middle, and where nearly all of the game lives. A riddle drawn in the
//             Cant with three replies drawn the same way. Answer it and the guardian is
//             satisfied; answer wrong, or say nothing for kShibbolethReplyHoldBeats, and
//             it takes the silence for an answer and the fight starts.
//   BOON    — over kShibbolethBoonAbovePct. Fluent enough that the two simply talk. No
//             riddle, no fight; the pet comes away rested, or with an escort.
//
// THREE SCREENS, AND THE RIDDLE IS THE MIDDLE ONE. A meeting is a HAIL
// (Nav::ShibbolethHail) — the thing arrives, does something the pet can see, and speaks —
// then whatever the band calls for, then a VERDICT (Nav::ShibbolethVerdict): what the
// guardian made of it, what that paid or cost, and what happens next. Every band passes
// through both, so an affront is a refusal the player watched rather than a boss that
// appeared, and a lost riddle is something the guardian DID rather than a fight with no
// stated cause. The bracketing screens are also where the encounter explains itself at
// all — the Cant strip, the sigil count and the purse are read on the hail, and a sigil
// earned lights its own cell on the verdict.
//
// WHAT A WIN ACTUALLY BUYS, and why it is two things. Answering correctly always pays
// the Happiness and the shed Fragmentation — that reward is for the ANSWER, and a player
// with no radio hardware, or with capture switched off, earns it in full. Learning a
// SIGIL is separate and costs one unspent SHAKE (buySigil): a captured handshake is the
// only thing a guardian will take for a piece of its own language. So the Cant is paced
// by the radio without any of the walk's rewards being gated behind it — which is the
// line docs/ORIENTATION.md draws between the two consent axes, kept here too.
//
// Three files, the usual split: the rules of the tongue are core/model/cant.h (no Game,
// no framebuffer), the pool is core/content/content_riddles.h (no rules), and this file
// is the lifecycle, the fluency roll and the payout. The screen is
// core/ui/shibboleth_screen.cpp.

namespace mal {

namespace {
// A guardian's own seed. Mixed from the walk's position rather than the clock so the
// same encounter re-renders identically for as long as it is on screen — the cipher is
// built once, in startShibboleth, and everything after reads it.
uint32_t cipherSeed(uint32_t rng, int area, int steps) {
    return rng ^ (static_cast<uint32_t>(area) << 24) ^ static_cast<uint32_t>(steps);
}
}  // namespace

namespace {
// The guardian's line pair for this meeting, bounds-safe. A row that authored fewer than
// kGuardianLines leaves null cells, and a null `cant`/`seen` would be a blank line rather
// than a crash — but the content gate rejects an under-filled row, so this is the
// defensive floor and not a supported shape.
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
    const int a = (exploreSector_ >= 0 && exploreSector_ < kAreaCount) ? exploreSector_ : 0;
    return area(a).guardian.name;
}

void Game::startShibboleth() {
    // Grade the welcome first. All three bands then meet the pet the SAME way — a hail —
    // because the pet cannot tell which one it landed in until the thing in front of it
    // decides to ask, refuse, or talk.
    //
    // A ROLL against fluency, not a threshold on it. The two edge bands are chances that
    // move in opposite directions as the Cant is learned — refusal thins out, a quiet
    // word thickens — so a pet with no sigils at all is still mostly ASKED, which is the
    // only reason the ladder has a first rung. See tunables.h.
    const int fluency = cantFluencyPct(cantSigils_);
    const int affront = kShibbolethAffrontBasePct * (100 - fluency) / 100;
    const int boon = kShibbolethBoonMaxPct * fluency / 100;
    rng_ = rng_ * 1664525u + 1013904223u;
    const int welcome = static_cast<int>((rng_ >> 16) % 100);
    shibWelcome_ = (welcome < affront)          ? ShibbolethWelcome::Affront
                 : (welcome >= 100 - boon)      ? ShibbolethWelcome::Boon
                                                : ShibbolethWelcome::Riddle;
    shibReply_ = ShibbolethReply::Pending;
    shibRow_ = 0;
    shibFlavor_[0] = '\0';
    shibVerdictLine_[0] = '\0';

    // Which of this guardian's greeting/demeanour pairs it meets the pet with.
    rng_ = rng_ * 1664525u + 1013904223u;
    shibLine_ = static_cast<int>((rng_ >> 16) % kGuardianLines);

    if (shibWelcome_ == ShibbolethWelcome::Riddle) {
        const int n = riddleCount();
        if (n <= 0) {
            // An empty pool has nothing to ask, so the meeting is a refusal instead —
            // which is the one band that needs no content beyond the guardian itself.
            shibWelcome_ = ShibbolethWelcome::Affront;
        } else {
            rng_ = rng_ * 1664525u + 1013904223u;
            shibRiddle_ = static_cast<int>((rng_ >> 16) % static_cast<unsigned>(n));

            // Shuffle which authored reply sits on which shown row. The pool always
            // authors the true reply first (content_riddles.h), so without this the
            // answer would be row 0 every time — and with it, authoring position carries
            // no information at all.
            for (int i = 0; i < kRiddleReplies; ++i) shibOrder_[i] = static_cast<uint8_t>(i);
            for (int i = kRiddleReplies - 1; i > 0; --i) {
                rng_ = rng_ * 1664525u + 1013904223u;
                const int j = static_cast<int>((rng_ >> 16) % static_cast<unsigned>(i + 1));
                const uint8_t t = shibOrder_[i]; shibOrder_[i] = shibOrder_[j]; shibOrder_[j] = t;
            }
        }
    }

    // One cipher for the WHOLE meeting — the greeting on the hail, the riddle and its
    // replies, and whatever the guardian says about the answer at the end are all drawn
    // in it, so a letter the pet cannot read is the same letter on every screen. Built
    // for every band and not just the asking one: a refusal is still spoken in the Cant.
    shibCipher_.build(cantSigils_, cipherSeed(rng_, exploreSector_, exploreSteps_));

    // The BODY. Reset per meeting and seeded off the same walk position the cipher is,
    // so a guardian re-renders identically for as long as it is on screen and is a
    // different shape the next time it is met — which is the whole of what a swarm has
    // instead of a sheet. The cell is the screen's (shibboleth_screen.h): a flock knows
    // how big the box is and nothing about where the box is.
    guardianFlock_.seed(cipherSeed(rng_, exploreSector_, exploreSteps_) | 1u);
    guardianFlock_.reset(kGuardianSwarmMarks, kGuardianCellW, kGuardianCellH);

    exploreEventBeat_ = 0;   // start the hail's hold (game_core.cpp's tick)
    fxBeat_ = 0;
    nav_ = Nav::ShibbolethHail;
    dirty_ = true;
}

FlockMood Game::guardianFlockMood() const {
    // What the swarm is doing, which is the same question as what the guardian is doing.
    // Read off the NAV rather than off a timer: the hail is a thing deciding, the riddle
    // board is a thing waiting on an answer, and the verdict is a thing that has made up
    // its mind — and each holds until the meeting moves on, which is what makes this a
    // state and not an effect running out.
    if (nav_ == Nav::ShibbolethHail) return FlockMood::Watching;
    if (nav_ == Nav::Shibboleth) return FlockMood::Attending;
    switch (shibbolethOutcome()) {
        case GuardianOutcome::Pleased:    return FlockMood::Pleased;
        case GuardianOutcome::Displeased: return FlockMood::Agitated;
        case GuardianOutcome::Affront:    return FlockMood::Withdrawn;
        case GuardianOutcome::Boon:       return FlockMood::Open;
    }
    return FlockMood::Watching;
}

void Game::onShibbolethHail(const ButtonEvent& ev) {
    // A REVEAL, like the Wi-Fi event's: B plays it forward, and any press restarts the
    // hold so a player still reading a guardian's greeting is not hurried past it. There
    // is nothing to cancel — the meeting has already happened.
    exploreEventBeat_ = 0;
    dirty_ = true;
    if (ev.button == Button::B) openShibbolethWelcome();
}

void Game::openShibbolethWelcome() {
    // What the hail was the front of. Two of the three bands were decided by the fluency
    // roll before the pet ever saw the screen, so they resolve straight into their
    // verdict; only the middle one has a question in it.
    switch (shibWelcome_) {
        case ShibbolethWelcome::Affront:
            // A refusal costs nothing and pays nothing, so the ledger slot carries the
            // REASON instead: the affront chance is fluency scaled down, and a pet that
            // reads none of the Cant is the pet this happens to. The line under it says
            // where the meeting goes from here, which is the part with a button on it.
            std::snprintf(shibVerdictLine_, sizeof(shibVerdictLine_),
                          "YOU READ %d OF %d SIGILS", sigilsKnown(), kCantSigils);
            std::snprintf(shibFlavor_, sizeof(shibFlavor_), "IT SETTLES THIS ITSELF");
            enterShibbolethVerdict();
            return;
        case ShibbolethWelcome::Boon:
            grantBoon();
            enterShibbolethVerdict();
            return;
        case ShibbolethWelcome::Riddle:
            shibRow_ = 0;
            exploreEventBeat_ = 0;   // start the ~15s answer clock (game_core.cpp's tick)
            fxBeat_ = 0;
            nav_ = Nav::Shibboleth;
            dirty_ = true;
            return;
    }
}

void Game::onShibboleth(const ButtonEvent& ev) {
    // A cursor list, the same contract every other picker keeps: A steps the row, B
    // commits it, C backs out. C is NOT a way out of the encounter — backing away from
    // something that asked you a question is its own answer, and it is the wrong one —
    // so it resolves as an unanswered riddle rather than returning to the walk.
    if (shibReply_ != ShibbolethReply::Pending) return;
    if (ev.button == Button::A) {
        shibRow_ = (shibRow_ + 1) % kRiddleReplies;
        exploreEventBeat_ = 0;   // a player who is reading is not being rushed
        dirty_ = true;
        return;
    }
    if (ev.button == Button::B) { answerShibboleth(/*answered=*/true); return; }
    if (ev.button == Button::C) { answerShibboleth(/*answered=*/false); return; }
}

int Game::shibbolethTrueRow() const {
    // replies[0] is the true one; find where the shuffle put it.
    for (int i = 0; i < kRiddleReplies; ++i)
        if (shibOrder_[i] == 0) return i;
    return 0;
}

void Game::answerShibboleth(bool answered) {
    if (shibReply_ != ShibbolethReply::Pending) return;
    const bool right = answered && shibRow_ == shibbolethTrueRow();
    if (!right) {
        shibReply_ = answered ? ShibbolethReply::Wrong : ShibbolethReply::Unanswered;
        // The cost of getting it wrong, which is the same cost as not trying: a guardian
        // does not distinguish between an insult and a silence. The fight that follows
        // carries the rest of it, and the verdict screen is where the two are joined up.
        model_.setHappiness(model_.happiness() - kShibbolethLoseHappy);
        model_.setFragmentation(model_.fragmentation() + kShibbolethLoseFrag);
        // Names WHICH silence this was. The guardian's own reaction is the same either
        // way (GuardianOutcome::Displeased) — this line is the engine saying what the pet
        // actually did, which is the half a player can act on next time.
        std::snprintf(shibFlavor_, sizeof(shibFlavor_), "%s",
                      answered ? "YOU ANSWERED WRONG" : "YOU SAID NOTHING");
        std::snprintf(shibVerdictLine_, sizeof(shibVerdictLine_), "-%d HAPPY  +%d FRAG",
                      kShibbolethLoseHappy, kShibbolethLoseFrag);
        markSaveDirty();
        enterShibbolethVerdict();
        return;
    }

    shibReply_ = ShibbolethReply::Answered;
    const int happyBefore = model_.happiness();
    model_.setHappiness(happyBefore + kShibbolethWinHappy);
    const int fragBefore = model_.fragmentation();
    model_.setFragmentation(fragBefore - kShibbolethWinFragCut);
    // Reports what the pet ACTUALLY moved rather than the tunables, so a pet already at
    // full Happiness or already clean is never told it gained what it had no room for —
    // the same honesty grantBoon's line keeps.
    std::snprintf(shibVerdictLine_, sizeof(shibVerdictLine_), "+%d HAPPY  -%d FRAG",
                  model_.happiness() - happyBefore, fragBefore - model_.fragmentation());

    // The sigil is the SEPARATE half, and the only part a shake buys. A pet that answered
    // correctly with an empty purse still keeps everything above — it understood the
    // question, it just had nothing to trade for the word.
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
    enterShibbolethVerdict();
}

GuardianOutcome Game::shibbolethOutcome() const {
    // The fluency band and the reply folded onto the ONE axis the content is authored
    // against. A riddle that was never reached keeps its band's outcome, which is what
    // makes an affront and a boon expressible as things the guardian DID.
    if (shibWelcome_ == ShibbolethWelcome::Affront) return GuardianOutcome::Affront;
    if (shibWelcome_ == ShibbolethWelcome::Boon) return GuardianOutcome::Boon;
    return shibReply_ == ShibbolethReply::Answered ? GuardianOutcome::Pleased
                                                   : GuardianOutcome::Displeased;
}

bool Game::shibbolethVerdictFights() const {
    // The two outcomes the guardian answers itself. Read by the screen to name the
    // button, so a player is never told "B CONTINUE" and handed a boss.
    const GuardianOutcome o = shibbolethOutcome();
    return o == GuardianOutcome::Displeased || o == GuardianOutcome::Affront;
}

void Game::shibbolethOutcomeSpeech(char* out, int cap) const {
    if (!out || cap <= 0) return;
    out[0] = '\0';
    const int a = (exploreSector_ >= 0 && exploreSector_ < kAreaCount) ? exploreSector_ : 0;
    // The SAME cipher as the greeting and the riddle. A guardian does not switch to the
    // 'net's alphabet because the conversation is over — what it makes of the answer is
    // as legible as the question was, which is what makes a fluent pet's verdict land.
    shibCipher_.applyTo(guardianOutcomeLine(area(a), shibbolethOutcome()).cant, out, cap);
}

const char* Game::shibbolethOutcomeSeen() const {
    const int a = (exploreSector_ >= 0 && exploreSector_ < kAreaCount) ? exploreSector_ : 0;
    return guardianOutcomeLine(area(a), shibbolethOutcome()).seen;
}

void Game::enterShibbolethVerdict() {
    exploreEventBeat_ = 0;   // start the verdict's hold (game_core.cpp's tick)
    fxBeat_ = 0;
    nav_ = Nav::ShibbolethVerdict;
    dirty_ = true;
}

void Game::onShibbolethVerdict(const ButtonEvent& ev) {
    // A REVEAL again: B plays out what the guardian decided, any press restarts the hold.
    // Nothing here is a choice — the choice was the reply, and this is its consequence.
    exploreEventBeat_ = 0;
    dirty_ = true;
    if (ev.button == Button::B) finishShibboleth();
}

void Game::finishShibboleth() {
    // Where a meeting actually ends, and the only place it does. A displeased or refusing
    // guardian answers for itself; anything else hands back to the walk carrying the
    // consequence on the flavor line, so the encounter is still legible one screen later.
    if (shibbolethVerdictFights()) { startGuardianCombat(); return; }
    if (shibFlavor_[0]) {
        std::strncpy(exploreFlavor_, shibFlavor_, sizeof(exploreFlavor_) - 1);
        exploreFlavor_[sizeof(exploreFlavor_) - 1] = '\0';
    }
    returnToExplore();
}

bool Game::buySigil() {
    if (sigilsKnown() >= kCantSigils) return false;   // nothing left to learn
    if (shakesUnspent() <= 0) return false;           // nothing to trade
    ++shakesSpent_;
    cantSigils_ = learnSigil(cantSigils_);
    log_.push(LogEventType::ItemGained, "LEARNED A SIGIL");
    markSaveDirty();
    return true;
}

void Game::grantBoon() {
    // What fluency is FOR. Two shapes, because a guardian that only ever handed out the
    // same lump would stop being a character: most of the time it simply sits with the
    // pet a while, and sometimes it sends something along with it.
    //
    // Pays here and hands back nowhere — finishShibboleth is the one exit, so the pet
    // reads what it was given on the verdict before the walk resumes.
    rng_ = rng_ * 1664525u + 1013904223u;
    const bool escort = static_cast<int>((rng_ >> 16) % 100) < kShibbolethBoonEscortPct;
    if (escort) {
        allyBuffBattlesLeft_ = kShibbolethEscortBattles;
        std::snprintf(shibFlavor_, sizeof(shibFlavor_), "ESCORTED X%d",
                      kShibbolethEscortBattles);
        std::snprintf(shibVerdictLine_, sizeof(shibVerdictLine_), "%d BATTLES AT YOUR SIDE",
                      kShibbolethEscortBattles);
    } else {
        const int happyBefore = model_.happiness();
        model_.setHappiness(happyBefore + kShibbolethBoonHappy);
        const int fragBefore = model_.fragmentation();
        model_.setFragmentation(fragBefore - kShibbolethBoonFragCut);
        // Reports the Happiness and Fragmentation actually moved rather than the
        // tunables, so a pet already near clean is never told it lost more than it had —
        // the same honesty resolveSafeRestEvent's line keeps.
        std::snprintf(shibFlavor_, sizeof(shibFlavor_), "A QUIET WORD (-%d FRAG)",
                      fragBefore - model_.fragmentation());
        std::snprintf(shibVerdictLine_, sizeof(shibVerdictLine_), "+%d HAPPY  -%d FRAG",
                      model_.happiness() - happyBefore,
                      fragBefore - model_.fragmentation());
    }
    markSaveDirty();
}

void Game::startGuardianCombat() {
    // The guardian's fight, on the ordinary wild path — so a loss ends the walk exactly
    // as any other loss does, and the post-encounter readout, the streak and the loot
    // rolls all behave as they always have. What makes it a guardian is the enemy
    // (guardianEnemy, combat_factory.cpp), not a second set of combat rules.
    encounterEnemy_ = guardianEnemy(exploreSector_, exploreSub_);
    startWildCombat(/*forceEnemyFirst=*/false);
}

void Game::shibbolethRiddleText(char* out, int cap) const {
    if (!out || cap <= 0) return;
    out[0] = '\0';
    if (shibRiddle_ < 0 || shibRiddle_ >= riddleCount()) return;
    shibCipher_.applyTo(riddles()[shibRiddle_].text, out, cap);
}

void Game::shibbolethGreeting(char* out, int cap) const {
    if (!out || cap <= 0) return;
    out[0] = '\0';
    const int a = (exploreSector_ >= 0 && exploreSector_ < kAreaCount) ? exploreSector_ : 0;
    // The SAME cipher the riddle is drawn in, deliberately: a letter the pet cannot read
    // has to be unreadable everywhere on the screen, or the greeting would quietly leak
    // the mapping the riddle is asking it to work without.
    shibCipher_.applyTo(guardianLine(area(a), shibLine_).cant, out, cap);
}

const char* Game::guardianDemeanour() const {
    const int a = (exploreSector_ >= 0 && exploreSector_ < kAreaCount) ? exploreSector_ : 0;
    return guardianLine(area(a), shibLine_).seen;
}

void Game::shibbolethReplyText(int row, char* out, int cap) const {
    if (!out || cap <= 0) return;
    out[0] = '\0';
    if (row < 0 || row >= kRiddleReplies) return;
    if (shibRiddle_ < 0 || shibRiddle_ >= riddleCount()) return;
    shibCipher_.applyTo(riddles()[shibRiddle_].replies[shibOrder_[row]], out, cap);
}

}  // namespace mal
