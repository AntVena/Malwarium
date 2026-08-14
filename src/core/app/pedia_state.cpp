// pedia_state.cpp — see pedia_state.h. First-cut builder: favors coverage over
// polish (every fixture block is present, several with a documented stub).
#include "core/app/pedia_state.h"

#include <cstdio>
#include <cstring>

#include "core/app/game.h"
#include "core/app/game_achievements.h"
#include "core/content/content_achievements.h"
#include "core/content/content_recipes.h"
#include "core/content/defs.h"
#include "core/content/registry.h"
#include "core/model/combat.h"
#include "core/model/loadout.h"
#include "core/model/move_loadout.h"
#include "core/model/pet_model.h"
#include "core/model/save.h"

namespace mal {

namespace {

// Append `s` as a JSON string literal, escaping the handful of characters that
// matter (quote/backslash/control chars). The only genuinely free-text field
// today is the HackerTag, but this is applied uniformly so nothing downstream
// has to remember to escape.
void appendJsonString(std::string& out, const char* s) {
    out += '"';
    if (s) {
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p) {
            const unsigned char c = *p;
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (c < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out += buf;
                    } else {
                        out += static_cast<char>(c);
                    }
            }
        }
    }
    out += '"';
}

void appendKeyString(std::string& out, const char* key, const char* value, bool comma = true) {
    appendJsonString(out, key);
    out += ':';
    appendJsonString(out, value);
    if (comma) out += ',';
}

void appendKeyInt(std::string& out, const char* key, long value, bool comma = true) {
    appendJsonString(out, key);
    out += ':';
    out += std::to_string(value);
    if (comma) out += ',';
}

// Uppercase, no-space stage tag ("PROCESS"/"SCRIPT"/"DAEMON"/"BOOT SECTOR") —
// matches the archive-entry `stage` values in web/fixtures/pedia_state.js.
const char* stageTag(Stage s) {
    switch (s) {
        case Stage::BootSector: return "BOOT SECTOR";
        case Stage::Process: return "PROCESS";
        case Stage::Script: return "SCRIPT";
        case Stage::Daemon: return "DAEMON";
    }
    return "?";
}

std::string formatHms(uint32_t ms) {
    const uint32_t totalSec = ms / 1000;
    const uint32_t h = totalSec / 3600;
    const uint32_t m = (totalSec % 3600) / 60;
    const uint32_t s = totalSec % 60;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02u:%02u:%02u", h, m, s);
    return std::string(buf);
}

} // namespace

