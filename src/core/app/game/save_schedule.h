// save_schedule.h: when the next save is due, the buffer every save is serialized into,
// and the counts that make a turned-away or refused write visible. Game::persistSave
// drives it; it holds no game state of its own.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "tunables.h"

namespace mal {

class SaveSchedule {
public:
    // A write is owed. persistSave also sets it on entry, so every guard that turns a
    // write away leaves it set and the next tick tries again.
    void markDirty() { dirty_ = true; }
    bool dirty() const { return dirty_; }

    // Whether the autosave should write now: a debounced write after a change, plus a
    // slower periodic one so passive decay is captured.
    bool due(uint32_t nowMs) const {
        const uint32_t since = nowMs - lastSaveMs_;
        return (dirty_ && since >= kSaveDebounceMs) || since >= kSaveAutosaveMs;
    }

    // The state in RAM matches what is on flash (a save was just loaded, or the store
    // was just wiped), so nothing is owed and the autosave clock starts over.
    void settle(uint32_t nowMs) {
        dirty_ = false;
        lastSaveMs_ = nowMs;
    }

    // Whether the buffer already holds room for the next save. One that does not still
    // has to allocate it in a single piece, which is why Game::saveHeapFloor asks.
    bool bufferReady() const { return blob_.capacity() >= targetBytes(); }

    // The buffer, grown first when this save needs more of it than the last one left.
    // Dropped before it is re-reserved so the peak is the new buffer alone.
    std::vector<uint8_t>& buffer() {
        if (!bufferReady()) {
            std::vector<uint8_t>().swap(blob_);
            blob_.reserve(targetBytes());
        }
        return blob_;
    }

    // The heap guard turned a write away with `headroom` bytes free.
    void noteDeferred(uint32_t headroom) {
        if (deferred_ == 0 || headroom < deferredLow_) deferredLow_ = headroom;
        ++deferred_;
    }

    // The store refused the blob in buffer(). The write stays owed; the clock still
    // advances, which paces the retries at kSaveDebounceMs instead of every beat.
    void noteWriteFailed(uint32_t nowMs) {
        lastBlobSize_ = blob_.size();
        ++writesFailed_;
        lastSaveMs_ = nowMs;
    }

    // The blob in buffer() is on flash.
    void noteLanded(uint32_t nowMs) {
        lastBlobSize_ = blob_.size();
        deferred_ = 0;
        writesFailed_ = 0;
        settle(nowMs);
    }

    // Consecutive heap-guard refusals and the smallest headroom seen doing it; both reset
    // when a write lands. The guard fires on a heap state that is gone by the time
    // anything samples it, so without the count a working guard looks like luck.
    int deferred() const { return deferred_; }
    uint32_t deferredLowMark() const { return deferredLow_; }
    // Consecutive writes the STORE refused. Non-zero means the blob on flash is older
    // than RAM, the one state that survives a reboot as lost progress.
    int writesFailed() const { return writesFailed_; }

private:
    // The last blob plus the growth state can put on between saves, never below the
    // reservation. The margin keeps a pet gaining a row off the allocating path.
    size_t targetBytes() const {
        const size_t want = lastBlobSize_ + kSaveBlobHeadroomBytes;
        return want < kSaveReserveBytes ? kSaveReserveBytes : want;
    }

    // Owning the buffer keeps a write off the allocator: it is sized once, behind the
    // higher floor, and reused, so a save landing while the radio holds the heap has
    // nothing large left to ask for.
    std::vector<uint8_t> blob_;
    size_t lastBlobSize_ = 0;
    bool dirty_ = false;
    uint32_t lastSaveMs_ = 0;
    int deferred_ = 0;
    uint32_t deferredLow_ = 0;
    int writesFailed_ = 0;
};

}  // namespace mal
