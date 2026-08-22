#include "core/model/mod_state.h"

namespace mal {

namespace {

// The combine contract for every ModEffect that keeps per-fight state. A kind absent
// from this table keeps none: the stat kinds land straight on the Combatant's base
// stats at build time, and the post-battle kinds are read by the Game off ModDef.
//
// The comparison rules are what stop two copies of a guarantee from cancelling each
// other out: the TIGHTEST cap wins (LowestMag) so stacking a second ECC Memory or
// Watchdog Timer can never loosen the ceiling the first one bought, and the STRONGEST
// cut wins (HighestMag) so a weaker Faraday Cage or Canary Trap can never dilute a
// better one. `mag2` rides along with whichever copy won, because a threshold and its
// payload are one setting — pairing Meltdown Core's 30% threshold with another copy's
// 40% power bonus would invent a mod neither row declares.
constexpr ModRule kModRules[] = {
    {ModEffect::RaidMirror,           ModCombine::Arm,         0,   true},
    {ModEffect::Thorns,               ModCombine::Sum,         0,   false},
    {ModEffect::DeathBlast,           ModCombine::Sum,         0,   true},
    {ModEffect::MaxHitCapPct,         ModCombine::LowestMag,   0,   false},
    {ModEffect::LoadBalance,          ModCombine::LowestMag,   0,   false},
    {ModEffect::WatchdogClamp,        ModCombine::LowestMag,   0,   false},
    {ModEffect::FaradayCut,           ModCombine::HighestMag,  100, false},
    {ModEffect::FirstStrikeRankMult,  ModCombine::Arm,         0,   true},
    {ModEffect::FirstHitCutPct,       ModCombine::HighestMag,  0,   true},
    {ModEffect::LowHealthPowerPct,    ModCombine::HighestMag,  0,   false},
    // Zero-Day is a gamble, not a guarantee — there is no "better" chance/payout pair
    // to pick between (a higher chance may pay less), so the last slot walked defines
    // it rather than a comparison inventing a preference.
    {ModEffect::GambleBattlePowerPct, ModCombine::Replace,     0,   false},
    // Tripwire's threshold is its mag2, so the widest window wins and brings its own
    // reflect magnitude with it.
    {ModEffect::ConditionalThorns,    ModCombine::HighestMag2, 0,   false},
    {ModEffect::StealAmplifyPct,      ModCombine::Sum,         0,   false},
    // Both line build-arounds add percentage POINTS to a passive's own roll, so they sum
    // the way any other bonus to the same roll would, and both cap at 100 — the roll they
    // feed is a percent chance, and a mod that could push one past certainty would turn a
    // passive the whole line is balanced around into a guarantee.
    {ModEffect::ExecOverridePct,      ModCombine::Sum,         100, false},
    {ModEffect::ReplicaSpawnPct,      ModCombine::Sum,         100, false},
    // Mutation Engine multiplies a COUNT rather than feeding a roll, so it takes no cap of
    // its own: what it scales is how many different effect kinds a fighter reached for, and
    // that is already bounded by the vocabulary moveEffectMask can report.
    {ModEffect::PolymorphEffectPct,   ModCombine::Sum,         0,   false},
    // Both scale a quantity rather than feeding a percent roll, so neither takes the 100
    // cap its two neighbours above do — there is no certainty for them to push past.
    //
    // The ledger carries TWO magnitudes (standing, and again while holding a seizure), and
    // Sum only ever accumulates the first — so it takes HighestMag, which is what every
    // other two-magnitude row here uses (LowHealthPowerPct is the shape).
    {ModEffect::ExtortionLedger,       ModCombine::HighestMag,  0,   false},
    {ModEffect::ReplicaWorthPct,      ModCombine::Sum,         0,   false},
};

}  // namespace

const ModRule* modRule(ModEffect kind) {
    for (const ModRule& r : kModRules)
        if (r.kind == kind) return &r;
    return nullptr;
}

ModState* ModStateSet::find(ModEffect kind) {
    for (int i = 0; i < n_; ++i)
        if (e_[i].kind == kind) return &e_[i];
    return nullptr;
}

const ModState* ModStateSet::find(ModEffect kind) const {
    for (int i = 0; i < n_; ++i)
        if (e_[i].kind == kind) return &e_[i];
    return nullptr;
}

int ModStateSet::mag(ModEffect kind) const {
    const ModState* s = find(kind);
    return s ? s->mag : 0;
}

int ModStateSet::mag2(ModEffect kind) const {
    const ModState* s = find(kind);
    return s ? s->mag2 : 0;
}

bool ModStateSet::armed(ModEffect kind) const {
    const ModState* s = find(kind);
    return s && s->pending > 0;
}

bool ModStateSet::spend(ModEffect kind) {
    ModState* s = find(kind);
    if (!s || s->pending <= 0) return false;
    --s->pending;
    return true;
}

ModState* ModStateSet::slot(ModEffect kind) {
    if (ModState* s = find(kind)) return s;
    if (n_ >= kModSlots) return nullptr;
    ModState* s = &e_[n_++];
    s->kind = kind;
    const ModRule* r = modRule(kind);
    if (r && r->oneShot) s->pending = 1;   // a fresh one-shot arrives armed
    return s;
}

void ModStateSet::arm(ModEffect kind, int magnitude, int magnitude2) {
    if (ModState* s = slot(kind)) {
        s->mag = magnitude;
        s->mag2 = magnitude2;
    }
}

void ModStateSet::apply(ModEffect kind, int magnitude, int magnitude2) {
    const ModRule* r = modRule(kind);
    if (!r) return;
    // Comparison rules only ever record a winner, so a copy that loses (or declares
    // nothing) leaves the set untouched — an absent entry reads as 0 at every hook.
    const ModState* cur = find(kind);
    switch (r->combine) {
        case ModCombine::Arm:
            slot(kind);
            return;
        case ModCombine::Sum:
            if (ModState* s = slot(kind)) s->mag += magnitude;
            break;
        case ModCombine::HighestMag:
            if (magnitude <= 0 || (cur && magnitude <= cur->mag)) return;
            if (ModState* s = slot(kind)) { s->mag = magnitude; s->mag2 = magnitude2; }
            break;
        case ModCombine::LowestMag:
            if (magnitude <= 0 || (cur && cur->mag > 0 && magnitude >= cur->mag)) return;
            if (ModState* s = slot(kind)) { s->mag = magnitude; s->mag2 = magnitude2; }
            break;
        case ModCombine::HighestMag2:
            if (magnitude2 <= 0 || (cur && magnitude2 <= cur->mag2)) return;
            if (ModState* s = slot(kind)) { s->mag = magnitude; s->mag2 = magnitude2; }
            break;
        case ModCombine::Replace:
            if (ModState* s = slot(kind)) { s->mag = magnitude; s->mag2 = magnitude2; }
            break;
    }
    if (r->capMag > 0)
        if (ModState* s = find(kind))
            if (s->mag > r->capMag) s->mag = r->capMag;
}

}  // namespace mal
