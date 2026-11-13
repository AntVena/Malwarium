// pvp_session.h: one 1v1 duel session, a run (CONTRIBUTING, "Where state lives"). From a
// challenge sent or received until it is cancelled, refused, times out, or its verdict is
// dismissed. The fight itself is a Fight; this holds the handshake around it and the
// verdict after it. Runtime-only and never saved: a duel is worth nothing and survives
// nothing.
//
// Game holds it only while a session is open, verdict included, so PvpPhase::Idle is
// "no session". What outlives a session stays on Game: the outbound frame queue (it
// carries the BYE that ends one, and BUSY replies when there is none) and the LINK
// screen's line saying why the last one ended.
#pragma once

#include <cstdint>
#include <cstring>

#include "core/net/pvp_link.h"

namespace mal {

// Where the local device is in a duel. Every phase but Idle is an open session, which
// keeps the LINK radio up.
enum class PvpPhase : uint8_t {
    Idle,        // nothing in flight
    Inviting,    // we challenged; waiting for their accept/decline
    Invited,     // they challenged us; waiting on THIS human to accept or refuse
    Arming,      // both fighters known; host is sending / guest awaiting START
    Fighting,    // the shared fight is running (nav_ == Nav::Combat)
    Result,      // it finished; the LINK screen is showing who won
};

struct PvpSession {
    PvpPhase phase = PvpPhase::Inviting;
    // The id every frame of this duel carries: what makes a stale or foreign frame
    // inert. It is NOT a credential (core/net/pvp_link.h).
    uint32_t id = 0;
    uint64_t peerKey = 0;               // opponent's packed radio MAC
    uint8_t peerMac[6] = {0};           // ...and the raw form, for addressing replies
    char peerTag[kPvpTagCap] = {0};     // their HackerTag, for every line the UI draws
    PvpFighter local;                   // our own spec: encoded, then built FROM
    PvpFighter remote;                  // theirs, as decoded off the wire
    bool challenger = false;            // we opened the session, so we already sent our
                                        // fighter inside the INVITE and owe no ACCEPT
    bool host = false;                  // we hold the player_ slot and pick the seed
    bool won = false;                   // the finished duel's verdict, locally framed
    uint32_t seed = 0;
    uint32_t phaseStartMs = 0;          // for the phase timeout
    uint32_t lastSendMs = 0;            // retry pacing for the frame holding the phase
    int retries = 0;

    // A session in its first phase: Inviting for the challenger, Invited for the target.
    static PvpSession opened(PvpPhase phase, uint32_t id, uint64_t peerKey,
                             const uint8_t* peerMac, const char* peerTag, bool host,
                             uint32_t nowMs) {
        PvpSession s;
        s.phase = phase;
        s.id = id;
        s.peerKey = peerKey;
        std::memcpy(s.peerMac, peerMac, sizeof(s.peerMac));
        s.setTag(peerTag);
        s.challenger = phase == PvpPhase::Inviting;
        s.host = host;
        s.phaseStartMs = nowMs;
        return s;
    }
    // A new phase restarts its timeout and its retry budget.
    void enter(PvpPhase p, uint32_t nowMs) {
        phase = p;
        phaseStartMs = nowMs;
        retries = 0;
    }
    // A blank tag reads as UNKNOWN OP on every line that names the opponent.
    void setTag(const char* tag) {
        std::strncpy(peerTag, tag && tag[0] ? tag : "UNKNOWN OP", sizeof(peerTag) - 1);
        peerTag[sizeof(peerTag) - 1] = '\0';
    }
};

}  // namespace mal
