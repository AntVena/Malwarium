// effect_text.cpp — {token} expansion + the derived stat line (see effect_text.h).
#include "core/content/effect_text.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mal {
namespace {

// One substitutable value, named as it appears between the braces. `text` non-null makes
// it a WORD token (substituted verbatim, `value` ignored) — for the rare template whose
// blank is an id rather than a magnitude, e.g. an achievement's "{key} line".
struct Token {
    const char* name;
    int value;
    const char* text = nullptr;
};

// Append as much of `s` as fits, always leaving `out` terminated. Returns the new
// write position so a caller can chain appends without re-measuring.
std::size_t append(char* out, std::size_t cap, std::size_t at, const char* s) {
    while (*s && at + 1 < cap) out[at++] = *s++;
    out[at] = '\0';
    return at;
}

std::size_t appendInt(char* out, std::size_t cap, std::size_t at, int v) {
    char n[16];
    std::snprintf(n, sizeof(n), "%d", v);
    return append(out, cap, at, n);
}

// Substitute every `{token}` / `{|token|}` in `tmpl` from `toks`. An unknown token is
// copied through braces and all — visible in-game and asserted against by the native
// gate, so a renamed field fails loudly instead of quietly dropping its number.
void expand(EffectText& out, const char* tmpl, const Token* toks, int nToks) {
    const std::size_t cap = sizeof(out.buf);
    std::size_t at = 0;
    if (!tmpl) return;
    for (const char* p = tmpl; *p && at + 1 < cap;) {
        if (*p != '{') {
            out.buf[at++] = *p++;
            out.buf[at] = '\0';
            continue;
        }
        const char* close = std::strchr(p, '}');
        if (!close) {  // unterminated — pass the rest through verbatim
            at = append(out.buf, cap, at, p);
            break;
        }
        const char* name = p + 1;
        std::size_t len = static_cast<std::size_t>(close - name);
        const bool abs = len >= 2 && name[0] == '|' && name[len - 1] == '|';
        if (abs) {
            ++name;
            len -= 2;
        }
        const Token* hit = nullptr;
        for (int i = 0; i < nToks; ++i)
            if (std::strlen(toks[i].name) == len &&
                std::strncmp(toks[i].name, name, len) == 0) {
                hit = &toks[i];
                break;
            }
        if (hit && hit->text) {
            at = append(out.buf, cap, at, hit->text);
        } else if (hit) {
            at = appendInt(out.buf, cap, at, abs ? std::abs(hit->value) : hit->value);
        } else {  // leave the braces in — the gate test fails on them
            for (const char* q = p; q <= close && at + 1 < cap; ++q) {
                out.buf[at++] = *q;
                out.buf[at] = '\0';
            }
        }
        p = close + 1;
    }
}

}  // namespace

void SpecBuilder::add(const char* label, const char* fmt, ...) {
    const int cap = static_cast<int>(sizeof(out.rows) / sizeof(out.rows[0]));
    if (out.count >= cap) return;
    SpecRow& r = out.rows[out.count++];
    std::snprintf(r.label, sizeof(r.label), "%s", label);
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(r.value, sizeof(r.value), fmt, ap);
    va_end(ap);
}

void SpecBuilder::flag(const char* label) {
    const int cap = static_cast<int>(sizeof(out.rows) / sizeof(out.rows[0]));
    if (out.count >= cap) return;
    SpecRow& r = out.rows[out.count++];
    std::snprintf(r.label, sizeof(r.label), "%s", label);
    r.value[0] = '\0';
}

namespace {

// The one-line form of any readout: " / "-separated, the separator only between
// entries so an empty set stays genuinely empty.
EffectText joinSpec(const SpecRows& s) {
    EffectText t;
    std::size_t at = 0;
    for (int i = 0; i < s.count; ++i) {
        if (at) at = append(t.buf, sizeof(t.buf), at, " / ");
        at = append(t.buf, sizeof(t.buf), at, s.rows[i].label);
        if (!s.rows[i].flag()) {
            at = append(t.buf, sizeof(t.buf), at, " ");
            at = append(t.buf, sizeof(t.buf), at, s.rows[i].value);
        }
    }
    return t;
}

}  // namespace

