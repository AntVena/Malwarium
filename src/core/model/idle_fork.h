// idle_fork.h: the resting flourish of a line that replicates (LinePassive::Replication).
//
// Every so often the pet makes one copy more than it has a replication slot for. The
// copy crawls out to where that slot would stand, looks both ways, and dies with the
// same dissolve a copy killed in a fight plays. It shows the line's cap at home, where
// the replicas are otherwise only scenery.
//
// Same split as IdleCamo: this half decides when and how far through, and the habitat
// decides where and with which glyph.
#pragma once

#include <cstdint>

namespace mal {

// In heartbeats (~4 per second). ~45s apart, ~4s long, so a fork reads as a thing the
// pet did rather than part of its idle loop. Crawl is long enough to cover two replica
// slots at a readable pace; Die is the glyph's two dissolve frames, once each.
constexpr int kIdleForkRestBeats = 180;
constexpr int kIdleForkCrawlBeats = 8;
constexpr int kIdleForkLookBeats = 6;
constexpr int kIdleForkDieBeats = 2;

enum class ForkStage : uint8_t { Rest, Crawl, Look, Die };

class IdleFork {
public:
    // Passed every beat for the reason IdleCamo::step gives: a hatch, evolution or load
    // just starts arriving with a different answer.
    void step(bool forker) {
        if (!forker) { beat_ = 0; return; }
        if (++beat_ >= kCycleBeats) beat_ = 0;
    }

    ForkStage stage() const {
        if (beat_ < kCrawlFrom) return ForkStage::Rest;
        if (beat_ < kLookFrom) return ForkStage::Crawl;
        if (beat_ < kDieFrom) return ForkStage::Look;
        return ForkStage::Die;
    }

    // 0-based beat within stage(). Rest's value is meaningless and never read.
    int beatInStage() const {
        switch (stage()) {
            case ForkStage::Crawl: return beat_ - kCrawlFrom;
            case ForkStage::Look:  return beat_ - kLookFrom;
            case ForkStage::Die:   return beat_ - kDieFrom;
            case ForkStage::Rest:  break;
        }
        return beat_;
    }

private:
    static constexpr int kCrawlFrom = kIdleForkRestBeats;
    static constexpr int kLookFrom = kCrawlFrom + kIdleForkCrawlBeats;
    static constexpr int kDieFrom = kLookFrom + kIdleForkLookBeats;
    static constexpr int kCycleBeats = kDieFrom + kIdleForkDieBeats;

    int beat_ = 0;
};

}  // namespace mal
