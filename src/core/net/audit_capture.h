// audit_capture.h — the Audit-capture policy state machine.
//
// PURE + HOST-TESTABLE. This owns *when* passive handshake capture is allowed and
// how the post-capture "hot & broadcasting" window and the re-arm cooldown behave.
// It holds no radio, no SD, no frames — the device tier calls noteHandshake() when
// its promiscuous sniffer actually observes a WPA 4-way handshake (EAPOL), and
// reads broadcasting()/phase() to drive the RF + UI. The .pcap bytes go through
// pcap.h; this is only the timing/gating brain, so the whole policy is unit-tested
// natively with a fake clock.
//
// AUTHORIZED USE / PASSIVE ONLY. Arming is an affirmative opt-in (Game persists the
// toggle, save v6). Turning the toggle off *seals* immediately and starts a long
// re-arm cooldown; the hot window also seals when it expires. There is deliberate
// friction (the cooldown) so the capture path can't be left continuously armed.
//
// Timers use the engine's monotonic game-ms (nowMs), which resets on reboot — like
// Lockout / Bandwidth. So only the toggle *intent* is persisted; a reboot clears any
// hot/cooldown window (there is no RTC, / config.h HAS_HARDWARE_RTC=0),
// which is the established v1 "reboot resets the clock" rule. It holds for the
// PENALTY windows, which is all of them but one: refunding a penalty costs a little
// cheese, so the simple thing wins. The exception is the 5/5 CSF recovery window
// (game_core.cpp, save v42) — that one is the last step before permanent loss, so it
// accumulates and persists instead.
#pragma once

#include <cstdint>

#include "tunables.h"

namespace mal {

// Disarmed  — toggle off (or cooled-down with the toggle off): no capture.
// Armed     — toggle on, listening; a handshake will be captured.
// Hot       — a handshake was just captured; broadcasting for ~kAuditHotBroadcastMs.
// Cooldown  — sealed (toggle-off OR hot window expired); re-arm blocked until
//             kAuditRearmCooldownMs elapses (then auto-arms iff the toggle is on).
enum class AuditPhase : uint8_t { Disarmed, Armed, Hot, Cooldown };

class AuditCapture {
public:
    AuditPhase phase() const { return phase_; }
    bool enabled() const { return enabled_; }             // toggle intent (persisted)
    bool armed() const { return phase_ == AuditPhase::Armed; }
    bool broadcasting() const { return phase_ == AuditPhase::Hot; }
    // Whether the device should actually run promiscuous capture right now.
    bool capturing() const {
        return phase_ == AuditPhase::Armed || phase_ == AuditPhase::Hot;
    }

    // Player flips the authorized-use opt-in. Turning it ON arms immediately when
    // not cooling (else it auto-arms once the cooldown clears, via tick). Turning
    // it OFF seals a live/armed session and starts the re-arm cooldown.
    void setEnabled(bool on, uint32_t nowMs);

    // The device saw a real WPA 4-way handshake (EAPOL) and wrote it to the .pcap.
    // Armed -> Hot (starts the broadcast window). Ignored in any other phase (can't
    // capture while disarmed, cooling, or already hot).
    void noteHandshake(uint32_t nowMs);

    // Periodic (call from Game::tick). Expires the hot window (-> seal -> cooldown),
    // self-seals an Armed session that has listened past kAuditArmWindowMs with no
    // capture (bounded arming — the radio can't be left hot forever), and, once the
    // cooldown elapses, re-arms if the toggle is still on.
    void tick(uint32_t nowMs);

    // Leaving the audit event soft-seals a live broadcast: stop broadcasting and
    // start the cooldown, without changing the toggle intent (so it re-arms later).
    void softSeal(uint32_t nowMs);

    // Seal a live session from EITHER Armed or Hot, keeping the toggle intent (so
    // it re-arms after the cooldown). The device tier calls this for the low-battery
    // guard — refuse to keep the radio hot when the pack is low, but don't forget
    // the player's opt-in. No-op unless currently Armed/Hot.
    void sealActive(uint32_t nowMs);

    // Cooldown elapsed (or never sealed) — a fresh arm/capture is permitted.
    bool canArm(uint32_t nowMs) const;

    // 0 unless Hot / Cooldown respectively; both clamp at 0 (never underflow).
    uint32_t hotRemainingMs(uint32_t nowMs) const;
    uint32_t cooldownRemainingMs(uint32_t nowMs) const;

private:
    void seal(uint32_t nowMs);   // -> Cooldown, latch the cooldown clock

    AuditPhase phase_ = AuditPhase::Disarmed;
    bool enabled_ = false;       // toggle intent (Game persists this, save v6)
    bool sealed_ = false;        // has ever sealed -> the cooldown clock is valid
    uint32_t hotUntilMs_ = 0;    // Hot expires at this game-ms
    uint32_t sealMs_ = 0;        // when the last seal happened (cooldown origin)
    uint32_t armedAtMs_ = 0;     // when the current Armed window began (arm-window)
};

} // namespace mal