const char* itemEffectToken(ItemEffect::Kind k) {
    switch (k) {
        case ItemEffect::Kind::None: return nullptr;
        case ItemEffect::Kind::Hunger: return "hunger";
        case ItemEffect::Kind::HungerStacking: return "hungerStack";
        case ItemEffect::Kind::Happy: return "happy";
        case ItemEffect::Kind::HappyToward50: return "happy50";
        case ItemEffect::Kind::Frag: return "frag";
        case ItemEffect::Kind::RemoveCareMistakeOnce: return "mistakes";
        case ItemEffect::Kind::ClearMistakeShieldOnce: return "shields";
        case ItemEffect::Kind::ForceTrojanDivert: return "divert";
        // The soak's factor is ONE number that is both halves of the trade — the clock
        // it stretches and the XP it pays — which is why one token serves a sentence
        // that names it twice. The branch-override pair is below with the other flags.
        case ItemEffect::Kind::ArmEvolveSoak:
        case ItemEffect::Kind::ArmEvolveSoakLate: return "soak";
        case ItemEffect::Kind::ArmCombatShieldBuff: return "shieldMins";
        case ItemEffect::Kind::ArmDeepWebDepthMultiplier: return "depthStep";
        case ItemEffect::Kind::SetDeepWebStartDepth: return "depth";
        case ItemEffect::Kind::SetDeepWebStartDepthToBest: return "bestDepth";
        case ItemEffect::Kind::BandwidthRegenBonusMin: return "regenMins";
        case ItemEffect::Kind::Bandwidth: return "bandwidth";
        // The permanent per-pet grants. Each token names the number of points (or the
        // percent) the row hands over, so an Epic dish's prose can promise the size of
        // the upgrade without typing the digit.
        case ItemEffect::Kind::StatPointPower: return "power";
        case ItemEffect::Kind::StatPointDefense: return "defense";
        case ItemEffect::Kind::StatPointSpeed: return "speed";
        case ItemEffect::Kind::StatPointHealth: return "maxhp";
        case ItemEffect::Kind::XpRateBonusPct: return "xpRate";
        // No token, and deliberately: a token exists so a description can name the
        // field holding its number, and a ghost is a flag rather than a quantity.
        // The row says what it does in words ("Cuts a Replication Ghost loose") and
        // the spec grid carries it as a flag; neither has a magnitude to interpolate.
        case ItemEffect::Kind::ClearReplicationGhost: return nullptr;
        // Same rule, same reason: a forced branch is a DIRECTION, and the direction is
        // in the Kind rather than in a magnitude. Bad-USB and Signed-USB say which way
        // they point in words, and the grid carries each as a flag.
        case ItemEffect::Kind::ForceEvolveBranchGood:
        case ItemEffect::Kind::ForceEvolveBranchBad: return nullptr;
        // And the port's two states, for the third time: a hold is a state and an eject
        // is an action. Neither has a size, so neither has a token.
        case ItemEffect::Kind::ArmEvolveHold:
        case ItemEffect::Kind::ClearUsbPort: return nullptr;
    }
    return nullptr;
}

EffectText effectText(const ItemDef& d) {
    // An item's on-Use magnitudes are a kind-keyed list, so its tokens are the kind
    // names (itemEffectToken) rather than fixed field names; the trailing hand-offs to
    // other systems get one token each.
    Token toks[kMaxItemEffects + 4];
    int n = 0;
    for (const ItemEffect& e : d.effects)
        if (const char* name = itemEffectToken(e.kind)) toks[n++] = {name, e.magnitude};
    toks[n++] = {"heal", d.combatHeal};
    toks[n++] = {"xp", d.preEncounterXp};
    toks[n++] = {"bits", d.bitsPrice};
    toks[n++] = {"spoil", d.spoil.pct};

    EffectText out;
    expand(out, d.effect, toks, n);
    return out;
}

