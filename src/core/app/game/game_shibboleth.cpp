#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/content/areas/area_defs.h"
#include "core/content/content_riddles.h"
#include "core/model/cant.h"

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
//   AFFRONT — under kShibbolethAffrontBelowPct. It will not hear an illiterate pet out.
//             Straight into its fight, no screen.
//   RIDDLE  — the middle, and where nearly all of the game lives. A riddle drawn in the
//             Cant with three replies drawn the same way. Answer it and the guardian is
//             satisfied; answer wrong, or say nothing for kShibbolethReplyHoldBeats, and
//             it takes the silence for an answer and the fight starts.
//   BOON    — over kShibbolethBoonAbovePct. Fluent enough that the two simply talk. No
//             riddle, no fight; the pet comes away rested, or with an escort.
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

const char* Game::guardianName() const {
    // The guardian of wherever the pet is standing. The DeepWeb has no AreaDef and so no
    // guardian of its own; nothing routes a dive here, and the clamp is what makes that
    // a dull fallback rather than a read off the end of the ladder.
    const int a = (exploreSector_ >= 0 && exploreSector_ < kAreaCount) ? exploreSector_ : 0;
    return area(a).guardian.name;
}

void Game::startShibboleth() {
    // Grade the welcome first: two of the three bands never open a screen at all.
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
    if (welcome < affront) {
        shibWelcome_ = ShibbolethWelcome::Affront;
        shibReply_ = ShibbolethReply::Pending;
        std::snprintf(shibFlavor_, sizeof(shibFlavor_), "%s TAKES AFFRONT", guardianName());
        startGuardianCombat();
        return;
    }
    if (welcome >= 100 - boon) {
        shibWelcome_ = ShibbolethWelcome::Boon;
        shibReply_ = ShibbolethReply::Pending;
        grantBoon();
        return;
    }

    shibWelcome_ = ShibbolethWelcome::Riddle;
    shibReply_ = ShibbolethReply::Pending;
    shibRow_ = 0;

    const int n = riddleCount();
    if (n <= 0) { startGuardianCombat(); return; }   // empty pool — nothing to ask
    rng_ = rng_ * 1664525u + 1013904223u;
    shibRiddle_ = static_cast<int>((rng_ >> 16) % static_cast<unsigned>(n));

    // Shuffle which authored reply sits on which shown row. The pool always authors the
    // true reply first (content_riddles.h), so without this the answer would be row 0
    // every time — and with it, authoring position carries no information at all.
    for (int i = 0; i < kRiddleReplies; ++i) shibOrder_[i] = static_cast<uint8_t>(i);
    for (int i = kRiddleReplies - 1; i > 0; --i) {
        rng_ = rng_ * 1664525u + 1013904223u;
        const int j = static_cast<int>((rng_ >> 16) % static_cast<unsigned>(i + 1));
        const uint8_t t = shibOrder_[i]; shibOrder_[i] = shibOrder_[j]; shibOrder_[j] = t;
    }

    // One cipher for the whole encounter — the riddle and all three replies are drawn in
    // it, so a letter the pet cannot read is the same letter everywhere on the screen.
    // That consistency is what makes a partly-learned Cant usable rather than noise.
    shibCipher_.build(cantSigils_, cipherSeed(rng_, exploreSector_, exploreSteps_));

    exploreEventBeat_ = 0;   // start the ~15s answer clock (game_core.cpp's tick)
    fxBeat_ = 0;
    nav_ = Nav::Shibboleth;
    dirty_ = true;
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
        // carries the rest of it.
        model_.setHappiness(model_.happiness() - kShibbolethLoseHappy);
        model_.setFragmentation(model_.fragmentation() + kShibbolethLoseFrag);
        std::snprintf(shibFlavor_, sizeof(shibFlavor_), "%s IS NOT ANSWERED",
                      guardianName());
        markSaveDirty();
        startGuardianCombat();
        return;
    }

    shibReply_ = ShibbolethReply::Answered;
    model_.setHappiness(model_.happiness() + kShibbolethWinHappy);
    const int fragBefore = model_.fragmentation();
    model_.setFragmentation(fragBefore - kShibbolethWinFragCut);

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
    rng_ = rng_ * 1664525u + 1013904223u;
    const bool escort = static_cast<int>((rng_ >> 16) % 100) < kShibbolethBoonEscortPct;
    if (escort) {
        allyBuffBattlesLeft_ = kShibbolethEscortBattles;
        std::snprintf(exploreFlavor_, sizeof(exploreFlavor_), "ESCORTED X%d",
                      kShibbolethEscortBattles);
    } else {
        model_.setHappiness(model_.happiness() + kShibbolethBoonHappy);
        const int before = model_.fragmentation();
        model_.setFragmentation(before - kShibbolethBoonFragCut);
        // Reports the Fragmentation actually shed rather than the tunable, so a pet
        // already near clean is never told it lost more than it had — the same honesty
        // resolveSafeRestEvent's line keeps.
        std::snprintf(exploreFlavor_, sizeof(exploreFlavor_), "A QUIET WORD (-%d FRAG)",
                      before - model_.fragmentation());
    }
    markSaveDirty();
    returnToExplore();
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

void Game::shibbolethReplyText(int row, char* out, int cap) const {
    if (!out || cap <= 0) return;
    out[0] = '\0';
    if (row < 0 || row >= kRiddleReplies) return;
    if (shibRiddle_ < 0 || shibRiddle_ >= riddleCount()) return;
    shibCipher_.applyTo(riddles()[shibRiddle_].replies[shibOrder_[row]], out, cap);
}

}  // namespace mal
