// looks_pages.h: CFG's pages over how the operator presents: THEME, BACKGROUND, TITLE and
// the HACKERTAG editor.
//
// The pages hold the focused row (seeded from what is applied as each opens) and the tag
// being typed with its caret. What is unlocked, what is owned and what is applied are
// the game's, reached through LooksHost, whose setters refuse a locked choice.
//
// This header names no core/ui type beyond ui_state.h, so game.h can hold the pages
// (inside CfgMenu) by value.
#pragma once

#include <cstdint>

#include "tunables.h"                   // kHackerTagMax
#include "core/app/game/cfg_page.h"
#include "core/render/scene_id.h"

namespace mal {

class Framebuffer;
struct Operator;

class LooksHost {
public:
    virtual const Operator& operatorRecord() const = 0;
    virtual int screenBeat() const = 0;
    virtual int themeRow() const = 0;
    virtual uint32_t themesUnlockedMask() const = 0;
    virtual bool setThemePick(int paletteIndex) = 0;
    virtual bool backgroundOwned(SceneId s) const = 0;
    virtual bool setBackgroundPick(SceneId s) = 0;
    virtual int nextSelectableTitle(int cur) const = 0;
    virtual void equipTitle(int sector) = 0;          // -1 for NONE
    virtual bool setHackerTag(const char* tag) = 0;   // false on a tag it refuses

protected:
    ~LooksHost() = default;
};

class LooksPages {
public:
    // Seeds the focus at what is applied, and the editor with the live tag.
    void open(const LooksHost& host, CfgScreen page);

    CfgMove onTheme(LooksHost& host, const ButtonEvent& ev);
    CfgMove onBackground(LooksHost& host, const ButtonEvent& ev);
    CfgMove onTitles(LooksHost& host, const ButtonEvent& ev);
    CfgMove onHackerTag(LooksHost& host, const ButtonEvent& ev);

    void drawTheme(Framebuffer& fb, const LooksHost& host) const;
    void drawBackground(Framebuffer& fb, const LooksHost& host) const;
    void drawTitles(Framebuffer& fb, const LooksHost& host) const;
    void drawHackerTag(Framebuffer& fb) const;

private:
    int pick_ = 0;   // the open picker's row; never open together, so one field
    // The HackerTag editor: the tag being typed, and the caret (kHackerTagMax is the
    // confirm cell).
    char tag_[kHackerTagMax + 1] = {0};
    int caret_ = 0;
};

}  // namespace mal