EffectText effectText(const ModDef& d) {
    const Token toks[] = {
        {"mag", d.magnitude},
        {"mag2", d.magnitude2},
        {"bonus", d.affinityBonus},
        {"magBonus", d.magnitude + d.affinityBonus},  // the with-affinity total
        {"tier", d.powerTier},
    };
    EffectText out;
    expand(out, d.effect, toks, sizeof(toks) / sizeof(toks[0]));
    return out;
}

EffectText effectText(const MoveDef& d) {
    const Token toks[] = {
        {"power", d.power},
        {"turns", d.channelTurns},
        {"refund", d.speedRefundPct},
        {"stackPower", d.stackPowerPct},
        {"stackPowerCap", d.stackPowerCap},
        {"stackDef", d.stackDefensePct},
        {"stackDefCap", d.stackDefenseCap},
        {"pierce", d.armorPiercePct},
        {"lock", d.lockTurns},
        {"dot", d.dotDamage ? d.dotDamage : d.poolRetaliateDot},
        {"dotTurns", d.dotTurns ? d.dotTurns : d.poolRetaliateTurns},
        {"stealPower", d.stealPowerPct},
        {"stealDef", d.stealDefensePct},
        {"stealSpeed", d.stealSpeedPct},
        {"stealHp", d.stealCurrentHpPct},
        {"stealMaxHp", d.stealMaxHpPct},
        {"evade", d.trapEvasionPct},
        {"rebound", d.trapReboundPct},
        {"armorRot", d.trapArmorRot},
        {"trapBonus", d.trapPassiveBonusPct},
        {"replicaChance", d.replicaSpawnPct},
        {"replicaPower", d.replicaPowerPct},
        {"replicaHealth", d.replicaHealthPct},
    };
    EffectText out;
    expand(out, d.effect, toks, sizeof(toks) / sizeof(toks[0]));
    return out;
}

EffectText effectText(const CrewExploitDef& d) {
    const Token toks[] = {{"mag", d.magnitude}};
    EffectText out;
    expand(out, d.desc, toks, sizeof(toks) / sizeof(toks[0]));
    return out;
}

EffectText effectText(const AchievementDef& d, int effectiveGoal) {
    // `{n}` is the goal the row is actually held to — which for a kGoalAll row is the
    // size of the set it counts over, resolved by the caller (app/game_achievements.h).
    // That is the whole point of templating this prose: "all {n} sub-areas" restates
    // itself when an area is added, instead of going quietly stale.
    const Token toks[] = {
        {"n", effectiveGoal},
        {"p", d.param},
        {"key", 0, d.key ? d.key : ""},
    };
    EffectText out;
    expand(out, d.trigger, toks, sizeof(toks) / sizeof(toks[0]));
    return out;
}

