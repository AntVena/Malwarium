// idle_feeding.h: the resting flourish of a line that skims mail (RestingTrick::Skim).
// Every so often a few envelopes sink into the habitat like fish food and the pet goes
// after each in turn and eats it.
//
// Unlike the clock-only flourishes this one reads the pet's position: it decides which
// envelope the pet is after and where to stand to bite it, and the habitat steers the
// wander there (IdleWander::chase). Positions are the wander's: logical px, x off the
// shelf anchor, y up off the shelf.
#pragma once

#include <cstdint>

#include "core/model/idle_wander.h"   // kWanderHalfSpanX, kWanderRiseMax

namespace mal {

// In heartbeats (~4 per second) and logical px. A feed ~50s apart; envelopes enter at the
// top of the living box (the space between the two carousel tracks) a few beats apart
// and sink to the shelf. A letter left uneaten when the feed runs out shreds where it lies.
constexpr int kIdleFeedRestBeats = 200;
constexpr int kIdleFeedLetters = 3;
constexpr int kIdleFeedDropGapBeats = 6;
constexpr int kIdleFeedTopPx = 82;
constexpr int kIdleFeedSinkPx = 3;
constexpr int kIdleFeedBiteBeats = 2;
constexpr int kIdleFeedMaxBeats = 60;
// Where a pet's mouth is relative to its own anchor: kMouthForwardPx toward the way it
// faces, up to kMouthRisePx above it. A bite lands within kMouthReachPx sideways.
constexpr int kIdleFeedMouthForwardPx = 18;
constexpr int kIdleFeedMouthRisePx = 18;
constexpr int kIdleFeedMouthReachPx = 6;
// Letters land as far out as a pet at the edge of its wander can still reach with its
// mouth, and alternate edges, so each one is a crossing of the habitat. kIdleFeedLaneJitterPx
// keeps the same edge from being the same spot.
constexpr int kIdleFeedDropHalfSpanPx = kWanderHalfSpanX + kIdleFeedMouthForwardPx;
constexpr int kIdleFeedLaneJitterPx = 8;

enum class MailState : uint8_t { Unborn, Sinking, Bitten, Gone };

struct Mail {
    MailState state = MailState::Unborn;
    int x = 0, y = 0;
    int side = 0;        // -1: the pet bites it from the left, +1 from the right, 0 unset
    int biteBeat = 0;    // beats since bitten
};

class IdleFeeding {
public:
    // `petX`/`petY` are where the wander has the pet now.
    void step(bool feeder, int petX, int petY) {
        if (!feeder) { reset(); beat_ = 0; return; }
        if (++beat_ <= kIdleFeedRestBeats) return;
        const int t = beat_ - kIdleFeedRestBeats - 1;
        if (t % kIdleFeedDropGapBeats == 0 && t / kIdleFeedDropGapBeats < kIdleFeedLetters)
            drop(letters_[t / kIdleFeedDropGapBeats], t / kIdleFeedDropGapBeats);
        bool live = false;
        for (Mail& b : letters_) {
            advance(b, petX, petY, t >= kIdleFeedMaxBeats);
            live |= b.state != MailState::Gone;
        }
        if (!live) { reset(); beat_ = 0; }
    }

    bool feeding() const { return beat_ > kIdleFeedRestBeats; }
    const Mail& letter(int i) const { return letters_[i]; }

    // The envelope the pet is after: the oldest still sinking. Null when there is none.
    const Mail* target() const {
        for (const Mail& b : letters_)
            if (b.state == MailState::Sinking) return &b;
        return nullptr;
    }
    // Where the pet stands to bite `b`, and which way it faces to do it.
    static int standX(const Mail& b) { return b.x + b.side * kIdleFeedMouthForwardPx; }
    static int standY(const Mail& b) {
        const int y = b.y - kIdleFeedMouthRisePx / 2;
        return y < 0 ? 0 : (y > kWanderRiseMax ? kWanderRiseMax : y);
    }
    static bool faceRight(const Mail& b) { return b.side < 0; }

    // Mid-bite, for the attack pose.
    bool biting() const {
        for (const Mail& b : letters_)
            if (b.state == MailState::Bitten) return true;
        return false;
    }

private:
    void reset() {
        for (Mail& b : letters_) b = Mail{};
    }

    uint32_t roll(uint32_t span) {
        rng_ = rng_ * 1664525u + 1013904223u;
        return (rng_ >> 16) % span;
    }

    void drop(Mail& b, int n) {
        if (n == 0) firstEdge_ = roll(2) ? 1 : -1;
        const int edge = (n & 1) ? -firstEdge_ : firstEdge_;
        b.state = MailState::Sinking;
        b.x = edge * (kIdleFeedDropHalfSpanPx - static_cast<int>(roll(kIdleFeedLaneJitterPx + 1)));
        b.y = kIdleFeedTopPx;
        b.side = 0;
    }

    // Approach from the side the pet is on, unless standing there would be off the
    // wander's span, in which case from the other.
    static int pickSide(const Mail& b, int petX) {
        int side = petX <= b.x ? -1 : 1;
        const int stand = b.x + side * kIdleFeedMouthForwardPx;
        if (stand > kWanderHalfSpanX || stand < -kWanderHalfSpanX) side = -side;
        return side;
    }

    void advance(Mail& b, int petX, int petY, bool expired) {
        switch (b.state) {
            case MailState::Unborn:
            case MailState::Gone:
                return;
            case MailState::Bitten:
                if (++b.biteBeat >= kIdleFeedBiteBeats) b.state = MailState::Gone;
                return;
            case MailState::Sinking:
                break;
        }
        if (b.side == 0) b.side = pickSide(b, petX);
        b.y = b.y > kIdleFeedSinkPx ? b.y - kIdleFeedSinkPx : 0;
        const int mouthX = petX - b.side * kIdleFeedMouthForwardPx;
        const int dx = mouthX - b.x;
        const bool inReach = dx <= kIdleFeedMouthReachPx && dx >= -kIdleFeedMouthReachPx &&
                             b.y <= petY + kIdleFeedMouthRisePx && b.y >= petY;
        if (inReach || expired) { b.state = MailState::Bitten; b.biteBeat = 0; }
    }

    int beat_ = 0;
    int firstEdge_ = 1;
    uint32_t rng_ = 0x9e3779b9u;
    Mail letters_[kIdleFeedLetters];
};

}  // namespace mal
