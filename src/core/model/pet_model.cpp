#include "core/model/pet_model.h"

#include "tunables.h"

namespace mal {

namespace {
int clamp100(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }
}

PetModel::PetModel()
    : hunger_(kStartHunger),
      fragmentation_(kStartFragmentation),
      happiness_(kStartHappiness),
      mistakes_(0) {}

Zone PetModel::hungerZone() const {
    if (hunger_ <= kHungerCriticalMax) return Zone::Critical;
    if (hunger_ <= kHungerCautionMax) return Zone::Caution;
    return Zone::Ok;
}

Zone PetModel::fragZone() const {
    if (fragmentation_ >= kFragCriticalMin) return Zone::Critical;
    if (fragmentation_ >= kFragCautionMin) return Zone::Caution;
    return Zone::Ok;
}

Zone PetModel::happyZone() const {
    if (happiness_ <= kHappyCriticalMax) return Zone::Critical;
    if (happiness_ < kHappyCautionMin) return Zone::Caution;
    return Zone::Ok;
}

CareBranch PetModel::careBranch() const {
    if (mistakes_ >= kCareDying) return CareBranch::Dying;
    if (mistakes_ > kCareGoodMax) return CareBranch::Bad;
    return CareBranch::Good;
}

void PetModel::tick(uint32_t elapsedMs) {
    constexpr uint32_t kMs = 60u * 1000u;
    hungerAccumMs_ += elapsedMs;
    happyAccumMs_ += elapsedMs;
    const uint32_t hungerStep = kHungerMinutesPerPoint * kMs;
    const uint32_t happyStep = kHappinessMinutesPerPoint * kMs;
    while (hungerAccumMs_ >= hungerStep) {
        hungerAccumMs_ -= hungerStep;
        hunger_ = clamp100(hunger_ - 1);
    }
    while (happyAccumMs_ >= happyStep) {
        happyAccumMs_ -= happyStep;
        happiness_ = clamp100(happiness_ - 1);
    }
    // Fragmentation does not decay passively.
}

void PetModel::feed(int hungerPts, int happyPts) {
    hunger_ = clamp100(hunger_ + hungerPts);
    happiness_ = clamp100(happiness_ + happyPts);
}

bool PetModel::applyDefrag(bool success) {
    if (success) {
        fragmentation_ = clamp100(fragmentation_ - kDefragReduction);
        return false;
    }
    fragmentation_ = clamp100(fragmentation_ + kMaintFailPenalty);
    return true;   // the +1 care mistake is applied Game-side (shielded)
}

void PetModel::applyPlayedDefrag(int fragRemoved) {
    if (fragRemoved <= 0) return;                  // a board worth nothing changes nothing
    fragmentation_ = clamp100(fragmentation_ - fragRemoved);
}

bool PetModel::applyAntivirus(bool success) {
    if (success) {
        fragmentation_ = clamp100(fragmentation_ - kAvReduction);
        ghost_ = false;
        debuffs_ = 0;
        return false;
    }
    fragmentation_ = clamp100(fragmentation_ + kMaintFailPenalty);
    return true;   // the +1 care mistake is applied Game-side (shielded)
}

void PetModel::setHunger(int v) { hunger_ = clamp100(v); }
void PetModel::setFragmentation(int v) { fragmentation_ = clamp100(v); }
void PetModel::setHappiness(int v) { happiness_ = clamp100(v); }

void PetModel::setCareMistakes(int v) {
    mistakes_ = v < 0 ? 0 : (v > kCareDying ? kCareDying : v);
}
void PetModel::addCareMistake(int n) { setCareMistakes(mistakes_ + n); }

} // namespace mal
