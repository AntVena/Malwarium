#include "core/app/game.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/model/pvp_battle.h"
#include "core/render/canvas.h"
#include "core/render/font5x7.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/theme.h"
#include "core/ui/widgets.h"
#include "generated/assets.h"

// game_pvp.cpp — the Hacker-face LINK slot: 1v1 duels with an operator standing in
// front of you.
//
// PEERS answers "who is around"; LINK is where you do something about it. The whole
// feature is a three-frame handshake and then silence:
//
//   1. B on a LIVE peer sends an INVITE carrying this device's fighter spec.
//   2. Their human presses B, which sends an ACCEPT carrying theirs. Consent is
//      explicit and human, never automatic — an incoming challenge is only even
//      considered while their LINK screen is open, and anything else replies BUSY.
//   3. Whichever device has the lower radio MAC is the HOST (pvpHostIsLocal). It picks
//      the seed and sends START. Both devices then build the same two fighters and step
//      the same seeded Combat locally (core/model/pvp_battle.h), so the fight plays out
//      on both screens at once with the same result and not one turn crosses the air.
//
// The host occupies Combat's player_ slot on BOTH devices, so the guest's own pet is
// the enemy_ row — a display fact the combat screen relabels (CombatSides), never a
// resolution difference. START also carries pvpCommitHash over the seed and both specs;
// a guest that computes a different one refuses rather than playing a fight its
// opponent isn't playing.
//
// NO STAKES, AND THE WIRE FORMAT DEPENDS ON THAT. Nothing here is saved, no XP, Bits,
// Fragmentation or care signal moves, and no Exploit is available mid-fight (the A+C
// picker pauses the fight for a human, and there is no way to pause the other device's
// copy). Duel frames are unauthenticated (core/net/pvp_link.h): the session id makes a
// stale frame inert but proves nothing, which is only acceptable while a win is worth
// nothing. Anything that pays out needs signing first.

