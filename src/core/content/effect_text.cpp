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
        case ItemEffect::Kind::ArmCombatShieldBuff: return "shieldMins";
        case ItemEffect::Kind::ArmDeepWebDepthMultiplier: return "depthStep";
        case ItemEffect::Kind::SetDeepWebStartDepth: return "depth";
        case ItemEffect::Kind::SetDeepWebStartDepthToBest: return "bestDepth";
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
        {"stackPower", d.stackPowerPct},
        {"stackPowerCap", d.stackPowerCap},
        {"stackDef", d.stackDefensePct},
        {"stackDefCap", d.stackDefenseCap},
        {"pierce", d.armorPiercePct},
        {"lock", d.lockTurns},
        {"dot", d.dotDamage},
        {"dotTurns", d.dotTurns},
        {"stealPower", d.stealPowerPct},
        {"stealDef", d.stealDefensePct},
        {"stealSpeed", d.stealSpeedPct},
        {"stealHp", d.stealCurrentHpPct},
        {"stealMaxHp", d.stealMaxHpPct},
        {"evade", d.trapEvasionPct},
        {"rebound", d.trapReboundPct},
        {"armorRot", d.trapArmorRot},
        {"trapBonus", d.trapPassiveBonusPct},
    };
    EffectText out;
    expand(out, d.effect, toks, sizeof(toks) / sizeof(toks[0]));
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
            case ItemEffect::Kind::ArmCombatShieldBuff:
                s.add("SHIELD", "%dMIN", e.magnitude);
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
    if (d.stealPowerPct) s.add("SIPHON PWR", "%d%%", d.stealPowerPct);
    if (d.stealDefensePct) s.add("SIPHON DEF", "%d%%", d.stealDefensePct);
    if (d.stealSpeedPct) s.add("BITE SPD", "%d%%", d.stealSpeedPct);
    if (d.stealCurrentHpPct) s.add("BITE DRAIN", "%d%%", d.stealCurrentHpPct);
    if (d.stealMaxHpPct) s.add("SIPHON MAXHP", "%d%%", d.stealMaxHpPct);
    if (d.trapArm) {
        s.add("EVADE", "%d%%", d.trapEvasionPct);
        s.add("REBOUND", "%d%%", d.trapReboundPct);
        s.add("ARMOR ROT", "%d%%", d.trapArmorRot);
        // What holding this trap adds to the Execution-Override hijack chance
        // (Combat::execOverrideChance) — otherwise the row's one magnitude that
        // reaches the player nowhere at all.
        if (d.trapPassiveBonusPct) s.add("OVERRIDE", "%+d%%", d.trapPassiveBonusPct);
    }
    return s.out;
}

EffectText statLine(const ItemDef& d) { return joinSpec(specRows(d)); }
EffectText statLine(const ModDef& d) { return joinSpec(specRows(d)); }
EffectText statLine(const MoveDef& d) { return joinSpec(specRows(d)); }

}  // namespace mal
