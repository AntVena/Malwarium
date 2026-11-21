// pedia_state.cpp. See pedia_state.h.
#include "core/app/pedia_state.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

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

// Writes one JSON document and places every comma itself, so no section counts its
// own elements. `key` opens a member and the next value completes it.
class JsonWriter {
public:
    explicit JsonWriter(std::string& out) : out_(out) {}

    void beginObject() { value(); out_ += '{'; first_.push_back(true); }
    void endObject() { out_ += '}'; first_.pop_back(); }
    void beginArray() { value(); out_ += '['; first_.push_back(true); }
    void endArray() { out_ += ']'; first_.pop_back(); }

    void key(const char* k) { separate(); appendJsonString(out_, k); out_ += ':'; keyed_ = true; }
    void str(const char* v) { value(); appendJsonString(out_, v); }
    void num(long v) { value(); out_ += std::to_string(v); }
    void null() { value(); out_ += "null"; }

    void field(const char* k, const char* v) { key(k); str(v); }
    void fieldNum(const char* k, long v) { key(k); num(v); }

private:
    void separate() {
        if (first_.empty()) return;
        if (!first_.back()) out_ += ',';
        first_.back() = false;
    }
    void value() {
        if (keyed_) { keyed_ = false; return; }
        separate();
    }

    std::string& out_;
    std::vector<bool> first_;
    bool keyed_ = false;
};

// Uppercase, no-space stage tag ("PROCESS"/"SCRIPT"/"DAEMON"/"BOOT SECTOR"):
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

// --- the landing fields ------------------------------------------------------
void writeLanding(JsonWriter& w, const Game& g) {
    w.field("hacker_tag", g.hackerTag());
    w.fieldNum("currency_bits", g.bits());
    w.fieldNum("hacker_rank", g.hackerRank());
    w.fieldNum("networks_seen", g.networksSeen());
    w.fieldNum("handshakes", g.handshakesSeen());   // lifetime captures (Audit mode)
    // The same tally PROFILE's SPECIES row shows, so the site and the device agree.
    w.fieldNum("species_raised", g.speciesRaised());
    // Shown in the site's user section, in the field that edits it. A download address
    // the operator set, so not a secret.
    w.field("update_url", g.updateJob().manifestUrl());
}

// --- active_pet: the landing page's live pet card ---------------------------
void writeActivePet(JsonWriter& w, const Game& g, const CreatureDef& pet) {
    w.beginObject();
    // web/app.js matches "species" against D.creatures[].id: the content id.
    w.field("species", pet.id);
    w.field("id", pet.id);
    w.field("stage", stageTag(pet.stage));
    w.field("line", pet.line ? pet.line : "");
    w.field("uptime", formatHms(g.lifetimeUptimeMs()).c_str());
    w.fieldNum("frag_pct", g.model().fragmentation());
    w.fieldNum("mistakes", g.model().careMistakes());

    // The kit this pet COULD hold, by the line rule (moveAllowedForLine). Which of them
    // it has learned is the top-level moves{} map: this is the scope, that the state.
    w.key("learnable");
    w.beginArray();
    for (const MoveDef* mv : g.content().allMoves())
        if (mv && moveAllowedForLine(*mv, pet.line)) w.str(mv->id);
    w.endArray();

    // The per-pet one-shots only a new egg clears (itemIsOncePerPetLifetime): the one
    // part of a pet's state the site cannot work out from the catalogue.
    w.key("lifetime_items");
    w.beginObject();
    for (const ItemDef* it : g.content().allItems())
        if (it && itemIsOncePerPetLifetime(*it))
            w.field(it->id, g.lifetimeItemSpent(*it) ? "spent" : "unspent");
    w.endObject();
    w.endObject();
}

