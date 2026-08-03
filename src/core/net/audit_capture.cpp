#include "core/net/audit_capture.h"

namespace mal {

void AuditCapture::seal(uint32_t nowMs) {
    phase_ = AuditPhase::Cooldown;
    sealed_ = true;
    sealMs_ = nowMs;
}

bool AuditCapture::canArm(uint32_t nowMs) const {
    if (!sealed_) return true;
    return (nowMs - sealMs_) >= kAuditRearmCooldownMs;
}

void AuditCapture::setEnabled(bool on, uint32_t nowMs) {
    if (on == enabled_) return;
    enabled_ = on;
    if (on) {
        // Arm now if allowed; otherwise stay in Cooldown and let tick() re-arm
        // once the cooldown clears.
        if (phase_ == AuditPhase::Disarmed && canArm(nowMs)) {
            phase_ = AuditPhase::Armed;
            armedAtMs_ = nowMs;         // start the bounded-arming listen window
        }
    } else {
        // Turning the toggle off seals a live/armed session (and starts cooldown).
        if (phase_ == AuditPhase::Armed || phase_ == AuditPhase::Hot)
            seal(nowMs);
        else if (phase_ == AuditPhase::Disarmed)
            phase_ = AuditPhase::Disarmed;  // already down; nothing to seal
    }
}

void AuditCapture::noteHandshake(uint32_t nowMs) {
    if (phase_ != AuditPhase::Armed) return;   // only an armed listener captures
    phase_ = AuditPhase::Hot;
    hotUntilMs_ = nowMs + kAuditHotBroadcastMs;
}

void AuditCapture::tick(uint32_t nowMs) {
    if (phase_ == AuditPhase::Hot) {
        if (static_cast<int32_t>(nowMs - hotUntilMs_) >= 0) seal(nowMs);
        return;
    }
    if (phase_ == AuditPhase::Armed) {
        // Bounded arming: a listen session that never captures self-seals rather
        // than keeping the radio hot forever (the seal->cooldown path re-arms it
        // later, since the toggle intent is untouched).
        if ((nowMs - armedAtMs_) >= kAuditArmWindowMs) seal(nowMs);
        return;
    }
    if (phase_ == AuditPhase::Cooldown && canArm(nowMs)) {
        if (enabled_) {
            phase_ = AuditPhase::Armed;
            armedAtMs_ = nowMs;         // fresh listen window on each re-arm
        } else {
            phase_ = AuditPhase::Disarmed;
        }
    }
}

void AuditCapture::softSeal(uint32_t nowMs) {
    if (phase_ == AuditPhase::Hot) seal(nowMs);
}

void AuditCapture::sealActive(uint32_t nowMs) {
    if (phase_ == AuditPhase::Armed || phase_ == AuditPhase::Hot) seal(nowMs);
}

uint32_t AuditCapture::hotRemainingMs(uint32_t nowMs) const {
    if (phase_ != AuditPhase::Hot) return 0;
    return (static_cast<int32_t>(hotUntilMs_ - nowMs) > 0) ? (hotUntilMs_ - nowMs) : 0;
}

uint32_t AuditCapture::cooldownRemainingMs(uint32_t nowMs) const {
    if (phase_ != AuditPhase::Cooldown || !sealed_) return 0;
    const uint32_t elapsed = nowMs - sealMs_;
    return (elapsed < kAuditRearmCooldownMs) ? (kAuditRearmCooldownMs - elapsed) : 0;
}

} // namespace mal
