// pet_model.h — the pet's vitals + care budget: the data STAT reads and the
// raising loop mutates. Pure logic, fully unit-testable.
#pragma once

#include <cstdint>

namespace mal {

// A status zone after polarity is applied. OK -> calm, Caution -> warn,
// Critical -> hot (+pulse). Meaning is invariant across stats.
enum class Zone { Ok, Caution, Critical };

// Care-budget branch derived from the mistake count.
enum class CareBranch { Good, Bad, Dying };

class PetModel {
public:
    PetModel();

    // Vitals, all 0..100.
    int hunger() const { return hunger_; }
    int fragmentation() const { return fragmentation_; }
    int happiness() const { return happiness_; }
    int careMistakes() const { return mistakes_; }

    // Zone classification per stat (thresholds from tunables.h).
    Zone hungerZone() const;
    Zone fragZone() const;
    Zone happyZone() const;
    CareBranch careBranch() const;

    // Idle-canvas hunger alert fires in the Hunger Critical band.
    bool isHungry() const { return hungerZone() == Zone::Critical; }
    // Lockout crisis fires when Hunger bottoms out.
    bool isStarving() const { return hunger_ == 0; }

    // Replication Ghost (FX_GHOST): the worm-line elevated-drain state, cleared
    // by an AV scan (MAINT). Debuffs are a placeholder count AV also clears.
    bool hasGhost() const { return ghost_; }
    void setGhost(bool g) { ghost_ = g; }
    int debuffs() const { return debuffs_; }
    void setDebuffs(int n) { debuffs_ = n < 0 ? 0 : n; }

    // Advance the pet clock by `elapsedMs` of game-time, applying decay.
    void tick(uint32_t elapsedMs);

    // --- Raising-loop actions -----------------------------------------
    // Feed: restore Hunger, add Happiness (food: happy=0 in v1; buffs: hunger=0).
    void feed(int hungerPts, int happyPts);
    // Defrag: success -kDefragReduction frag; failure +kMaintFailPenalty. Returns TRUE
    // on a FAILURE so the Game-level caller can apply the +1 care mistake through the
    // SHIELDED path (v21 Restore Point covers maint fails too) — PetModel no longer
    // touches the mistake counter itself.
    bool applyDefrag(bool success);
    // AV: success -kAvReduction frag + clears ghost/debuffs; failure as Defrag.
    // Returns TRUE on a FAILURE (same shielded-mistake contract as applyDefrag).
    bool applyAntivirus(bool success);

    // --- Mutators the raising loop / tests use ------------------------------
    void setHunger(int v);
    void setFragmentation(int v);
    void setHappiness(int v);
    void setCareMistakes(int v);
    void addCareMistake(int n = 1);

private:
    int hunger_;
    int fragmentation_;
    int happiness_;
    int mistakes_;
    bool ghost_ = false;
    int debuffs_ = 0;
    // Fractional decay accumulators (game-ms carried between ticks).
    uint32_t hungerAccumMs_ = 0;
    uint32_t happyAccumMs_ = 0;
};

} // namespace mal