// --- the reveal maps: one state word per catalogue row -----------------------
// Each is enumerated off its content table, so a new row reaches the site with no edit
// here.
void writeRevealMaps(JsonWriter& w, const Game& g) {
    // "hatched" (creatureRaised: the permanent tally, so a creature stays revealed after
    // its successor stamps over it) wins over "seen" (faced, never raised).
    w.key("pets");
    w.beginObject();
    for (const CreatureDef* c : g.content().allCreatures())
        w.field(c->id, g.creatureRaised(c->id) ? "hatched"
                       : g.creatureSeen(c->id)  ? "seen"
                                                 : "locked");
    w.endObject();

    // The append-only wild roster (kWildMalbeastIds, save v25). Bosses and Sim dummies
    // never set either bit.
    w.key("malbeasts");
    w.beginObject();
    for (int i = 0; i < kWildMalbeastCount; ++i)
        w.field(kWildMalbeastIds[i], g.malbeastDefeated(i) ? "defeated"
                                     : g.malbeastSeen(i)    ? "seen"
                                                             : "locked");
    w.endObject();

    // Unlocked = EVER held (itemCollected), the tally the cuisine ladders count. Keyed on
    // the bag, a pantry would empty itself as its food was eaten.
    w.key("items");
    w.beginObject();
    for (const ItemDef* it : g.content().allItems())
        w.field(it->id, g.itemCollected(it->id) ? "unlocked" : "locked");
    w.endObject();

    // The kitchen's second axis: a dish can be met (items{}) without its method known.
    w.key("recipes");
    w.beginObject();
    for (int i = 0; i < kMergeRecipeCount; ++i) {
        const MergeRecipe& r = kMergeRecipes[i];
        w.field(r.outputId, g.recipeKnown(r.outputId) ? "known" : "locked");
    }
    w.endObject();

    // Owned = a held spare OR currently installed.
    w.key("mods");
    w.beginObject();
    for (const ModDef* m : g.content().allMods()) {
        const bool owned = g.modPool().owns(m->id) || g.modSlots().slotOf(m->id) >= 0;
        w.field(m->id, owned ? "unlocked" : "locked");
    }
    w.endObject();

    // Owned = in the owned pool OR the innate default move.
    w.key("moves");
    w.beginObject();
    const char* def = g.moveLoadout().defaultMove();
    for (const MoveDef* mv : g.content().allMoves()) {
        const bool owned = g.moveLoadout().owns(mv->id) || (def && std::strcmp(def, mv->id) == 0);
        w.field(mv->id, owned ? "owned" : "locked");
    }
    w.endObject();
}

// --- achievements: done or not, and how far along each ladder is -------------
// The device has no achievement browser, so the site is where a player finds out they
// are three sub-areas short. A one-off moment has no progress to show.
void writeAchievements(JsonWriter& w, const Game& g) {
    w.key("achievements");
    w.beginObject();
    for (int i = 0; i < kAchievementCount; ++i) {
        const AchievementDef& d = kAchievements[i];
        w.field(d.id, g.hasAchievement(d) ? "complete" : "incomplete");
    }
    w.endObject();

    w.key("achievement_progress");
    w.beginObject();
    for (int i = 0; i < kAchievementCount; ++i) {
        const AchievementDef& d = kAchievements[i];
        if (d.series == AchSeries::Event) continue;
        const int goal = achievementGoal(d);
        if (goal <= 0) continue;
        w.key(d.id);
        w.beginObject();
        w.fieldNum("value", g.achValue(d));
        w.fieldNum("goal", goal);
        w.endObject();
    }
    w.endObject();
}

// --- archive[]: the ARCH [RETIRED]/[CORRUPTED] records -----------------------
// A SaveRecord keeps only {id, status, generation}, so `name` is the species' display
// name and `record` names the generation.
void writeArchive(JsonWriter& w, const Game& g) {
    w.key("archive");
    w.beginArray();
    for (const SaveRecord& r : g.records()) {
        const CreatureDef* def = g.content().creature(r.id);
        const bool retired = static_cast<RecordStatus>(r.status) == RecordStatus::Retired;
        char recordNote[48];
        std::snprintf(recordNote, sizeof(recordNote), "generation %d", r.generation);
        w.beginObject();
        w.field("name", def ? def->displayName : r.id);
        w.field("species", r.id);
        w.field("stage", def ? stageTag(def->stage) : "?");
        w.field("cause", retired ? "retired" : "crashed");
        w.field("record", recordNote);
        w.endObject();
    }
    w.endArray();
}

} // namespace

std::string buildPediaStateJson(const Game& g) {
    std::string out;
    out.reserve(2048);
    JsonWriter w{out};
    w.beginObject();
    writeLanding(w, g);
    w.key("active_pet");
    if (const CreatureDef* pet = g.pet()) writeActivePet(w, g, *pet);
    else w.null();   // no pet yet (empty save, pre-Hatch)
    writeRevealMaps(w, g);
    writeAchievements(w, g);
    writeArchive(w, g);
    w.endObject();
    return out;
}

} // namespace mal
