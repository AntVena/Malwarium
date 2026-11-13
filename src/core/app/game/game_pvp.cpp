#include "core/app/game.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/model/pvp_battle.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/layout.h"
#include "core/ui/theme.h"
#include "core/ui/widgets.h"
#include "generated/assets.h"

// game_pvp.cpp (the Hacker-face LINK slot): 1v1 duels with an operator standing in
// front of you.
//
// PEERS answers "who is around"; LINK is where you do something about it. The whole
// feature is a three-frame handshake and then silence:
//
//   1. B on a LIVE peer sends an INVITE carrying this device's fighter spec.
//   2. Their human presses B, which sends an ACCEPT carrying theirs. Consent is
//      explicit: an incoming challenge is considered only while the LINK screen is
//      open, and anywhere else the device replies BUSY.
//   3. Whichever device has the lower radio MAC is the HOST (pvpHostIsLocal). It picks
//      the seed and sends START. Both devices then build the same two fighters and step
//      the same seeded Combat locally (core/model/pvp_battle.h), so the fight plays out
//      on both screens at once with the same result and not one turn crosses the air.
//
// The host occupies Combat's player_ slot on BOTH devices, so the guest's own pet is
// the enemy_ row: the combat screen relabels the sides (CombatSides) and the fight
// resolves the same. START also carries pvpCommitHash over the seed and both specs; a
// guest that computes a different one refuses rather than playing a fight its opponent
// isn't playing.
//
// NO STAKES, AND THE WIRE FORMAT DEPENDS ON THAT. Nothing here is saved, no XP, Bits,
// Fragmentation or care signal moves, and no Exploit is available mid-fight (the A+C
// picker pauses the fight for a human, and there is no way to pause the other device's
// copy). Duel frames are unauthenticated (core/net/pvp_link.h): the session id makes a
// stale frame inert but proves nothing, which is only acceptable while a win is worth
// nothing. Anything that pays out needs signing first.

