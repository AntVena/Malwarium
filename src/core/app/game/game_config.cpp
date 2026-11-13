// game_config.cpp: the device settings CFG changes, and the rules over them (part of the
// Game unit split, core/app/game.h). CfgMenu drives them.
#include "core/content/areas/area_defs.h"       // area(): a cleared area pays out its place
#include "core/content/content_backgrounds.h"   // the places an operator owns and picks between
#include "core/content/content_homes.h"         // sceneForCreature: a raised creature brings its own
#include "core/content/content_themes.h"        // the colour sets offered, and what unlocks each
#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/render/palette.h"   // setPalTheme: the one place the theme index moves

namespace mal {

void Game::executeFactoryReset(FactoryScope scope) {
    // The two scopes differ in exactly what the screen promises. RESET PET lays a
    // fresh egg over the same operator: resetToHatch() keeps every player-level
    // tally, which is how 'Pedia progress survives. WIPE EVERYTHING clears that
    // standing state first, so the reset that follows lands on a device with no
    // history. Order matters: the wipe runs BEFORE resetToHatch, so the fresh egg it
    // lays is tallied as raised (a wiped device still knows its own current pet).
    if (scope == FactoryScope::WipeEverything) wipeDeviceProgress();
    resetToHatch();
}

void Game::setUiMode(UiMode m) {
    if (settings_.setUiMode(m)) settingChanged();
}

void Game::cycleUiMode() {
    setUiMode(static_cast<UiMode>((static_cast<int>(settings_.uiMode) + 1) % 3));
}

void Game::setCareAlerts(bool on) {
    if (settings_.setCareAlerts(on)) settingChanged();
}

const char* Game::themeName() const { return kPalThemeNames[settings_.theme]; }

// Which kThemes ROW the applied set is, for the picker's ACTIVE tag and its opening
// focus. The table and the palette are separately ordered on purpose (content_themes.h),
// so this is a lookup and not a cast.
int Game::themeRow() const {
    for (int i = 0; i < kThemeCount; ++i)
        if (palThemeByName(kThemes[i].palette) == settings_.theme) return i;
    return 0;
}

bool Game::themeRowUnlocked(int row) const {
    if (row < 0 || row >= kThemeCount) return false;
    const ThemeDef& t = kThemes[row];
    if (t.source == ThemeSource::Start) return true;
    // Held at some point, not held NOW. The chip is a ROM that was read, so selling or
    // losing it cannot take the set back; see content_themes.h.
    return t.earnedById && hasCollectedItem(t.earnedById);
}

uint32_t Game::themesUnlockedMask() const {
    uint32_t m = 0;
    for (int i = 0; i < kThemeCount; ++i)
        if (themeRowUnlocked(i)) m |= 1u << i;
    return m;
}

bool Game::hasCollectedItem(const char* id) const {
    if (!id) return false;
    for (const ItemDef* d : op_.shelves.collectedItems)
        if (d && std::strcmp(d->id, id) == 0) return true;
    return false;
}

bool Game::setThemePick(int index) {
    if (index < 0 || index >= kPalThemeCount) index = 0;
    // A set nobody has unlocked is not applied, and index 0 always is, so a save
    // naming a set this device has not earned (a chip removed from the tables, a blob
    // from another device) falls back to base.
    int row = -1;
    for (int i = 0; i < kThemeCount; ++i)
        if (palThemeByName(kThemes[i].palette) == index) { row = i; break; }
    if (row < 0 || !themeRowUnlocked(row)) {
        if (index == 0) { setPalTheme(0); settings_.theme = 0; return true; }
        return false;
    }
    setPalTheme(index);            // even when unchanged: a load applies through here
    if (index == settings_.theme) return true;
    settings_.theme = index;
    settingChanged();              // nothing else changed, but every pixel of it did
    return true;
}

void Game::setBrightness(int level) {
    if (settings_.setBrightness(level)) settingChanged();
}

void Game::equipTitle(int sector) {
    op_.world.equippedTitle = sector;
    markSaveDirty();
}

// The one safe write of the tag, from the web 'Pedia's rename and the CFG editor alike:
// the editor's own charset, validated up front so a bad value never mutates the live tag.
bool Game::setHackerTag(const char* s) {
    if (!s) return false;
    const size_t len = std::strlen(s);
    if (len == 0 || len > static_cast<size_t>(kHackerTagMax)) return false;
    for (size_t i = 0; i < len; ++i) {
        const char c = s[i];
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        if (!ok) return false;
    }
    std::strncpy(op_.hackerTag, s, sizeof(op_.hackerTag) - 1);
    op_.hackerTag[sizeof(op_.hackerTag) - 1] = '\0';
    markSaveDirty();
    return true;
}


bool Game::backgroundOwned(SceneId s) const {
    const BackgroundDef* b = backgroundFor(s);
    if (!b) return false;
    switch (b->source) {
        case BackgroundSource::Start:
            return true;
        case BackgroundSource::Raise:
            // Owned once a creature that actually LIVES here has been raised. Asked of
            // the roster rather than recorded on the way past, so the answer keeps up
            // with the homes table: move a line's place and every operator who raised
            // that line already owns the new one.
            for (const CreatureDef* c : op_.shelves.raisedCreatures)
                if (sceneForCreature(*c) == s) return true;
            return false;
        case BackgroundSource::Clear:
            for (int a = 0; a < kAreaCount; ++a)
                if (area(a).scene == s) return op_.world.sectorCleared[a];
            return false;
        case BackgroundSource::Bracket:
            return op_.tallies.tourneyWins >= b->rung;
        case BackgroundSource::Achieve:
            // The achievement bitset is already saved, so this is derived the same
            // way as the other three.
            return b->earnedById && hasAchievement(b->earnedById);
    }
    return false;
}

bool Game::setBackgroundPick(SceneId s) {
    // AUTO always takes, and nothing else does unless it has been earned. A pick is
    // refused rather than clamped: quietly landing on a neighbouring background would
    // be the screen deciding where the operator lives.
    if (s != SceneId::None && !backgroundOwned(s)) return false;
    op_.backgroundPick = s;
    markSaveDirty();
    dirty_ = true;
    return true;
}

void Game::announceBackground(SceneId s) {
    // Ownership is derived, so nothing stores "just earned". It is found by asking
    // before and after the grant. Each of the three callers does that; this is only the
    // saying-so.
    const BackgroundDef* b = backgroundFor(s);
    if (!b) return;
    char line[28];
    std::snprintf(line, sizeof(line), "BG: %s", b->name);
    log_.push(LogEventType::ItemGained, line);
    dirty_ = true;
}

}  // namespace mal
