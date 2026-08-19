// pvp_link.h — the 1v1 duel handshake's wire format.
//
// PURE + HOST-TESTABLE, like peer_link.h: this is only the encode/decode of the bytes
// two Malwariums exchange to agree on a fight. The radio that carries them is the
// device tier (platform/esp32/net_link.h, ESP-NOW unicast); the session state machine
// that drives them is Game (game_pvp.cpp); the fight they describe is built by
// core/model/pvp_battle.h. None of those is needed to test this file, and it pulls in
// no Arduino/ESP-IDF header.
//
// WHY A SPEC AND NOT A BLOW-BY-BLOW. Combat resolution is deterministic from a seed
// (core/model/combat.h), so a duel does not need its turns broadcast: if both devices
// build byte-identical Combatants and step the same seeded sim, they independently
// reach the same fight, hit for hit. So the whole protocol is "agree on two fighter
// SPECS and one seed", after which each screen plays the fight out locally. Three
// frames, then radio silence.
//
//   INVITE   challenger -> target   session id + the challenger's fighter
//   ACCEPT   target -> challenger   the target's fighter (the human said yes)
//   DECLINE  target -> challenger   the human said no, or this device is busy
//   START    host -> guest          the seed, plus a hash of everything it commits to
//   BYE      either -> either       abandon the session
//
// WHO IS HOST is not negotiated — pvpHostIsLocal() derives it from the two radio MACs,
// which both devices already know the moment they hear each other. The host picks the
// seed and sends START; the guest starts on receipt. That single frame is also what
// makes the two screens start together.
//
// UNAUTHENTICATED, AND BOUNDED BY THAT. Every field is attacker-controlled and only
// sanitized (see wire_field.h), never verified. This is acceptable for exactly as long
// as a duel is worth nothing: the first cut has no stakes, persists no result, and
// pays no reward, so the worst a forged frame achieves is an unwanted challenge prompt
// that a human still has to accept. The `session` id binds the frames of one duel
// together and makes a replayed frame from an old one inert, but it is NOT a
// signature. Anything that later moves persistent state across this link needs real
// per-frame signing first.
#pragma once

#include <cstddef>
#include <cstdint>

#include "tunables.h"