SpecRows specRows(const ItemDef& d) {
    SpecBuilder s;
    for (const ItemEffect& e : d.effects) {
        switch (e.kind) {
            case ItemEffect::Kind::None: break;
            case ItemEffect::Kind::Hunger: s.add("HUNGER", "%+d", e.magnitude); break;
            // Per-item-in-the-run, so the grid says so rather than printing a flat
            // number the pet will only score on its first bite.
            case ItemEffect::Kind::HungerStacking:
                s.add("HUNGER", "%+d EA", e.magnitude);
                break;
            case ItemEffect::Kind::Happy: s.add("HAPPY", "%+d", e.magnitude); break;
            case ItemEffect::Kind::HappyToward50:
                s.add("HAPPY>50", "%d", e.magnitude);
                break;
            case ItemEffect::Kind::Frag: s.add("FRAG", "%+d", e.magnitude); break;
            // "once per lifecycle" is a caveat, not a magnitude — every row carrying
            // one already says so in its own prose, so the grid keeps the number.
            case ItemEffect::Kind::RemoveCareMistakeOnce:
                s.add("MISTAKE", "-%d", e.magnitude);
                break;
            case ItemEffect::Kind::ClearMistakeShieldOnce:
                s.flag("MISTAKE SHIELD");
                break;
            case ItemEffect::Kind::ForceTrojanDivert: s.flag("TROJAN DIVERT"); break;
            // A direction, not a magnitude — the grid says which way the branch is
            // forced and leaves the "whatever the care record says" half to the row.
            case ItemEffect::Kind::ForceEvolveBranchGood: s.flag("FORCE GOOD"); break;
            case ItemEffect::Kind::ForceEvolveBranchBad: s.flag("FORCE BAD"); break;
            case ItemEffect::Kind::ArmEvolveSoak:
                s.add("SOAK", "x%d", e.magnitude);
                break;
            // The late soak reports the same factor, because that IS what it pays and
            // what it costs at Process. The doubled Script clock is a property of WHERE
            // it is used rather than of the row, so the row's prose carries it and the
            // grid keeps the number that is true wherever it goes in.
            case ItemEffect::Kind::ArmEvolveSoakLate:
                s.add("SOAK", "x%d", e.magnitude);
                break;
            case ItemEffect::Kind::ArmEvolveHold: s.flag("EVOLVE HELD"); break;
            case ItemEffect::Kind::ClearUsbPort: s.flag("CLEARS USB"); break;
            case ItemEffect::Kind::ArmCombatShieldBuff:
                s.add("DEATH SAVE", "%dMIN", e.magnitude);
                break;
            case ItemEffect::Kind::ArmDeepWebDepthMultiplier:
                s.add("DIVE STEP", "x%d", e.magnitude);
                break;
            case ItemEffect::Kind::SetDeepWebStartDepth:
                s.add("DIVE FROM", "%d", e.magnitude);
                break;
            case ItemEffect::Kind::SetDeepWebStartDepthToBest:
                s.flag("DIVE FROM BEST");
                break;
            case ItemEffect::Kind::BandwidthRegenBonusMin:
                s.add("BW REGEN", "-%dMIN", e.magnitude);
                break;
            case ItemEffect::Kind::Bandwidth:
                s.add("BANDWIDTH", "%+d", e.magnitude);
                break;
            // The permanent grants report the bare stat and its size, and leave the
            // FOR-LIFE half to the row's prose — where the other once-per-lifetime items
            // already say it. A label carrying it too would push these past the half
            // column (fitsHalf, ui/widgets.cpp) and cost every Epic dish a grid line it
            // needs for the sentence that explains the grant.
            case ItemEffect::Kind::StatPointPower:
            case ItemEffect::Kind::StatPointDefense:
            case ItemEffect::Kind::StatPointSpeed:
            case ItemEffect::Kind::StatPointHealth:
                s.add(levelStatWord(statPointEffectIndex(e.kind)), "%+d", e.magnitude);
                break;
            case ItemEffect::Kind::XpRateBonusPct:
                s.add("XP RATE", "%+d%%", e.magnitude);
                break;
            // A flag, not a number — the cure is unconditional when a ghost is
            // there and a no-op when it isn't, so there is nothing to print but
            // the fact that the row does it.
            case ItemEffect::Kind::ClearReplicationGhost:
                s.flag("GHOST CURE");
                break;
        }
    }
    if (d.combatHeal) s.add("HEAL", "%d", d.combatHeal);
    if (d.preEncounterXp) s.add("XP", "%d", d.preEncounterXp);
    return s.out;
}

