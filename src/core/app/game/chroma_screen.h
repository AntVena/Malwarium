// chroma_screen.h: the CHROMATOPHORE board (Nav::Chroma), the Metamorphic line's hatch
// game and a GAMES cabinet, over the Chromatophore run (core/model/chromatophore.h).
//
// The run holds the rules. The screen holds nothing of its own: it owns the skins'
// colours and textures, the water, the chips and the creature drawn in the skin it
// wears, and turns A, B and C into wearing one skin each. B on the parked result banks
// and leaves. What a run buys is the game's (ChromaHost::finishChroma).
#pragma once

#include <cstdint>

#include "core/app/game/board_host.h"

namespace mal {

class Chromatophore;
struct CreatureDef;
struct SpriteData;

class ChromaHost : public BoardHost {
public:
    virtual Chromatophore& liveChroma() = 0;
    virtual const Chromatophore& chroma() const = 0;
    // Who stands on the board: the pet, or the egg of the line the cabinet was built for.
    virtual const CreatureDef* chromaSubject() const = 0;
    virtual const SpriteData* creatureSprite(const CreatureDef& c) const = 0;
    virtual uint32_t bootHatchRemainMs() const = 0;   // what the passes are spent against
    virtual void finishChroma() = 0;

protected:
    ~ChromaHost() = default;
};

class ChromaScreen {
public:
    void onButton(ChromaHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const ChromaHost& host) const;
};

}  // namespace mal