namespace mal {

namespace {
constexpr int kMargin = 10;
constexpr int kLineH = kFontH + 5;
// A challengeable row is tag / pet — two lines, so the list clears the header rule with
// room for the status band and the hint band underneath.
constexpr int kRowH = kLineH * 3 + 6;   // tag · pet+stage · crew
constexpr int kVisibleRows = 3;
constexpr int kMaxRows = 32;
}  // namespace

// --- Identity ----------------------------------------------------------------

void Game::setLocalRadioMac(const uint8_t* mac) {
    localRadioKey_ = packPeerKey(mac);
}

bool Game::buildPvpFighter(PvpFighter* out) const {
    if (!out || !pet_) return false;
    PvpFighter f;
    std::strncpy(f.tag, hackerTag_, sizeof(f.tag) - 1);
    std::strncpy(f.creatureId, pet_->id, sizeof(f.creatureId) - 1);

    // Equipped kit by id. Only the slots this pet's stage has actually UNLOCKED are
    // offered — an equipped-but-locked move is inert in combat anyway, and sending it
    // would invite the two devices to disagree about which slots count.
    const int slots = MoveLoadout::slotsForStage(pet_->stage);
    for (int i = 0; i < slots && i < kMaxMoveSlots; ++i)
        if (const char* id = moveLoadout_.equipped(i))
            std::strncpy(f.moveIds[i], id, kPvpIdCap - 1);
    for (int i = 0; i < kModSlots; ++i)
        if (const char* id = loadout_.equipped(i))
            std::strncpy(f.modIds[i], id, kPvpIdCap - 1);

    // Level is display-only in a duel (nothing pays XP), but the stat points it bought
    // are the pet's real strength and must cross.
    f.level = static_cast<uint16_t>(std::min(combatLevel_, 0xFFFF));
    for (int i = 0; i < 4; ++i)
        f.statPoints[i] = static_cast<uint8_t>(std::min(statPoints_[i], 255));
    *out = f;
    return true;
}

// --- Outbound ----------------------------------------------------------------

void Game::queuePvpFrame(const uint8_t* mac, const uint8_t* buf, size_t len) {
    if (!mac || !buf || len == 0 || len > kPvpFrameCap) return;
    // Full outbox: drop it. Every frame this state machine sends is repeated on
    // kPvpRetryMs and is idempotent at the receiver, so a drop costs one retry
    // interval — cheaper than growing a queue for a three-frame protocol.
    if (pvpOutCount_ >= kPvpOutboxCap) return;
    PvpOut& o = pvpOutbox_[pvpOutCount_++];
    std::memcpy(o.mac, mac, 6);
    std::memcpy(o.buf, buf, len);
    o.len = len;
}

bool Game::takePvpOut(uint8_t* mac, uint8_t* buf, size_t cap, size_t* len) {
    if (!mac || !buf || !len || pvpOutCount_ <= 0) return false;
    const PvpOut& o = pvpOutbox_[0];
    if (o.len > cap) {          // unreachable while cap >= kPvpFrameCap; drop rather than overrun
        --pvpOutCount_;
        for (int i = 0; i < pvpOutCount_; ++i) pvpOutbox_[i] = pvpOutbox_[i + 1];
        return false;
    }
    std::memcpy(mac, o.mac, 6);
    std::memcpy(buf, o.buf, o.len);
    *len = o.len;
    --pvpOutCount_;
    for (int i = 0; i < pvpOutCount_; ++i) pvpOutbox_[i] = pvpOutbox_[i + 1];
    return true;
}

void Game::sendPvpPending() {
    uint8_t buf[kPvpFrameCap];
    size_t n = 0;
    switch (pvpPhase_) {
        case PvpPhase::Inviting:
            n = encodePvpInvite(pvpSession_, pvpLocalFighter_, buf, sizeof(buf));
            if (n) queuePvpFrame(pvpPeerMac_, buf, n);
            break;
        case PvpPhase::Arming:
        case PvpPhase::Fighting:
            // Whoever ANSWERED the challenge owes the other device its fighter spec —
            // the challenger already sent theirs inside the INVITE. Repeating it
            // alongside START is what lets a host that was also the target get both
            // frames across without a second handshake stage.
            if (!pvpChallenger_) {
                n = encodePvpAccept(pvpSession_, pvpLocalFighter_, buf, sizeof(buf));
                if (n) queuePvpFrame(pvpPeerMac_, buf, n);
            }
            // Only the host announces the fight, and it is the same START every time —
            // the guest dedupes on already being in the fight.
            if (pvpHost_) {
                n = encodePvpStart(pvpSession_, pvpSeed_,
                                   pvpCommitHash(pvpSeed_, pvpLocalFighter_,
                                                 pvpRemoteFighter_),
                                   buf, sizeof(buf));
                if (n) queuePvpFrame(pvpPeerMac_, buf, n);
            }
            break;
        case PvpPhase::Idle:
        case PvpPhase::Invited:     // waiting on OUR human — nothing to say yet
        case PvpPhase::Result:
            break;
    }
    pvpLastSendMs_ = nowMs_;
    ++pvpRetries_;
}

void Game::endPvpSession(const char* why, bool notify) {
    if (notify && pvpPhase_ != PvpPhase::Idle && pvpSession_ != 0) {
        uint8_t buf[kPvpFrameCap];
        const size_t n = encodePvpBye(pvpSession_, buf, sizeof(buf));
        if (n) queuePvpFrame(pvpPeerMac_, buf, n);
    }
    pvpPhase_ = PvpPhase::Idle;
    pvpSession_ = 0;
    pvpRetries_ = 0;
    std::snprintf(pvpStatus_, sizeof(pvpStatus_), "%s", why ? why : "");
    dirty_ = true;
}

void Game::pvpTick() {
    if (pvpPhase_ == PvpPhase::Idle || pvpPhase_ == PvpPhase::Result) return;

    // A duel that lost its screen lost its fight: a modal (Lockout, Critical System
    // Failure) preempting Nav::Combat means this device stopped playing the fight the
    // other one is still stepping. End it honestly rather than leaving a zombie session.
    if (pvpPhase_ == PvpPhase::Fighting && nav_ != Nav::Combat) {
        endPvpSession("DUEL INTERRUPTED", /*notify=*/true);
        return;
    }

    // The handshake stages fail out; Fighting doesn't, because it is purely local from
    // here and always terminates.
    if (pvpPhase_ != PvpPhase::Fighting &&
        nowMs_ - pvpPhaseStartMs_ >= kPvpInviteTimeoutMs) {
        endPvpSession(pvpPhase_ == PvpPhase::Invited ? "CHALLENGE EXPIRED" : "NO ANSWER",
                      /*notify=*/true);
        return;
    }

    if (pvpRetries_ >= kPvpMaxRetries) return;
    if (nowMs_ - pvpLastSendMs_ >= kPvpRetryMs) sendPvpPending();
}

// --- Starting a duel ---------------------------------------------------------

bool Game::pvpChallenge(uint64_t key) {
    if (pvpPhase_ != PvpPhase::Idle) return false;
    // No local MAC means no way to decide who hosts — the host build and any board that
    // never handed one over simply can't duel. Inert, not broken.
    if (localRadioKey_ == 0 || key == 0 || key == localRadioKey_) return false;
    if (!buildPvpFighter(&pvpLocalFighter_)) {
        endPvpSession("NO PET TO FIGHT", /*notify=*/false);
        return false;
    }
    // Only someone actually in range: a duel needs both devices powered and present,
    // and a remembered peer is neither.
    const PeerHello* live = nullptr;
    for (int i = 0; i < livePeerCount_; ++i)
        if (livePeers_[i].key == key && peerLive(livePeers_[i]))
            live = &livePeers_[i].hello;
    if (!live) return false;

    pvpPeerKey_ = key;
    unpackPeerKey(key, pvpPeerMac_);
    std::snprintf(pvpPeerTag_, sizeof(pvpPeerTag_), "%s",
                  live->tag[0] ? live->tag : "UNKNOWN OP");
    rng_ = rng_ * 1664525u + 1013904223u;
    pvpSession_ = rng_ ? rng_ : 1u;
    pvpChallenger_ = true;
    pvpHost_ = pvpHostIsLocal(localRadioKey_, key);
    pvpPhase_ = PvpPhase::Inviting;
    pvpPhaseStartMs_ = nowMs_;
    pvpRetries_ = 0;
    pvpStatus_[0] = '\0';
    sendPvpPending();
    dirty_ = true;
    return true;
}

void Game::pvpAcceptChallenge() {
    if (pvpPhase_ != PvpPhase::Invited) return;
    if (!buildPvpFighter(&pvpLocalFighter_)) {
        uint8_t buf[kPvpFrameCap];
        const size_t n = encodePvpDecline(pvpSession_, PvpDeclineReason::NoPet, buf,
                                          sizeof(buf));
        if (n) queuePvpFrame(pvpPeerMac_, buf, n);
        endPvpSession("NO PET TO FIGHT", /*notify=*/false);
        return;
    }
    pvpPhase_ = PvpPhase::Arming;
    pvpPhaseStartMs_ = nowMs_;
    pvpRetries_ = 0;
    if (pvpHost_) {
        // We host: pick the seed and start immediately. sendPvpPending then carries our
        // fighter AND the START across together, repeated until the retry budget runs
        // out — the guest only needs one of them to land.
        rng_ = rng_ * 1664525u + 1013904223u;
        startPvpBattle(rng_ ? rng_ : 1u);
    } else {
        sendPvpPending();            // our fighter; then we wait for their START
    }
    dirty_ = true;
}

void Game::pvpDeclineChallenge() {
    if (pvpPhase_ != PvpPhase::Invited) return;
    uint8_t buf[kPvpFrameCap];
    const size_t n = encodePvpDecline(pvpSession_, PvpDeclineReason::Refused, buf,
                                      sizeof(buf));
    if (n) queuePvpFrame(pvpPeerMac_, buf, n);
    endPvpSession("", /*notify=*/false);   // the DECLINE above IS the notification
}

void Game::pvpCancel() {
    if (pvpPhase_ == PvpPhase::Idle) return;
    endPvpSession("", /*notify=*/true);
}

// --- Inbound -----------------------------------------------------------------

bool Game::onPvpFrame(const uint8_t* mac, const uint8_t* payload, size_t len) {
    if (!mac) return false;
    PvpFrame f;
    // A frame that fails to decode is simply not a duel frame: wrong magic, a protocol
    // version this build can't lay out, or a wrong-length payload. Dropped silently —
    // anything in range can transmit, so a bad frame is noise, not an error.
    if (!decodePvpFrame(payload, len, &f)) return false;

    const uint64_t key = packPeerKey(mac);
    if (key == 0 || key == localRadioKey_) return false;

    // An INVITE opens a session, so it is the one frame that doesn't have to match the
    // one we're already in. Everything else must, which is what makes a stale or
    // foreign frame inert.
    if (f.type != PvpFrameType::Invite &&
        (pvpPhase_ == PvpPhase::Idle || f.session != pvpSession_ || key != pvpPeerKey_))
        return false;

    switch (f.type) {
        case PvpFrameType::Invite: {
            // A RETRY of the session we are already in, from the same operator. The
            // challenger repeats its INVITE until something answers, which for a
            // challenge means "until a human presses a button" — so these arrive for
            // the whole time the prompt is on screen. Ignoring them is the only correct
            // answer: replying BUSY here would decline the very challenge this device
            // is displaying, and the challenger would give up before its opponent could
            // possibly accept.
            if (pvpSession_ == f.session && pvpPeerKey_ == key) return false;

            if (pvpPhase_ != PvpPhase::Idle) {
                // A DIFFERENT session — someone else (or the same operator starting
                // over) challenging a device that is already in a duel. That one really
                // is busy, and saying so beats letting them wait for a timeout.
                uint8_t buf[kPvpFrameCap];
                const size_t n = encodePvpDecline(f.session, PvpDeclineReason::Busy, buf,
                                                  sizeof(buf));
                if (n) queuePvpFrame(mac, buf, n);
                return false;
            }
            // Consent is a human on the LINK screen. Anywhere else the answer is BUSY —
            // a challenge must never interrupt someone who isn't looking for one, and
            // this device may not have the radio at all outside that screen.
            uint8_t buf[kPvpFrameCap];
            PvpDeclineReason refuse = PvpDeclineReason::Busy;
            bool ok = true;
            if (!linkScreenOpen() || localRadioKey_ == 0) ok = false;
            else if (!pet_) { ok = false; refuse = PvpDeclineReason::NoPet; }
            else if (f.rules != kPvpRulesVersion ||
                     !canBuildPvpCombatant(registry_, f.fighter)) {
                // Different builds or different content would resolve the same spec
                // differently. Refusing is the honest outcome — two screens disagreeing
                // about who won is worse than no duel.
                ok = false;
                refuse = PvpDeclineReason::Mismatch;
            }
            if (!ok) {
                const size_t n = encodePvpDecline(f.session, refuse, buf, sizeof(buf));
                if (n) queuePvpFrame(mac, buf, n);
                return false;
            }
            pvpSession_ = f.session;
            pvpPeerKey_ = key;
            std::memcpy(pvpPeerMac_, mac, 6);
            pvpRemoteFighter_ = f.fighter;
            std::snprintf(pvpPeerTag_, sizeof(pvpPeerTag_), "%s",
                          f.fighter.tag[0] ? f.fighter.tag : "UNKNOWN OP");
            pvpChallenger_ = false;
            pvpHost_ = pvpHostIsLocal(localRadioKey_, key);
            pvpPhase_ = PvpPhase::Invited;
            pvpPhaseStartMs_ = nowMs_;
            pvpRetries_ = 0;
            pvpStatus_[0] = '\0';
            dirty_ = true;
            return true;
        }

        case PvpFrameType::Accept: {
            // Only the challenger is waiting on one, and only once — a repeat that
            // arrives after the fight has started is the retry doing its job.
            if (pvpPhase_ != PvpPhase::Inviting) return false;
            if (f.rules != kPvpRulesVersion ||
                !canBuildPvpCombatant(registry_, f.fighter)) {
                endPvpSession("BUILD MISMATCH", /*notify=*/true);
                return true;
            }
            pvpRemoteFighter_ = f.fighter;
            if (f.fighter.tag[0])
                std::snprintf(pvpPeerTag_, sizeof(pvpPeerTag_), "%s", f.fighter.tag);
            pvpPhase_ = PvpPhase::Arming;
            pvpPhaseStartMs_ = nowMs_;
            pvpRetries_ = 0;
            if (pvpHost_) {
                rng_ = rng_ * 1664525u + 1013904223u;
                startPvpBattle(rng_ ? rng_ : 1u);
            }
            dirty_ = true;
            return true;
        }

        case PvpFrameType::Start: {
            if (pvpHost_) return false;                  // we pick the seed, not them
            if (pvpPhase_ != PvpPhase::Arming) return false;   // already fighting, or too early
            // The divergence guard. Recomputed over OUR copies of the two specs, in the
            // host-then-guest order both devices seat them in; a mismatch means we would
            // have played a different fight, so refuse instead.
            if (pvpCommitHash(f.seed, pvpRemoteFighter_, pvpLocalFighter_) != f.commit) {
                endPvpSession("FIGHT DESYNC", /*notify=*/true);
                return true;
            }
            startPvpBattle(f.seed);
            return true;
        }

        case PvpFrameType::Decline: {
            if (pvpPhase_ != PvpPhase::Inviting) return false;
            const char* why = "DECLINED";
            switch (f.reason) {
                case PvpDeclineReason::Busy:     why = "OPERATOR BUSY"; break;
                case PvpDeclineReason::NoPet:    why = "THEY HAVE NO PET"; break;
                case PvpDeclineReason::Mismatch: why = "BUILD MISMATCH"; break;
                case PvpDeclineReason::Refused:  break;
            }
            endPvpSession(why, /*notify=*/false);
            return true;
        }

        case PvpFrameType::Bye:
            // They walked away. Nothing to tell them, and a fight already resolved
            // locally keeps its result — the verdict was computed here, not received.
            if (pvpPhase_ == PvpPhase::Result) return false;
            endPvpSession("OPPONENT LEFT", /*notify=*/false);
            return true;
    }
    return false;
}

// --- The fight ---------------------------------------------------------------

void Game::startPvpBattle(uint32_t seed) {
    pvpSeed_ = seed;
    // Host first, always, on both devices — that fixed seating is what makes two
    // independent runs of the same seeded Combat land on the same result.
    const PvpFighter& host = pvpHost_ ? pvpLocalFighter_ : pvpRemoteFighter_;
    const PvpFighter& guest = pvpHost_ ? pvpRemoteFighter_ : pvpLocalFighter_;
    beginPvpBattle(combat_, registry_, host, guest, seed);
    // The one fight in the game whose opponent is a real species rather than a
    // malbeast/boss/dummy spec, so it is the one that can reveal a creature the
    // operator has never raised. Recorded at the START, not the verdict: facing a
    // species is the glimpse, and a duel that desyncs or is walked out of was still
    // fought. This is reveal state, not a stake — a duel still pays nothing.
    markCreatureSeen(pvpRemoteFighter_.creatureId);
    // ...and on the same rule, the achievement for facing another operator at all. A duel
    // pays nothing, so recording that it HAPPENED is the whole reward.
    unlockAchievement(ach::kFirstDuel);
    combatBeat_ = 0;
    combatTurnBeat_ = 0;
    combatHitBeat_ = 0;
    combatStatsOpen_ = false;
    pvpPhase_ = PvpPhase::Fighting;
    pvpPhaseStartMs_ = nowMs_;
    pvpRetries_ = 0;
    nav_ = Nav::Combat;
    sendPvpPending();      // host: the START; guest-that-answered: its fighter, again
    dirty_ = true;
}

void Game::finishPvpBattle() {
    // Combat's verdict is framed around player_, which is the HOST. A guest reads it
    // inverted — same fight, same winner, opposite pronoun.
    const bool hostWon = combat_.outcome() == Combat::Outcome::Win;
    pvpWon_ = pvpHost_ == hostWon;
    pvpPhase_ = PvpPhase::Result;
    pvpRetries_ = 0;

    char line[28];
    std::snprintf(line, sizeof(line), "%s %s", pvpWon_ ? "BEAT" : "LOST TO", pvpPeerTag_);
    log_.push(pvpWon_ ? LogEventType::CombatWon : LogEventType::CombatLost, line);
    // No applyCombatResult: a duel pays nothing and costs nothing, so no XP, Bits,
    // Fragmentation, care signal or save write happens here. The banner is the reward.
    nav_ = Nav::Submenu;
    dirty_ = true;
}

// --- Input -------------------------------------------------------------------

void Game::onHackerLink(const ButtonEvent& ev) {
    switch (pvpPhase_) {
        case PvpPhase::Invited:
            // The consent gate. B accepts, C refuses; A is inert so a stray cycle press
            // can never answer for the human.
            if (ev.button == Button::B) pvpAcceptChallenge();
            else if (ev.button == Button::C) pvpDeclineChallenge();
            return;
        case PvpPhase::Inviting:
        case PvpPhase::Arming:
            if (ev.button == Button::C) pvpCancel();      // give up waiting
            return;
        case PvpPhase::Result:
            // Any button clears the verdict back to the challengeable list.
            pvpPhase_ = PvpPhase::Idle;
            pvpSession_ = 0;
            pvpStatus_[0] = '\0';
            dirty_ = true;
            return;
        case PvpPhase::Fighting:
            return;                                       // the combat screen owns input
        case PvpPhase::Idle:
            break;
    }

    if (ev.button == Button::C) { nav_ = Nav::Cursor; return; }

    uint64_t keys[kMaxRows];
    const int n = pvpChallengeableKeys(keys, kMaxRows);
    if (n == 0) return;                                   // empty state: A/B inert
    if (pvpRow_ >= n) pvpRow_ = 0;
    if (ev.button == Button::A) pvpRow_ = (pvpRow_ + 1) % n;
    else if (ev.button == Button::B) pvpChallenge(keys[pvpRow_]);
}

int Game::pvpChallengeableKeys(uint64_t* out, int max) const {
    // Live peers only. A remembered operator is a memory, not an opponent — both
    // devices have to be powered, present, and on the same rendezvous channel for a
    // duel to happen at all.
    int n = 0;
    for (int i = 0; i < livePeerCount_ && n < max; ++i)
        if (peerLive(livePeers_[i])) out[n++] = livePeers_[i].key;
    return n;
}

// --- Rendering ---------------------------------------------------------------

void Game::drawHackerLink(Framebuffer& fb) const {
    drawText(fb, kMargin, 28, "LINK", palColor(Pal::INK));

    // Name whoever actually holds the radio rather than implying LINK has it. An empty
    // list means something entirely different when the 'Pedia AP owns the radio than
    // when nobody is around.
    const bool starved = apEnabled_ || auditCapture_.capturing();
    const char* radioState = apEnabled_                  ? "AP HAS RADIO"
                             : auditCapture_.capturing() ? "CAPTURE HAS RADIO"
                                                         : "LINKING";
    drawText(fb, kActiveW - kMargin - textWidth(radioState), 28, radioState,
             starved ? palColor(Pal::WARN) : palColor(Pal::INK_DIM));
    fb.fillRect(0, 44, kActiveW, 1, palColor(Pal::TRACK));

    // Every non-Idle phase takes over the body: what the operator needs to know is the
    // state of the one session, not a list they can't act on.
    if (pvpPhase_ != PvpPhase::Idle) {
        const char* head = "";
        const char* sub = "";
        const char* hint = "";
        switch (pvpPhase_) {
            case PvpPhase::Inviting:
                head = "CHALLENGE SENT"; sub = "WAITING FOR ANSWER"; hint = "C CANCEL";
                break;
            case PvpPhase::Invited:
                head = "CHALLENGES YOU"; sub = "NO STAKES  NO EXPLOITS";
                hint = "B FIGHT   C REFUSE";
                break;
            case PvpPhase::Arming:
                head = "SYNCING FIGHT"; sub = "AGREEING ON THE SEED"; hint = "C CANCEL";
                break;
            case PvpPhase::Fighting:
                head = "DUEL IN PROGRESS"; sub = ""; hint = "";
                break;
            case PvpPhase::Result:
                // The verdict is a WORD, never a colour — it has to survive grayscale.
                head = pvpWon_ ? "YOU WON" : "YOU LOST"; sub = ""; hint = "ANY KEY";
                break;
            case PvpPhase::Idle:
                break;
        }
        drawText(fb, kMargin, 56, head, palColor(Pal::INK));
        drawText(fb, kMargin, 56 + kLineH, pvpPeerTag_, palColor(Pal::ACCENT));
        if (sub[0]) drawText(fb, kMargin, 56 + kLineH * 2, sub, palColor(Pal::INK_DIM));
        if (hint[0])
            drawText(fb, kMargin, kActiveH - 20, hint, palColor(Pal::ACCENT));
        fb.fillRect(0, kActiveH - 26, kActiveW, 1, palColor(Pal::TRACK));
        return;
    }

    // Idle: whatever went wrong last time, then the challengeable list.
    if (pvpStatus_[0]) drawText(fb, kMargin, 50, pvpStatus_, palColor(Pal::WARN));
    const int listTop = pvpStatus_[0] ? 50 + kLineH : 52;

    uint64_t keys[kMaxRows];
    const int n = pvpChallengeableKeys(keys, kMaxRows);
    if (n == 0) {
        // Three very different reasons to be empty — say which. Words, never colour.
        drawText(fb, kMargin, listTop, starved ? "LINK CANT RUN" : "NOBODY IN RANGE",
                 palColor(Pal::INK_DIM));
        drawText(fb, kMargin, listTop + kLineH,
                 apEnabled_                  ? "TURN OFF AP IN CFG"
                 : auditCapture_.capturing() ? "CAPTURE IN PROGRESS"
                 : !pet_                     ? "AND NO PET TO FIGHT"
                                             : "LISTENING...",
                 palColor(Pal::INK_DIM));
    } else {
        int sel = pvpRow_;
        if (sel >= n) sel = 0;
        int scrollTop = 0;
        if (n > kVisibleRows) {
            if (sel >= kVisibleRows) scrollTop = sel - kVisibleRows + 1;
            scrollTop = std::min(scrollTop, n - kVisibleRows);
        }
        for (int v = 0; v < kVisibleRows && scrollTop + v < n; ++v) {
            const uint64_t key = keys[scrollTop + v];
            const PeerHello* h = nullptr;
            for (int i = 0; i < livePeerCount_; ++i)
                if (livePeers_[i].key == key) h = &livePeers_[i].hello;
            if (!h) continue;
            const int rowY = listTop + v * kRowH;
            const bool s = scrollTop + v == sel;
            if (s) drawRowCursor(fb, 2, rowY + 1, palColor(Pal::ACCENT));

            drawText(fb, kMargin, rowY, h->tag[0] ? h->tag : "UNKNOWN OP",
                     s ? palColor(Pal::INK) : palColor(Pal::INK_DIM));

            char pet[40];
            if (h->petName[0])
                std::snprintf(pet, sizeof(pet), "%s  %s", h->petName,
                              stageName(static_cast<Stage>(h->stage)));
            else
                std::snprintf(pet, sizeof(pet), "NO PET");
            drawText(fb, kMargin + 6, rowY + kLineH, pet, palColor(Pal::INK_DIM));

            // Who you would be fighting FOR. crewName/crewRed ride in every hello
            // (core/net/peer_link.h) and had no reader, so the one thing that makes
            // a duel a rivalry rather than a stat check was arriving and being
            // dropped. Same three channels the CREW screen uses — the name, the
            // side's glyph, and its colour — so an unenlisted peer reads as
            // NO CREW rather than as a missing tint.
            const int crewY = rowY + kLineH * 2;
            if (h->crewName[0]) {
                const Rgb565 side = teamColor(h->crewRed);
                drawSpriteTinted(fb, h->crewRed ? ASSET_ICON_TEAM_RED
                                                : ASSET_ICON_TEAM_BLUE,
                                 0, kMargin + 6, crewY - 5, side);
                drawText(fb, kMargin + 28, crewY, h->crewName, side);
            } else {
                drawText(fb, kMargin + 6, crewY, "NO CREW", palColor(Pal::INK_DIM));
            }
        }
    }

    // Ambient discovery borrows the audit scan's radio window, so this screen finding
    // nobody with the scan off is the toggle working, not a fault. Saying so is what
    // keeps a LINK-on/AUDIT-off device from looking broken.
    if (linkAmbientStarved())
        drawText(fb, kMargin, kActiveH - 34, "AMBIENT NEEDS AUDIT SCAN",
                 palColor(Pal::WARN));

    fb.fillRect(0, kActiveH - 26, kActiveW, 1, palColor(Pal::TRACK));
    drawText(fb, kMargin, kActiveH - 20, n > 0 ? "A NEXT  B FIGHT" : "A NEXT",
             palColor(Pal::INK_DIM));
    drawText(fb, kActiveW - kMargin - textWidth("C BACK"), kActiveH - 20, "C BACK",
             palColor(Pal::ACCENT));
}

}  // namespace mal
