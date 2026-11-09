// idle_feeding.h: the resting flourish of a line that skims mail (RestingTrick::Skim).
// Every so often scraps of somebody's identity (a letter, an address, a password) sink
// into the habitat like fish food and the pet goes after each in turn and eats it. Then
// it puts them to use: it squats a lookalike of its own name and logs in with what it
// took.
//
// Unlike the clock-only flourishes this one reads the pet's position: it decides which
// scrap the pet is after and where to stand to bite it, and the habitat steers the
// wander there (IdleWander::chase). Positions are the wander's: logical px, x off the
// shelf anchor, y up off the shelf.
#pragma once

#include <cstdint>

#include "core/model/idle_wander.h"   // kWanderHalfSpanX, kWanderRiseMax

namespace mal {

// In heartbeats (~4 per second) and logical px. A feed ~100s apart, with the line's lure
// (core/model/idle_lure.h) halfway between; scraps enter at the top of the living box
// (the space between the two carousel tracks) a few beats apart and sink to the shelf.
// A scrap left uneaten when the feed runs out shreds where it lies.
constexpr int kIdleFeedRestBeats = 400;
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
// Scraps land as far out as a pet at the edge of its wander can still reach with its
// mouth, and alternate edges, so each one is a crossing of the habitat. kIdleFeedLaneJitterPx
// keeps the same edge from being the same spot.
constexpr int kIdleFeedDropHalfSpanPx = kWanderHalfSpanX + kIdleFeedMouthForwardPx;
constexpr int kIdleFeedLaneJitterPx = 8;
// After the feed. The squat shows the pet's name, swaps one letter for a lookalike on
// kIdleSquatSwapBeat, and holds the lookalike through the login that follows. The login
// types the lookalike in, then the password, then grants access and tears away.
constexpr int kIdleSquatBeats = 8;
constexpr int kIdleSquatSwapBeat = 3;
constexpr int kIdleLoginTypeBeats = 6;
constexpr int kIdleLoginPassDots = 4;
constexpr int kIdleLoginGrantBeats = 5;
constexpr int kIdleLoginTearBeats = 2;
constexpr int kIdleLoginBeats =
    kIdleLoginTypeBeats + kIdleLoginPassDots + kIdleLoginGrantBeats + kIdleLoginTearBeats;

enum class MailState : uint8_t { Unborn, Sinking, Bitten, Gone };
enum class MailKind : uint8_t { Letter, Address, Password };
constexpr int kMailKinds = 3;

struct Mail {
    MailState state = MailState::Unborn;
    MailKind kind = MailKind::Letter;
    int x = 0, y = 0;
    int side = 0;        // -1: the pet bites it from the left, +1 from the right, 0 unset
    int biteBeat = 0;    // beats since bitten
};

enum class FeedStage : uint8_t { Rest, Feed, Squat, Login };
enum class LoginStep : uint8_t { User, Pass, Granted, Torn };

class IdleFeeding {
public:
    // `petX`/`petY` are where the wander has the pet now.
    void step(bool feeder, int petX, int petY) {
        if (!feeder) { reset(); return; }
        ++beat_;
        switch (stage_) {
            case FeedStage::Rest:
                if (beat_ > kIdleFeedRestBeats) enter(FeedStage::Feed);
                return;
            case FeedStage::Feed:
                feed(petX, petY);
                return;
            case FeedStage::Squat:
                if (beat_ >= kIdleSquatBeats) enter(FeedStage::Login);
                return;
            case FeedStage::Login:
                if (beat_ >= kIdleLoginBeats) enter(FeedStage::Rest);
                return;
        }
    }

    FeedStage stage() const { return stage_; }
    bool feeding() const { return stage_ == FeedStage::Feed; }
    bool active() const { return stage_ != FeedStage::Rest; }
    // 0-based beat within stage(). Rest's value is meaningless.
    int beatInStage() const { return beat_; }
    const Mail& letter(int i) const { return letters_[i]; }

    // Is the name showing the lookalike yet: the squat past its swap, or the login.
    bool squatted() const {
        return (stage_ == FeedStage::Squat && beat_ >= kIdleSquatSwapBeat) ||
               stage_ == FeedStage::Login;
    }
    LoginStep loginStep() const {
        if (beat_ < kIdleLoginTypeBeats) return LoginStep::User;
        if (beat_ < kIdleLoginTypeBeats + kIdleLoginPassDots) return LoginStep::Pass;
        if (beat_ < kIdleLoginBeats - kIdleLoginTearBeats) return LoginStep::Granted;
        return LoginStep::Torn;
    }
    // How many of `len` user-name characters are typed in, and how many password dots.
    int typed(int len) const {
        if (beat_ >= kIdleLoginTypeBeats) return len;
        return len * (beat_ + 1) / kIdleLoginTypeBeats;
    }
    int dots() const {
        const int d = beat_ - kIdleLoginTypeBeats + 1;
        return d < 0 ? 0 : (d > kIdleLoginPassDots ? kIdleLoginPassDots : d);
    }

    // The scrap the pet is after: the oldest still sinking. Null when there is none.
    const Mail* target() const {
        if (stage_ != FeedStage::Feed) return nullptr;
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
        if (stage_ != FeedStage::Feed) return false;
        for (const Mail& b : letters_)
            if (b.state == MailState::Bitten) return true;
        return false;
    }

private:
    void reset() {
        stage_ = FeedStage::Rest;
        beat_ = 0;
        for (Mail& b : letters_) b = Mail{};
    }

    void enter(FeedStage s) {
        stage_ = s;
        beat_ = 0;
        if (s == FeedStage::Rest || s == FeedStage::Feed)
            for (Mail& b : letters_) b = Mail{};
    }

    uint32_t roll(uint32_t span) {
        rng_ = rng_ * 1664525u + 1013904223u;
        return (rng_ >> 16) % span;
    }

    void feed(int petX, int petY) {
        const int t = beat_ - 1;
        if (t % kIdleFeedDropGapBeats == 0 && t / kIdleFeedDropGapBeats < kIdleFeedLetters)
            drop(letters_[t / kIdleFeedDropGapBeats], t / kIdleFeedDropGapBeats);
        bool live = false;
        for (Mail& b : letters_) {
            advance(b, petX, petY, t >= kIdleFeedMaxBeats);
            live |= b.state != MailState::Gone;
        }
        if (!live) enter(FeedStage::Squat);
    }

    void drop(Mail& b, int n) {
        if (n == 0) {
            firstEdge_ = roll(2) ? 1 : -1;
            firstKind_ = static_cast<int>(roll(kMailKinds));
        }
        const int edge = (n & 1) ? -firstEdge_ : firstEdge_;
        b.state = MailState::Sinking;
        b.kind = static_cast<MailKind>((firstKind_ + n) % kMailKinds);
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

    FeedStage stage_ = FeedStage::Rest;
    int beat_ = 0;
    int firstEdge_ = 1;
    int firstKind_ = 0;
    uint32_t rng_ = 0x9e3779b9u;
    Mail letters_[kIdleFeedLetters];
};

}  // namespace mal