std::string buildPediaStateJson(const Game& g) {
    std::string out;
    out.reserve(2048);
    out += '{';

    // Landing fields ---------------------------------
    appendKeyString(out, "hacker_tag", g.hackerTag());
    appendKeyInt(out, "currency_bits", g.bits());
    appendKeyInt(out, "hacker_rank", g.hackerRank());
    appendKeyInt(out, "networks_seen", g.networksSeen());
    // Extension beyond the sample fixture (asked for explicitly) — lifetime
    // captured handshakes (Audit mode).
    appendKeyInt(out, "handshakes", g.handshakesSeen());
    // Distinct species this device has raised — the same tally PROFILE's SPECIES row
    // shows, so the site and the on-device viewer never disagree about the number.
    appendKeyInt(out, "species_raised", g.speciesRaised());
    // Where this device checks for updates, so the site's user section can show
    // the current value in the field that edits it. Not a secret — it is a
    // download address, and the operator is the one who set it.
    appendKeyString(out, "update_url", g.updateManifestUrl());

    // --- active_pet (the landing page's live pet card) ----------------------
    appendJsonString(out, "active_pet");
    out += ':';
    if (const CreatureDef* pet = g.pet()) {
        out += '{';
        // "species" is the field web/app.js actually matches against
        // D.creatures[].id — keep it the content id, not the display name.
        appendKeyString(out, "species", pet->id);
        appendKeyString(out, "id", pet->id);
        appendKeyString(out, "stage", stageTag(pet->stage));
        appendKeyString(out, "line", pet->line ? pet->line : "");
        appendKeyString(out, "uptime", formatHms(g.lifetimeUptimeMs()).c_str());
        appendKeyInt(out, "frag_pct", g.model().fragmentation());
        appendKeyInt(out, "mistakes", g.model().careMistakes(), /*comma=*/false);
        out += '}';
    } else {
        out += "null";  // no pet yet (empty save, pre-Hatch)
    }
    out += ',';

    // --- pets{} (per-creature reveal state) ---------------------------------
    // "hatched" (Game::creatureRaised — the device's permanent tally of species it
    // has raised, so a creature stays revealed after its successor stamps over it)
    // wins over "seen" (Game::markCreatureSeen — faced in a fight but never raised;
    // today only a duel opponent), which wins over "locked".
    appendJsonString(out, "pets");
    out += ':';
    out += '{';
    {
        const std::vector<const CreatureDef*> creatures = g.content().allCreatures();
        for (size_t i = 0; i < creatures.size(); ++i) {
            const CreatureDef* c = creatures[i];
            const char* state = g.creatureRaised(c->id) ? "hatched"
                                : g.creatureSeen(c->id)  ? "seen"
                                                          : "locked";
            appendKeyString(out, c->id, state, i + 1 < creatures.size());
        }
    }
    out += "},";

    // --- malbeasts{} ---------------------------------------------------------
    // The fixed 6-entry wild-malbeast roster (combat.h kWildMalbeastIds, save v25):
    // "defeated" (a won live wild fight) wins over "seen" (the wild roll itself),
    // which wins over "locked". Sub/area bosses and Sim dummies never set either bit.
    appendJsonString(out, "malbeasts");
    out += ':';
    out += '{';
    {
        for (int i = 0; i < kWildMalbeastCount; ++i) {
            const char* state = g.malbeastDefeated(i) ? "defeated"
                                : g.malbeastSeen(i)    ? "seen"
                                                        : "locked";
            appendKeyString(out, kWildMalbeastIds[i], state, i + 1 < kWildMalbeastCount);
        }
    }
    out += "},";

    // --- items{} (unlocked = ever held, not held right now) -----------------
    // Game::itemCollected, the same lifetime tally the cuisine and rarity achievement
    // ladders count. Current possession would un-reveal a row the moment its last copy
    // was spent, which is wrong everywhere and worst in the kitchen: food is FOR eating,
    // so a pantry keyed on the bag would empty itself as it was played.
    appendJsonString(out, "items");
    out += ':';
    out += '{';
    {
        const std::vector<const ItemDef*> items = g.content().allItems();
        for (size_t i = 0; i < items.size(); ++i) {
            const ItemDef* it = items[i];
            const bool met = g.itemCollected(it->id);
            appendKeyString(out, it->id, met ? "unlocked" : "locked", i + 1 < items.size());
        }
    }
    out += "},";

    // --- recipes{} (the MERGE HUB methods, keyed by the dish they make) ------
    // The kitchen's second axis. Meeting a dish (items{} above) and knowing how to cook
    // it are independent facts — a dish can be bought off a shelf without its method
    // ever dropping — so the site needs both to say which of the two a row is waiting on.
    // Enumerated from the content table and answered by Game, so adding a recipe row
    // reaches the site with no edit here.
    appendJsonString(out, "recipes");
    out += ':';
    out += '{';
    for (int i = 0; i < kMergeRecipeCount; ++i) {
        const MergeRecipe& r = kMergeRecipes[i];
        appendKeyString(out, r.outputId, g.recipeKnown(r.outputId) ? "known" : "locked",
                        i + 1 < kMergeRecipeCount);
    }
    out += "},";

    // --- mods{} (owned = a held spare OR currently installed) ---------------
    appendJsonString(out, "mods");
    out += ':';
    out += '{';
    {
        const std::vector<const ModDef*> mods = g.content().allMods();
        for (size_t i = 0; i < mods.size(); ++i) {
            const ModDef* m = mods[i];
            const bool owned = g.loadout().owns(m->id) || g.loadout().slotOf(m->id) >= 0;
            appendKeyString(out, m->id, owned ? "unlocked" : "locked", i + 1 < mods.size());
        }
    }
    out += "},";

    // --- moves{} (owned = in the owned pool OR the innate default move) -----
    appendJsonString(out, "moves");
    out += ':';
    out += '{';
    {
        const std::vector<const MoveDef*> moves = g.content().allMoves();
        const char* def = g.moveLoadout().defaultMove();
        for (size_t i = 0; i < moves.size(); ++i) {
            const MoveDef* mv = moves[i];
            const bool owned = g.moveLoadout().owns(mv->id) ||
                                (def && std::strcmp(def, mv->id) == 0);
            appendKeyString(out, mv->id, owned ? "owned" : "locked", i + 1 < moves.size());
        }
    }
    out += "},";

    // --- achievements{} -------------------------------------------------------
    // Straight off the table (content/content_achievements.cpp), so a new row reaches
    // the site with no edit here — there is no second list to keep in step.
    appendJsonString(out, "achievements");
    out += ':';
    out += '{';
    for (int i = 0; i < kAchievementCount; ++i) {
        const AchievementDef& d = kAchievements[i];
        appendKeyString(out, d.id, g.hasAchievement(d) ? "complete" : "incomplete",
                        i + 1 < kAchievementCount);
    }
    out += "},";

    // --- achievement_progress{} ----------------------------------------------
    // How far along each unfinished ladder is, as "value/goal" — the device has no
    // achievement browser, so the 'Pedia is where a player finds out they are three
    // sub-areas short. Only the countable rows appear; a one-off moment has nothing
    // meaningful between 0 and done.
    appendJsonString(out, "achievement_progress");
    out += ':';
    out += '{';
    {
        bool first = true;
        for (int i = 0; i < kAchievementCount; ++i) {
            const AchievementDef& d = kAchievements[i];
            if (d.series == AchSeries::Event) continue;
            const int goal = achievementGoal(d);
            if (goal <= 0) continue;
            if (!first) out += ',';
            first = false;
            appendJsonString(out, d.id);
            out += ':';
            out += '{';
            appendKeyInt(out, "value", g.achValue(d));
            appendKeyInt(out, "goal", goal, /*comma=*/false);
            out += '}';
        }
    }
    out += "},";

    // --- archive[] (ARCH [RETIRED]/[CORRUPTED] records) ----------------------
    // TODO(pedia-v2): SaveRecord only carries {id, status, generation} — there
    // is no per-instance nickname or final-stats snapshot, so `name` falls back
    // to the species display name and `record` is a bare generation note rather
    // than the flavorful "N care mistakes · N days uptime · ..." the fixture
    // shows. Real flavor text needs the record to capture more at CSF/retire time.
    appendJsonString(out, "archive");
    out += ':';
    out += '[';
    {
        const std::vector<SaveRecord>& records = g.records();
        for (size_t i = 0; i < records.size(); ++i) {
            const SaveRecord& r = records[i];
            const CreatureDef* def = g.content().creature(r.id);
            const char* displayName = def ? def->displayName : r.id;
            const char* stage = def ? stageTag(def->stage) : "?";
            const char* cause =
                (static_cast<RecordStatus>(r.status) == RecordStatus::Retired) ? "retired"
                                                                                : "crashed";
            char recordNote[48];
            std::snprintf(recordNote, sizeof(recordNote), "generation %d", r.generation);

            out += '{';
            appendKeyString(out, "name", displayName);
            appendKeyString(out, "species", r.id);
            appendKeyString(out, "stage", stage);
            appendKeyString(out, "cause", cause);
            appendKeyString(out, "record", recordNote, /*comma=*/false);
            out += '}';
            if (i + 1 < records.size()) out += ',';
        }
    }
    out += ']';

    out += '}';
    return out;
}

} // namespace mal