namespace mal {

namespace {
// A challengeable row is tag / pet: two lines, so the list clears the header rule with
// room for the status band and the hint band underneath.
constexpr int kPvpBlockH = kLineH * 3 + 6;   // tag · pet+stage · crew
constexpr int kPvpVisibleRows = 3;
constexpr int kMaxRows = 32;
}  // namespace

// --- Identity ----------------------------------------------------------------

void Game::setLocalRadioMac(const uint8_t* mac) {
    localRadioKey_ = packPeerKey(mac);
}

bool Game::buildPvpFighter(PvpFighter* out) const {
    if (!out || !pet_.creature) return false;
    PvpFighter f;
    std::strncpy(f.tag, op_.hackerTag, sizeof(f.tag) - 1);
    std::strncpy(f.creatureId, pet_.creature->id, sizeof(f.creatureId) - 1);

    // Equipped kit by id. Only slots this pet's stage has unlocked are sent. A
    // locked move is inert in combat, and sending it could make the two devices
    // disagree about which slots count.
    const int slots = MoveLoadout::slotsForStage(pet_.creature->stage);
    for (int i = 0; i < slots && i < kMaxMoveSlots; ++i)
        if (const char* id = pet_.moves.equipped(i))
            std::strncpy(f.moveIds[i], id, kPvpIdCap - 1);
    for (int i = 0; i < kModSlots; ++i)
        if (const char* id = pet_.mods.equipped(i))
            std::strncpy(f.modIds[i], id, kPvpIdCap - 1);

    // Level is display-only in a duel (nothing pays XP), but the stat points it bought
    // are the pet's real strength and must cross, including the off-level ones an Epic
    // dish granted, which the pet fights with at home and would otherwise leave behind.
    f.level = static_cast<uint16_t>(std::min(pet_.level, 0xFFFF));
    for (int i = 0; i < 4; ++i)
        f.statPoints[i] = static_cast<uint8_t>(std::min(pet_.totalStatPoint(i), 255));
    *out = f;
    return true;
}

// --- Outbound ----------------------------------------------------------------

void Game::queuePvpFrame(const uint8_t* mac, const uint8_t* buf, size_t len) {
    if (!mac || !buf || len == 0 || len > kPvpFrameCap) return;
    // Full outbox: drop it. Every frame this state machine sends is repeated on
    // kPvpRetryMs and is idempotent at the receiver, so a drop costs one retry
    // interval, and a three-frame protocol needs no queue.
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
    if (!pvp_) return;
    PvpSession& p = *pvp_;
    uint8_t buf[kPvpFrameCap];
    size_t n = 0;
    switch (p.phase) {
        case PvpPhase::Inviting:
            n = encodePvpInvite(p.id, p.local, buf, sizeof(buf));
            if (n) queuePvpFrame(p.peerMac, buf, n);
            break;
        case PvpPhase::Arming:
        case PvpPhase::Fighting:
            // The device that answered the challenge sends its fighter spec next:
            // the challenger already sent theirs inside the INVITE. Repeating it
            // alongside START is what lets a host that was also the target get both
            // frames across without a second handshake stage.
            if (!p.challenger) {
                n = encodePvpAccept(p.id, p.local, buf, sizeof(buf));
                if (n) queuePvpFrame(p.peerMac, buf, n);
            }
            // Only the host announces the fight, and it is the same START every time:
            // the guest dedupes on already being in the fight.
            if (p.host) {
                n = encodePvpStart(p.id, p.seed, pvpCommitHash(p.seed, p.local, p.remote),
                                   buf, sizeof(buf));
                if (n) queuePvpFrame(p.peerMac, buf, n);
            }
            break;
        case PvpPhase::Idle:
        case PvpPhase::Invited:     // waiting on OUR human: nothing to say yet
        case PvpPhase::Result:
            break;
    }
    p.lastSendMs = nowMs_;
    ++p.retries;
}

void Game::endPvpSession(const char* why, bool notify) {
    if (notify && pvp_ && pvp_->id != 0) {
        uint8_t buf[kPvpFrameCap];
        const size_t n = encodePvpBye(pvp_->id, buf, sizeof(buf));
        if (n) queuePvpFrame(pvp_->peerMac, buf, n);
    }
    pvp_.reset();
    std::snprintf(pvpStatus_, sizeof(pvpStatus_), "%s", why ? why : "");
    dirty_ = true;
}

void Game::pvpTick() {
    if (!pvp_ || pvp_->phase == PvpPhase::Result) return;
    const PvpPhase phase = pvp_->phase;

    // A duel that lost its screen lost its fight: a modal (Lockout, Critical System
    // Failure) preempting Nav::Combat means this device stopped playing the fight the
    // other one is still stepping. End it honestly rather than leaving a zombie session.
    if (phase == PvpPhase::Fighting && nav_ != Nav::Combat) {
        endPvpSession("DUEL INTERRUPTED", /*notify=*/true);
        return;
    }

    // The handshake stages fail out; Fighting doesn't, because it is purely local from
    // here and always terminates.
    if (phase != PvpPhase::Fighting && nowMs_ - pvp_->phaseStartMs >= kPvpInviteTimeoutMs) {
        endPvpSession(phase == PvpPhase::Invited ? "CHALLENGE EXPIRED" : "NO ANSWER",
                      /*notify=*/true);
        return;
    }

    if (pvp_->retries >= kPvpMaxRetries) return;
    if (nowMs_ - pvp_->lastSendMs >= kPvpRetryMs) sendPvpPending();
}

// --- Starting a duel ---------------------------------------------------------

bool Game::pvpChallenge(uint64_t key) {
    if (pvp_) return false;
    // Without a local MAC there is no way to pick the host, so the host build and any
    // board without one cannot duel. Inert, not broken.
    if (localRadioKey_ == 0 || key == 0 || key == localRadioKey_) return false;
    PvpFighter local;
    if (!buildPvpFighter(&local)) {
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

    uint8_t mac[6];
    unpackPeerKey(key, mac);
    rng_ = rng_ * 1664525u + 1013904223u;
    pvp_ = PvpSession::opened(PvpPhase::Inviting, rng_ ? rng_ : 1u, key, mac, live->tag,
                              pvpHostIsLocal(localRadioKey_, key), nowMs_);
    pvp_->local = local;
    pvpStatus_[0] = '\0';
    sendPvpPending();
    dirty_ = true;
    return true;
}

void Game::pvpAcceptChallenge() {
    if (pvpPhase() != PvpPhase::Invited) return;
    PvpSession& p = *pvp_;
    if (!buildPvpFighter(&p.local)) {
        uint8_t buf[kPvpFrameCap];
        const size_t n = encodePvpDecline(p.id, PvpDeclineReason::NoPet, buf, sizeof(buf));
        if (n) queuePvpFrame(p.peerMac, buf, n);
        endPvpSession("NO PET TO FIGHT", /*notify=*/false);
        return;
    }
    p.enter(PvpPhase::Arming, nowMs_);
    if (p.host) {
        // We host: pick the seed and start immediately. sendPvpPending then carries our
        // fighter and the START together, repeated until the retry budget runs out: the
        // guest only needs one of them to land.
        rng_ = rng_ * 1664525u + 1013904223u;
        startPvpBattle(rng_ ? rng_ : 1u);
    } else {
        sendPvpPending();            // our fighter; then we wait for their START
    }
    dirty_ = true;
}

void Game::pvpDeclineChallenge() {
    if (pvpPhase() != PvpPhase::Invited) return;
    uint8_t buf[kPvpFrameCap];
    const size_t n = encodePvpDecline(pvp_->id, PvpDeclineReason::Refused, buf, sizeof(buf));
    if (n) queuePvpFrame(pvp_->peerMac, buf, n);
    endPvpSession("", /*notify=*/false);   // the DECLINE above IS the notification
}

void Game::pvpCancel() {
    if (!pvp_) return;
    endPvpSession("", /*notify=*/true);
}

// --- Inbound -----------------------------------------------------------------

bool Game::onPvpFrame(const uint8_t* mac, const uint8_t* payload, size_t len) {
    if (!mac) return false;
    PvpFrame f;
    // A frame that fails to decode is simply not a duel frame: wrong magic, a protocol
    // version this build can't lay out, or a wrong-length payload. Dropped silently,
    // because anything in range can transmit and a bad frame is noise.
    if (!decodePvpFrame(payload, len, &f)) return false;

    const uint64_t key = packPeerKey(mac);
    if (key == 0 || key == localRadioKey_) return false;

    // An INVITE opens a session, so it is the one frame that doesn't have to match the
    // one we're already in. Everything else must, which is what makes a stale or
    // foreign frame inert.
    if (f.type != PvpFrameType::Invite && (!pvp_ || f.session != pvp_->id || key != pvp_->peerKey))
        return false;

    switch (f.type) {
        case PvpFrameType::Invite: {
            // A RETRY of the session we are already in, from the same operator. The
            // challenger repeats its INVITE until something answers, which means until
            // someone presses a button, so these arrive the whole time the prompt is
            // up. Ignoring them is the only correct answer: replying BUSY here would
            // decline the very challenge this device is displaying, and the challenger
            // would give up before its opponent could possibly accept.
            if (pvp_ && pvp_->id == f.session && pvp_->peerKey == key) return false;

            if (pvp_) {
                // A DIFFERENT session: someone else (or the same operator starting
                // over) challenging a device that is already in a duel. That one really
                // is busy, and saying so beats letting them wait for a timeout.
                uint8_t buf[kPvpFrameCap];
                const size_t n = encodePvpDecline(f.session, PvpDeclineReason::Busy, buf,
                                                  sizeof(buf));
                if (n) queuePvpFrame(mac, buf, n);
                return false;
            }
            // Consent is a human on the LINK screen. Anywhere else the answer is BUSY:
            // a challenge must not interrupt someone who is not looking for one, and
            // outside that screen the radio may be off.
            uint8_t buf[kPvpFrameCap];
            PvpDeclineReason refuse = PvpDeclineReason::Busy;
            bool ok = true;
            if (!linkScreenOpen() || localRadioKey_ == 0) ok = false;
            else if (!pet_.creature) { ok = false; refuse = PvpDeclineReason::NoPet; }
            else if (f.rules != kPvpRulesVersion ||
                     !canBuildPvpCombatant(registry_, f.fighter)) {
                // Different builds or different content would resolve the same spec
                // differently. Refuse: two screens disagreeing about who won is worse
                // than no duel.
                ok = false;
                refuse = PvpDeclineReason::Mismatch;
            }
            if (!ok) {
                const size_t n = encodePvpDecline(f.session, refuse, buf, sizeof(buf));
                if (n) queuePvpFrame(mac, buf, n);
                return false;
            }
            pvp_ = PvpSession::opened(PvpPhase::Invited, f.session, key, mac, f.fighter.tag,
                                      pvpHostIsLocal(localRadioKey_, key), nowMs_);
            pvp_->remote = f.fighter;
            pvpStatus_[0] = '\0';
            dirty_ = true;
            return true;
        }

        case PvpFrameType::Accept: {
            // Only the challenger is waiting on one, and only once: a repeat
            // after the fight has started is just the retry.
            if (pvp_->phase != PvpPhase::Inviting) return false;
            if (f.rules != kPvpRulesVersion ||
                !canBuildPvpCombatant(registry_, f.fighter)) {
                endPvpSession("BUILD MISMATCH", /*notify=*/true);
                return true;
            }
            pvp_->remote = f.fighter;
            if (f.fighter.tag[0]) pvp_->setTag(f.fighter.tag);
            pvp_->enter(PvpPhase::Arming, nowMs_);
            if (pvp_->host) {
                rng_ = rng_ * 1664525u + 1013904223u;
                startPvpBattle(rng_ ? rng_ : 1u);
            }
            dirty_ = true;
            return true;
        }

        case PvpFrameType::Start: {
            if (pvp_->host) return false;                    // we pick the seed, not them
            if (pvp_->phase != PvpPhase::Arming) return false;  // already fighting, or too early
            // The divergence guard. Recomputed over OUR copies of the two specs, in the
            // host-then-guest order both devices seat them in; a mismatch means we would
            // have played a different fight, so refuse instead.
            if (pvpCommitHash(f.seed, pvp_->remote, pvp_->local) != f.commit) {
                endPvpSession("FIGHT DESYNC", /*notify=*/true);
                return true;
            }
            startPvpBattle(f.seed);
            return true;
        }

        case PvpFrameType::Decline: {
            if (pvp_->phase != PvpPhase::Inviting) return false;
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
            // They walked away. Nothing to send. A fight already resolved locally
            // keeps its result, because the verdict is computed on each device.
            if (pvp_->phase == PvpPhase::Result) return false;
            endPvpSession("OPPONENT LEFT", /*notify=*/false);
            return true;
    }
    return false;
}

// --- The fight ---------------------------------------------------------------

void Game::startPvpBattle(uint32_t seed) {
    PvpSession& p = *pvp_;
    p.seed = seed;
    // Host first on both devices. That fixed seating makes two runs of the same
    // seeded Combat reach the same result.
    const PvpFighter& host = p.host ? p.local : p.remote;
    const PvpFighter& guest = p.host ? p.remote : p.local;
    fight_ = Fight::started(FightCaller::Duel);
    beginPvpBattle(fight_.combat, registry_, host, guest, seed);
    // The one fight in the game whose opponent is a real species rather than a
    // malbeast/boss/dummy spec, so it is the one that can reveal a creature the
    // operator has never raised. Recorded at the START, not the verdict: facing a
    // species is the glimpse, and a duel that desyncs or is walked out of was still
    // fought. This is reveal state, not a stake: a duel still pays nothing.
    markCreatureSeen(p.remote.creatureId);
    // ...and on the same rule, the achievement for facing another operator at all. A duel
    // pays nothing, so recording that it HAPPENED is the whole reward.
    unlockAchievement(ach::kFirstDuel);
    // ...and, on the same rule, the mirror: recorded at the START, as the reveal and the
    // duel above are.
    noteMirrorMatch(p.remote.creatureId);
    fxBeat_ = 0;
    fightScreen_.shutPanel();
    p.enter(PvpPhase::Fighting, nowMs_);
    nav_ = Nav::Combat;
    sendPvpPending();      // host: the START; guest-that-answered: its fighter, again
    dirty_ = true;
    playSound(Sound::CombatStart);
}

void Game::finishPvpBattle() {
    // Combat's verdict is framed around player_, which is the HOST. A guest reads it
    // inverted: same fight, same winner, opposite pronoun.
    // A Draw (the turn limit, level on Health) is a loss for BOTH devices: neither side
    // won, so neither inverts it into a win.
    PvpSession& p = *pvp_;
    const bool drawn = fight_.combat.outcome() == Combat::Outcome::Draw;
    const bool hostWon = fight_.combat.outcome() == Combat::Outcome::Win;
    p.won = !drawn && p.host == hostWon;
    p.phase = PvpPhase::Result;
    p.retries = 0;

    char line[28];
    std::snprintf(line, sizeof(line), "%s %s", p.won ? "BEAT" : "LOST TO", p.peerTag);
    log_.push(p.won ? LogEventType::CombatWon : LogEventType::CombatLost, line);
    // The lifetime duel-win tally (save v56), and the one thing this function persists.
    // It is not a payout and does not make the duel one: RECORDING a result costs the
    // loser nothing and hands the winner nothing they can spend, which is the same
    // footing FIRST_DUEL already stands on above (it fires at the start of every duel
    // and marks the save then). A duel still pays no XP, Bits, Fragmentation or care
    // signal.
    if (p.won) {
        ++op_.tallies.pvpWins;
        markSaveDirty();
    }
    // No applyCombatResult: a duel pays nothing and costs nothing, so no XP, Bits,
    // Fragmentation or care signal comes of it here. The banner is the reward.
    nav_ = Nav::Submenu;
    dirty_ = true;
}

// --- Input -------------------------------------------------------------------

void Game::onHackerLink(const ButtonEvent& ev) {
    switch (pvpPhase()) {
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
            pvp_.reset();
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
    // Live peers only. A remembered operator cannot be challenged: a duel needs
    // both devices powered, present, and on the same rendezvous channel.
    int n = 0;
    for (int i = 0; i < livePeerCount_ && n < max; ++i)
        if (peerLive(livePeers_[i])) out[n++] = livePeers_[i].key;
    return n;
}

// --- Rendering ---------------------------------------------------------------

void Game::drawHackerLink(Framebuffer& fb) const {
    drawHeaderBand(fb, "LINK");
    drawText(fb, kMargin, kContextY, "LINK", palColor(Pal::INK));

    // Name whoever actually holds the radio rather than implying LINK has it. An empty
    // list means something entirely different when the 'Pedia AP owns the radio than
    // when nobody is around.
    const bool starved = apEnabled_ || auditCapture_.capturing();
    const char* radioState = apEnabled_                  ? "AP HAS RADIO"
                             : auditCapture_.capturing() ? "CAPTURE HAS RADIO"
                                                         : "LINKING";
    drawText(fb, kActiveW - kMargin - textWidth(radioState), kContextY, radioState,
             starved ? palColor(Pal::WARN) : palColor(Pal::INK_DIM));
    fb.fillRect(0, kContextRule, kActiveW, 1, palColor(Pal::TRACK));

    // Every non-Idle phase takes over the body: what the operator needs to know is the
    // state of the one session, not a list they can't act on.
    if (pvp_) {
        const char* head = "";
        const char* sub = "";
        const char* secondary = "";
        const char* hint = "";
        switch (pvp_->phase) {
            case PvpPhase::Inviting:
                head = "CHALLENGE SENT"; sub = "WAITING FOR ANSWER"; hint = "C CANCEL";
                break;
            case PvpPhase::Invited:
                head = "CHALLENGES YOU"; sub = "NO STAKES  NO EXPLOITS";
                secondary = "C REFUSE"; hint = "B FIGHT";
                break;
            case PvpPhase::Arming:
                head = "SYNCING FIGHT"; sub = "AGREEING ON THE SEED"; hint = "C CANCEL";
                break;
            case PvpPhase::Fighting:
                head = "DUEL IN PROGRESS"; sub = ""; hint = "";
                break;
            case PvpPhase::Result:
                // The verdict is a WORD, never a colour: it has to survive grayscale.
                head = pvp_->won ? "YOU WON" : "YOU LOST"; sub = ""; hint = "ANY KEY";
                break;
            case PvpPhase::Idle:
                break;
        }
        drawText(fb, kMargin, 56, head, palColor(Pal::INK));
        drawText(fb, kMargin, 56 + kLineH, pvp_->peerTag, palColor(Pal::ACCENT));
        if (sub[0]) drawText(fb, kMargin, 56 + kLineH * 2, sub, palColor(Pal::INK_DIM));
        drawHackerHint(fb, secondary, hint);
        return;
    }

    // Idle: whatever went wrong last time, then the challengeable list.
    if (pvpStatus_[0]) drawText(fb, kMargin, 50, pvpStatus_, palColor(Pal::WARN));
    const int listTop = pvpStatus_[0] ? 50 + kLineH : 52;

    uint64_t keys[kMaxRows];
    const int n = pvpChallengeableKeys(keys, kMaxRows);
    if (n == 0) {
        // Three very different reasons to be empty: say which. Words, never colour.
        drawText(fb, kMargin, listTop, starved ? "LINK CANT RUN" : "NOBODY IN RANGE",
                 palColor(Pal::INK_DIM));
        drawText(fb, kMargin, listTop + kLineH,
                 apEnabled_                  ? "TURN OFF AP IN CFG"
                 : auditCapture_.capturing() ? "CAPTURE IN PROGRESS"
                 : !pet_.creature                     ? "AND NO PET TO FIGHT"
                                             : "LISTENING...",
                 palColor(Pal::INK_DIM));
    } else {
        int sel = pvpRow_;
        if (sel >= n) sel = 0;
        int scrollTop = 0;
        if (n > kPvpVisibleRows) {
            if (sel >= kPvpVisibleRows) scrollTop = sel - kPvpVisibleRows + 1;
            scrollTop = std::min(scrollTop, n - kPvpVisibleRows);
        }
        for (int v = 0; v < kPvpVisibleRows && scrollTop + v < n; ++v) {
            const uint64_t key = keys[scrollTop + v];
            const PeerHello* h = nullptr;
            for (int i = 0; i < livePeerCount_; ++i)
                if (livePeers_[i].key == key) h = &livePeers_[i].hello;
            if (!h) continue;
            const int rowY = listTop + v * kPvpBlockH;
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
            // dropped. Same three channels the CREW screen uses (the name, the
            // side's glyph, and its colour), so an unenlisted peer reads as NO
            // CREW.
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

    // FIGHT is what this screen is for, so it takes the accent verb (VISUAL_LANGUAGE
    // §4.2) whenever there is someone to fight; with nobody listed, BACK is all there is.
    drawHackerHint(fb, n > 0 ? "A NEXT  C BACK" : "A NEXT", n > 0 ? "B FIGHT" : "C BACK");
}

}  // namespace mal
