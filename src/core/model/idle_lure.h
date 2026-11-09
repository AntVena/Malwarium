// idle_lure.h: the other half of a skimming line's rest (RestingTrick::Skim): it sets
// bait. A few prize tags go out on lines from the pet like an angler's lures and bob
// there; a cursor comes in from the edge of the screen, clicks one, and is reeled in
// with it and eaten.
//
// Same split as IdleCamo: this half decides when, which tag and which edge, and the
// habitat decides where the tags hang and how each piece is drawn. It runs between the
// mail skims (core/model/idle_feeding.h), never alongside one: the tick steps each only
// while the other is at rest, and this one starts its clock half a rest ahead so the two
// take turns.
#pragma once

#include <cstdint>

namespace mal {

// In heartbeats (~4 per second). The pet holds still for the whole of it: an angler
// waits.
constexpr int kIdleLureRestBeats = 400;
constexpr int kIdleLureTags = 3;
constexpr int kIdleLureCastBeats = 3;
constexpr int kIdleLureDangleBeats = 8;
constexpr int kIdleLureApproachBeats = 6;
constexpr int kIdleLureHoverBeats = 2;
constexpr int kIdleLureClickBeats = 2;
constexpr int kIdleLureReelBeats = 4;
constexpr int kIdleLureEatBeats = 2;

enum class LureStage : uint8_t { Rest, Cast, Dangle, Approach, Hover, Click, Reel, Eat };

namespace lure_detail {
constexpr int kStages = 8;
constexpr int kLen[kStages] = {
    kIdleLureRestBeats,     kIdleLureCastBeats,  kIdleLureDangleBeats,
    kIdleLureApproachBeats, kIdleLureHoverBeats, kIdleLureClickBeats,
    kIdleLureReelBeats,     kIdleLureEatBeats,
};
constexpr int from(int s) { return s == 0 ? 0 : from(s - 1) + kLen[s - 1]; }
}  // namespace lure_detail

class IdleLure {
public:
    void step(bool angler) {
        if (!angler) { beat_ = kFirstBeat; return; }
        if (++beat_ >= kCycleBeats) beat_ = 0;
        if (beat_ == lure_detail::from(1)) {
            target_ = static_cast<int>(roll(kIdleLureTags));
            edge_ = roll(2) ? 1 : -1;
        }
    }

    LureStage stage() const {
        int s = 0;
        while (s + 1 < lure_detail::kStages && beat_ >= lure_detail::from(s + 1)) ++s;
        return static_cast<LureStage>(s);
    }
    bool active() const { return stage() != LureStage::Rest; }
    // 0-based beat within stage(). Rest's value is meaningless.
    int beatInStage() const { return beat_ - lure_detail::from(static_cast<int>(stage())); }
    static int stageBeats(LureStage s) { return lure_detail::kLen[static_cast<int>(s)]; }

    // The tag the cursor goes for, and the side of the screen it comes from (-1 left).
    int target() const { return target_; }
    int edge() const { return edge_; }

    // Jaws closing on the catch, for the attack pose.
    bool biting() const { return stage() == LureStage::Eat; }

private:
    static constexpr int kCycleBeats = lure_detail::from(lure_detail::kStages);
    static constexpr int kFirstBeat = kIdleLureRestBeats / 2;

    uint32_t roll(uint32_t span) {
        rng_ = rng_ * 1664525u + 1013904223u;
        return (rng_ >> 16) % span;
    }

    int beat_ = kFirstBeat;
    int target_ = 0;
    int edge_ = 1;
    uint32_t rng_ = 0x51ed2701u;
};

}  // namespace mal