namespace mal {

// Wire layout version. Append fields and bump it; decode rejects a version it does not
// understand rather than misreading the layout.
constexpr uint8_t kPvpProtoVersion = 1;

// Resolution version — bumped whenever a change to combat maths or to the content a
// fighter references would make two builds resolve the SAME spec+seed differently.
// The wire could be identical and the fight still diverge, so this is checked
// separately from kPvpProtoVersion and a mismatch declines the duel outright. That is
// the honest outcome: better a refused challenge than two screens disagreeing about
// who won.
constexpr uint8_t kPvpRulesVersion = 2;

// Field capacities INCLUDE the terminator. Content ids are the long pole (the longest
// shipped move/mod/creature id is 20 chars), so one cap covers all three.
constexpr int kPvpTagCap = kHackerTagMax + 1;
constexpr int kPvpIdCap = 24;

// What one device says its pet IS, in terms both devices can rebuild identically.
//
// Deliberately a RECIPE, not a stat block: content ids plus the player-side numbers
// that modify them. Both devices resolve the ids against their own registry and run
// the same build (makePvpCombatant, core/model/pvp_battle.h), so the derived stats can
// never disagree about a rounding step. It also means a fighter is ~200 bytes instead
// of a serialized Combatant.
struct PvpFighter {
    char tag[kPvpTagCap] = {0};                    // the operator's HackerTag
    char creatureId[kPvpIdCap] = {0};              // species — the whole base stat line
    char moveIds[kMaxMoveSlots][kPvpIdCap] = {};   // equipped moves, by slot; empty = none
    char modIds[kModSlots][kPvpIdCap] = {};        // equipped mods, by slot; empty = none
    uint16_t level = 0;                            // combat level (display + level-diff)
    uint8_t statPoints[4] = {0, 0, 0, 0};          // power / defense / speed / maxHealth
};

// Frame kinds. Values are wire constants — append, never renumber.
enum class PvpFrameType : uint8_t {
    Invite = 1,
    Accept = 2,
    Decline = 3,
    Start = 4,
    Bye = 5,
};

// Why a challenge was refused. Carried on DECLINE so the challenger's screen can say
// which, instead of a bare "no" that reads identically to a bug.
enum class PvpDeclineReason : uint8_t {
    Refused = 0,     // the human pressed C
    Busy = 1,        // mid-duel, or not on the LINK screen to consent from
    NoPet = 2,       // no hatched pet to fight with
    Mismatch = 3,    // kPvpRulesVersion disagreement — the two builds would diverge
};

// A decoded frame. Only the members its `type` defines are meaningful; the rest keep
// their defaults, so a caller that switches on `type` never reads a stale field.
struct PvpFrame {
    PvpFrameType type = PvpFrameType::Bye;
    uint32_t session = 0;        // binds every frame of one duel
    uint8_t rules = 0;           // sender's kPvpRulesVersion (Invite/Accept)
    PvpFighter fighter;          // Invite/Accept
    PvpDeclineReason reason = PvpDeclineReason::Refused;  // Decline
    uint32_t seed = 0;           // Start — the fight's RNG seed
    uint32_t commit = 0;         // Start — pvpCommitHash over seed + both fighters
};

// Encoded sizes. Fixed per type: every field is a fixed-width slot, so there is no
// length-prefix parsing and a short/long frame is rejected outright.
constexpr size_t kPvpHeaderSize = 4 + 1 + 1 + 4;   // magic + proto version + type + session
constexpr size_t kPvpFighterSize = kPvpTagCap + kPvpIdCap + kMaxMoveSlots * kPvpIdCap +
                                    kModSlots * kPvpIdCap + 2 + 4;
constexpr size_t kPvpInviteSize = kPvpHeaderSize + 1 + kPvpFighterSize;  // +rules
constexpr size_t kPvpAcceptSize = kPvpInviteSize;
constexpr size_t kPvpDeclineSize = kPvpHeaderSize + 1;
constexpr size_t kPvpStartSize = kPvpHeaderSize + 4 + 4;
constexpr size_t kPvpByeSize = kPvpHeaderSize;

// ESP-NOW v1 carries 250 bytes per frame and this protocol never fragments, so the
// largest frame has to fit one. If a new field breaks this, shorten a cap or move the
// fighter to its own frame — do not add fragmentation for a beacon-grade protocol.
constexpr size_t kPvpMaxFrameSize = 250;
static_assert(kPvpInviteSize <= kPvpMaxFrameSize, "an INVITE must fit one ESP-NOW frame");

// The largest frame this build can produce — what a send buffer should be sized to.
constexpr size_t kPvpFrameCap = kPvpInviteSize;

// --- Encode ------------------------------------------------------------------
// Each returns the number of bytes written, or 0 when `out` is null or `cap` is too
// small. Strings are truncated to their field capacity.

size_t encodePvpInvite(uint32_t session, const PvpFighter& f, uint8_t* out, size_t cap);
size_t encodePvpAccept(uint32_t session, const PvpFighter& f, uint8_t* out, size_t cap);
size_t encodePvpDecline(uint32_t session, PvpDeclineReason reason, uint8_t* out, size_t cap);
size_t encodePvpStart(uint32_t session, uint32_t seed, uint32_t commit, uint8_t* out,
                      size_t cap);
size_t encodePvpBye(uint32_t session, uint8_t* out, size_t cap);

// --- Decode ------------------------------------------------------------------

// Parse a received frame. Returns false — leaving `out` untouched — unless the frame
// carries the duel magic, declares a protocol version this build understands, names a
// known type, and is exactly that type's length.
//
// On success every string field of `out` is guaranteed NUL-terminated and printable
// ASCII (wire_field.h), so a hostile sender cannot inject control bytes into the
// framebuffer through this path.
bool decodePvpFrame(const uint8_t* in, size_t len, PvpFrame* out);

// A cheap "is this even ours" test on the magic alone, without decoding. The radio
// hands every frame it hears to both seams (platform/esp32/net_link.h), so this is what
// lets the duel side ignore a HELLO beacon before it costs a buffer slot or a log line.
bool isPvpFrame(const uint8_t* in, size_t len);

// --- Agreement helpers -------------------------------------------------------

// Which device hosts, decided from the two packed radio MACs (packPeerKey) that both
// sides already have: the LOWER key hosts. No negotiation round, no tie — two distinct
// devices cannot share a MAC. The host picks the seed, sends START, and occupies
// Combat's player_ slot on BOTH screens (see core/model/pvp_battle.h).
bool pvpHostIsLocal(uint64_t localKey, uint64_t remoteKey);

// A hash over everything the host commits to when it sends START: the seed and both
// fighter specs, in host-then-guest order. The guest recomputes it from its own copies
// and refuses the duel if it differs.
//
// This is the divergence guard, not a security measure — it catches the cases that
// would otherwise produce two screens showing different fights (a spec that decoded
// differently, a stale session, mismatched builds) and turns them into a clean refusal.
// An attacker who can forge frames can forge this too; see the file header.
uint32_t pvpCommitHash(uint32_t seed, const PvpFighter& host, const PvpFighter& guest);

}  // namespace mal
