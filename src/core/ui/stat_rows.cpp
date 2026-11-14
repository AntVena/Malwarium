// stat_rows.cpp: the row models of STAT's flowed pages (stat_screen.h), kept apart from
// the draws because the ARENA's opponent sheet builds a loadout through them too.
#include "core/ui/stat_screen.h"

#include <cstdio>
#include <cstring>

#include "core/content/effect_text.h"
#include "core/content/registry.h"
#include "core/model/loadout.h"
#include "core/model/move_loadout.h"
#include "core/model/pet.h"
#include "core/model/pet_upgrades.h"
#include "core/model/stat_tiers.h"
#include "tunables.h"

namespace mal {

std::vector<ProseRow> buildLoadoutRows(const ContentRegistry& reg,
                                       const MoveLoadout& moveLoad,
                                       const ModSlots& modLoad,
                                       Stage stage, bool isEgg) {
    std::vector<ProseRow> out;
    // An egg can't train or mod (Game::slotLocked): nothing to show.
    if (isEgg) {
        out.push_back({RowKind::Item, "- NO LOADOUT -", {}, {}});
        return out;
    }

    out.push_back({RowKind::Header, "MOVES", {}, {}});
    // One row per unlocked slot: the equipped move, or (when the slot is empty) the
    // innate Quick Jab fallback marked (DEFAULT). Quick Jab is NOT a standalone row;
    // it surfaces only in a slot that has no move (mirrors the combat per-slot fallback,
    // ), so it never reads as a dedicated "default slot".
    const MoveDef* def = reg.move(moveLoad.defaultMove());
    const int unlocked = MoveLoadout::slotsForStage(stage);
    for (int i = 0; i < unlocked; ++i) {
        const char* id = moveLoad.equipped(i);
        if (const MoveDef* m = id ? reg.move(id) : nullptr) {
            out.push_back({RowKind::Item, m->displayName, {}, effectText(*m)});
        } else if (def) {
            out.push_back({RowKind::Item, def->displayName, {}, effectText(*def)});
            setProseTag(out.back(), "DEFAULT");
        }
    }

    out.push_back({RowKind::Header, "MODS", {}, {}});
    bool anyMod = false;
    for (int i = 0; i < kModSlots; ++i) {
        if (const char* id = modLoad.equipped(i)) {
            if (const ModDef* m = reg.mod(id)) {
                out.push_back({RowKind::Item, m->displayName, {}, effectText(*m)});
                anyMod = true;
            }
        }
    }
    if (!anyMod) out.push_back({RowKind::Item, "- NONE -", {}, {}});

    return out;
}

std::vector<ProseRow> buildTierRows(const int statPoints[kLevelStatCount]) {
    std::vector<ProseRow> out;
    if (!statPoints) return out;
    // Stat order, then rung order: the same order the shared table is written in, and
    // the same order every other stat-indexed readout on the device uses (0 power ·
    // 1 defense · 2 speed · 3 max-Health).
    static const char* kStatNames[kLevelStatCount] = {"POWER", "DEFENSE", "SPEED",
                                                      "MAX HEALTH"};
    for (int i = 0; i < kLevelStatCount; ++i) {
        const int points = statPoints[i];
        const int reached = statTiersReached(points);
        ProseRow head{};
        head.kind = RowKind::Header;
        head.label = kStatNames[i];
        // The stat's own point total rides its heading: it is the one number on the page
        // that is about the STAT rather than about a rung, and every "N TO GO" below is
        // measured from it, so the two have to be readable together.
        setProseTag(head, "%d PTS", points);
        out.push_back(head);
        for (int t = 0; t < kStatTierCount; ++t) {
            const StatTierDef& def = statTier(static_cast<LevelStat>(i), t);
            ProseRow row{};
            row.label = def.name;
            row.body = statTierText(static_cast<LevelStat>(i), t);
            // Through the shared helper, not `def.points - points`: "how far to the next
            // rung" is the engine's answer to give, and a page that computes its own is
            // how a screen starts disagreeing with the thing it reports on.
            if (t < reached) setProseTag(row, "HELD");
            else if (t == reached) setProseTag(row, "%d TO GO", statTierPointsToNext(points));
            else setProseTag(row, "AT %d", def.points);
            out.push_back(row);
        }
    }
    return out;
}

std::vector<ProseRow> buildBuffRows(const ContentRegistry& reg, const Pet& pet,
                                    uint32_t backupRemainMs) {
    const PetArmed& armed = pet.armed;
    const PetUpgrades& upgrades = pet.upgrades;
    std::vector<ProseRow> out;
    // The two headings are emitted LAZILY, by the first row that belongs under each:
    // an empty section is not a section, and a page that says PERMANENT over nothing
    // has told the reader something false about the pet.
    auto section = [&](const char* label) {
        for (const ProseRow& r : out)
            if (isHeading(r.kind) && std::strcmp(r.label, label) == 0) return;
        ProseRow head{};
        head.label = label;
        head.kind = RowKind::Header;
        out.push_back(head);
    };
    auto entry = [&](const ItemDef& d) {
        ProseRow row{};
        row.label = d.displayName;
        row.body = effectText(d);
        out.push_back(row);
        return &out.back();
    };
    auto add = [&](const char* itemId, bool armed) -> ProseRow* {
        if (!armed) return nullptr;
        section("ARMED");
        if (const ItemDef* d = reg.item(itemId)) return entry(*d);
        ProseRow row{};
        row.label = itemId;
        out.push_back(row);
        return &out.back();
    };
    add("restore_point", pet.grants.shieldActive);
    add("ambig_usb", armed.trojanDivert);
    // The one timed buff: its countdown is the row's suffix, read fresh each repaint
    // because the page rebuilds its rows every draw.
    if (ProseRow* row = add("backup_drive", backupRemainMs > 0)) {
        const uint32_t s = backupRemainMs / 1000u;
        std::snprintf(row->suffix, sizeof(row->suffix), "%u:%02u LEFT", s / 60u,
                      static_cast<unsigned>(s % 60u));
    }
    // The two DeepWeb Dive depth buffs: which ITEM armed them isn't tracked
    // directly (the pet only keeps the raw multiplier/depth), so find the item whose
    // effect matches the current state instead of hardcoding an id here.
    // `heading` is which section the found row lands under: every arming lands under
    // ARMED, and an Epic dish's standing grant under PERMANENT.
    auto addByEffect = [&](ItemEffect::Kind kind, int magnitude, bool matchAnyMagnitude,
                           const char* heading = "ARMED") {
        for (const ItemDef* d : reg.allItems())
            for (const ItemEffect& e : d->effects)
                if (e.kind == kind && (matchAnyMagnitude || e.magnitude == magnitude)) {
                    section(heading);
                    entry(*d);
                    return;
                }
    };
    if (armed.deepWebDepthMultiplier > 1)
        addByEffect(ItemEffect::Kind::ArmDeepWebDepthMultiplier, armed.deepWebDepthMultiplier,
                    false);
    if (armed.deepWebStartDepth == PetArmed::kStartDepthUseBest)
        addByEffect(ItemEffect::Kind::SetDeepWebStartDepthToBest, 0, true);
    else if (armed.deepWebStartDepth != -1)
        addByEffect(ItemEffect::Kind::SetDeepWebStartDepth, armed.deepWebStartDepth, false);
    // The rest of the USB port, found the same way and for the same reason: the pet keeps
    // the forced DIRECTION and the raw soak FACTOR, not which device wrote them, so the
    // row names whichever item carries that effect. The soak matches on its magnitude
    // (which is what tells a Sandbox-USB from a Hypervisor-USB); a forced branch has no
    // magnitude at all, so it matches on the Kind alone.
    if (pet.usb.branch == BranchOverride::Good)
        addByEffect(ItemEffect::Kind::ForceEvolveBranchGood, 0, true);
    else if (pet.usb.branch == BranchOverride::Bad)
        addByEffect(ItemEffect::Kind::ForceEvolveBranchBad, 0, true);
    if (pet.usb.soakFactor > 1) {
        // Either soak Kind can be the one armed and the pet keeps only the factor, so ask
        // for the plain one first and fall back to the late one: the magnitude is what
        // tells a Sandbox-USB from a Hypervisor-USB, and only one row carries each.
        const std::size_t before = out.size();
        addByEffect(ItemEffect::Kind::ArmEvolveSoak, pet.usb.soakFactor, false);
        if (out.size() == before)
            addByEffect(ItemEffect::Kind::ArmEvolveSoakLate, pet.usb.soakFactor, false);
    }
    if (pet.usb.hold) addByEffect(ItemEffect::Kind::ArmEvolveHold, 0, true);
    // Last, because these are the rows that never lapse: everything an Epic dish has
    // permanently given this pet. Found by effect like the dive buffs above rather than
    // by id, so a dish that grants one can be renamed without touching this file, and
    // in stat order, so a pet carrying several reads as one block rather than in
    // whichever order it happened to be fed.
    if (upgrades.bandwidthRegenMin > 0)
        addByEffect(ItemEffect::Kind::BandwidthRegenBonusMin, 0, true, "PERMANENT");
    static const ItemEffect::Kind kStatGrants[kLevelStatCount] = {
        ItemEffect::Kind::StatPointPower, ItemEffect::Kind::StatPointDefense,
        ItemEffect::Kind::StatPointSpeed, ItemEffect::Kind::StatPointHealth};
    for (int i = 0; i < kLevelStatCount; ++i)
        if (upgrades.statBonus[i] > 0) addByEffect(kStatGrants[i], 0, true, "PERMANENT");
    if (upgrades.xpRatePct > 0)
        addByEffect(ItemEffect::Kind::XpRateBonusPct, 0, true, "PERMANENT");
    if (upgrades.favouriteFound) {
        section("PERMANENT");
        ProseRow row{};
        row.label = "FAVOURITE DISH";
        std::snprintf(row.body.buf, sizeof(row.body.buf),
                      "Loves %s. +%d to every combat stat.",
                      upgrades.favouriteFood ? upgrades.favouriteFood->displayName
                                             : "its favourite",
                      kFavouriteFoodStatBonus);
        out.push_back(row);
    }
    return out;
}

}  // namespace mal