SpecRows specRows(const ModDef& d) {
    SpecBuilder s;
    switch (d.effectKind) {
        case ModEffect::None: break;
        case ModEffect::PowerPct: s.add("POWER", "%+d%%", d.magnitude); break;
        case ModEffect::DamageCutPct: s.add("DMG CUT", "%d%%", d.magnitude); break;
        case ModEffect::MaxHealth: s.add("MAX HP", "%+d", d.magnitude); break;
        case ModEffect::Speed: s.add("SPEED", "%+d", d.magnitude); break;
        case ModEffect::PostBattleBits: s.add("WIN BITS", "%+d", d.magnitude); break;
        case ModEffect::RaidMirror: s.flag("SURVIVES 1 FATAL HIT"); break;
        case ModEffect::FatigueFragCut:
            s.add("FATIGUE FRAG", "-%d%%", d.magnitude);
            break;
        case ModEffect::Thorns: s.add("THORNS", "%d", d.magnitude); break;
        case ModEffect::DeathBlast: s.add("ON KO", "%d", d.magnitude); break;
        case ModEffect::MaxHitCapPct: s.add("HIT CAP", "%d%%HP", d.magnitude); break;
        case ModEffect::LoadBalance:
            s.add("SPLIT OVER", "%d%%HP", d.magnitude);
            s.add("DEFER", "%d%%", d.magnitude2);
            break;
        case ModEffect::WatchdogClamp:
            s.add("FREEZE CAP", "%dTURN", d.magnitude);
            break;
        case ModEffect::FaradayCut: s.add("DOT", "-%d%%", d.magnitude); break;
        case ModEffect::RegenPerTurn: s.add("REGEN", "%+d/TURN", d.magnitude); break;
        case ModEffect::ArmorPiercePct: s.add("PIERCE", "%d%%", d.magnitude); break;
        case ModEffect::FirstStrikeRankMult: s.flag("1ST HIT x ATK RANK"); break;
        case ModEffect::AttackCountPowerPct:
            s.add("POWER/ATK", "%+d%%", d.magnitude);
            break;
        case ModEffect::DefendCountCutPct:
            s.add("DMG CUT/DEF", "%+d%%", d.magnitude);
            break;
        case ModEffect::FirstHitCutPct:
            s.add("1ST HIT CUT", "%d%%", d.magnitude);
            break;
        case ModEffect::LowHealthPowerPct:
            s.add("BELOW", "%d%%HP", d.magnitude);
            s.add("POWER", "%+d%%", d.magnitude2);
            break;
        case ModEffect::GambleBattlePowerPct:
            s.add("CHANCE", "%d%%", d.magnitude);
            s.add("POWER", "%+d%%", d.magnitude2);
            break;
        case ModEffect::ConditionalThorns:
            s.add("BELOW", "%d%%HP", d.magnitude2);
            s.add("THORNS", "%d", d.magnitude);
            break;
        case ModEffect::StealAmplifyPct:
            s.add("SIPHON+", "%+d%%", d.magnitude);
            break;
        // Both read as percentage POINTS on a passive's roll, so the label names the roll
        // rather than the mod — a player comparing two rows sees what moved.
        case ModEffect::ExecOverridePct:
            s.add("HIJACK", "%+d%%", d.magnitude);
            break;
        case ModEffect::ReplicaSpawnPct:
            s.add("REPLICATE", "%+d%%", d.magnitude);
            break;
        // Two-magnitude rows name BOTH halves, the way LowHealthPowerPct does: what the
        // mod does standing, and what the line's own mechanic turns that into.
        case ModEffect::ExtortionLedger:
            s.add("DMG CUT", "%d%%", d.magnitude);
            s.add("OWED POWER", "%+d%%", d.magnitude2);
            break;
        case ModEffect::ReplicaWorthPct:
            s.add("COPY WORTH", "%+d%%", d.magnitude);
            break;
        case ModEffect::PolymorphEffectPct:
            // Not a percentage — it is stat POINTS paid per effect kind, so the value is
            // written as the count it is rather than borrowing a %% that would read as one.
            s.add("PER EFFECT", "%+d", d.magnitude);
            break;
    }
    // The Speed mod's secondary knob is a COST, not a second effect — every other
    // magnitude2 user spells its own pair out above.
    if (d.effectKind == ModEffect::Speed && d.magnitude2)
        s.add("POWER", "-%d%%", d.magnitude2);
    if (d.effectKind == ModEffect::MaxHealth && d.magnitude2)
        s.add("SPEED", "-%d", d.magnitude2);
    if (d.affinityBonus && d.line) s.add("ON LINE", "%+d", d.affinityBonus);
    return s.out;
}

