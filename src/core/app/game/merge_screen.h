// merge_screen.h: the Hacker face's MERGE HUB slot, as a feature that owns its own state.
//
// The recipe list, folded to titles with the focused recipe unfolding its ingredients.
// The screen holds its cursor; which recipes are owned and whether one can be cooked are
// the game's (game_merge.cpp), reached through MergeHost.
#pragma once

#include "core/device/platform.h"

namespace mal {

class ContentRegistry;
class Framebuffer;
class Inventory;

// What MERGE needs from the game. Game implements it; a test can too.
class MergeHost {
public:
    virtual const Inventory& inventory() const = 0;
    virtual const ContentRegistry& content() const = 0;
    virtual int screenBeat() const = 0;
    virtual bool mergeHubUnlocked() const = 0;
    virtual bool recipeOwned(int recipeIndex) const = 0;      // won off a Decryptogram
    virtual bool recipeCraftable(int recipeIndex) const = 0;  // owned, ingredients in the bag

    virtual void craftRecipe(int recipeIndex) = 0;
    virtual void leaveToCarousel() = 0;

protected:
    ~MergeHost() = default;
};

class MergeScreen {
public:
    void enter() { row_ = 0; }
    void onButton(MergeHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const MergeHost& host) const;

private:
    int row_ = 0;   // a kMergeRecipes row
};

}  // namespace mal