SpecRows specRows(const MoveDef& d) {
    SpecBuilder s;
    // An Obfuscation move's power is the size of the POOL it lays down, not a
    // one-shot brace, so it leads with what that number actually buys. A trap move's
    // power is 0 by design (it arms rather than braces), so it leads with the trap.
    if (d.shieldPool) s.add("SHIELD POOL", "%d", d.power);
    else if (!d.trapArm) s.add(moveKindTag(d.kind), "%d", d.power);
    if (d.channelTurns > 1) s.add("CHANNEL", "%d", d.channelTurns);
    // What the cast hands back toward its next action. Sits with the power it is the
    // counterweight to, because the two together are the whole of what a brace costs.
    if (d.speedRefundPct) s.add("TEMPO", "%d%%", d.speedRefundPct);
    // What a strike on the live pool plants on the striker (poolRow). Its own label rather
    // than DOT's, because this one is spent by the ENEMY's action, not by the caster's.
    if (d.poolRetaliateDot) s.add("SALT", "%dx%d", d.poolRetaliateDot, d.poolRetaliateTurns);
    if (d.armorPiercePct) s.add("PIERCE", "%d%%", d.armorPiercePct);
    if (d.stackPowerPct) {
        s.add("POWER", "%+d%%", d.stackPowerPct);
        s.add("UP TO", "%+d%%", d.stackPowerCap);
    }
    if (d.stackDefensePct) {
        s.add("DEF", "%+d%%", d.stackDefensePct);
        s.add("UP TO", "%+d%%", d.stackDefenseCap);
    }
    if (d.lockTurns) s.add("FREEZE", "%d", d.lockTurns);
    if (d.dotDamage) s.add("DOT", "%dx%d", d.dotDamage, d.dotTurns);
    // Leads the steal block: it is the one steal that outlives its own hit, since the
    // pool MOVES rather than the hit landing harder. Short label on purpose — the grid
    // packs two rows to a line when both fit half-width (gridLines), so a compact row
    // here fills the power row's line instead of opening another at the end.
    if (d.stealMaxHpPct) s.add("MAX HP", "%d%%", d.stealMaxHpPct);
    if (d.stealPowerPct) s.add("SIPHON PWR", "%d%%", d.stealPowerPct);
    if (d.stealDefensePct) s.add("SIPHON DEF", "%d%%", d.stealDefensePct);
    if (d.stealSpeedPct) s.add("BITE SPD", "%d%%", d.stealSpeedPct);
    if (d.stealCurrentHpPct) s.add("BITE DRAIN", "%d%%", d.stealCurrentHpPct);
    if (d.trapArm) {
        s.add("EVADE", "%d%%", d.trapEvasionPct);
        s.add("REBOUND", "%d%%", d.trapReboundPct);
        s.add("ARMOR ROT", "%d%%", d.trapArmorRot);
        // What holding this trap adds to the Execution-Override hijack chance
        // (Combat::execOverrideChance) — otherwise the row's one magnitude that
        // reaches the player nowhere at all.
        if (d.trapPassiveBonusPct) s.add("OVERRIDE", "%+d%%", d.trapPassiveBonusPct);
    }
    // Worm replication: what the cast puts on the board. The move's own `kind` already
    // decided which sort spawns, so the readout names the sort rather than repeating the
    // chance under two labels — and only the magnitude that kind actually reads follows
    // it. Both are shares of the parent, which is why they carry a %.
    if (d.replicaSpawnPct) {
        const bool defender = d.kind == MoveDef::Kind::Defend;
        s.add(defender ? "SPAWN DEF" : "SPAWN ATK", "%d%%", d.replicaSpawnPct);
        if (defender) s.add("COPY HP", "%d%%", d.replicaHealthPct);
        else s.add("COPY PWR", "%d%%", d.replicaPowerPct);
    }
    return s.out;
}

EffectText statLine(const ItemDef& d) { return joinSpec(specRows(d)); }
EffectText statLine(const ModDef& d) { return joinSpec(specRows(d)); }
EffectText statLine(const MoveDef& d) { return joinSpec(specRows(d)); }

}  // namespace mal
