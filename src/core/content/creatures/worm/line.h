// The Worm family — the line that fights with a BOARD instead of a swing.
//
// Every row here is weak on its own, and that is the design rather than a tuning gap:
// a worm's damage comes from the copies it puts on the screen (content_moves.cpp's
// "worm" track, resolved in Combat::applyEffect), and its survival comes from those
// copies being what an attack usually hits. Its two passives are Shared Resources —
// speed matched to the opponent's forever, and a fixed pile of replication slots — and
// both live in content_passives.h.
//
// The line hatches from the Vermicell egg (content_evolution.cpp's "worm" EggLineDef),
// which is EARNED rather than offered: it appears at line-select once the archive holds
// two of the same species at once. Its hatch minigame is the Isolation Protocol
// (game_isolation.cpp), and finishing one clean is what fires WORM_WHISPERER.
//
// Every row of the line is drawn now. The line's own art rules are in
// assets/CREATURE_VISUAL_RULES.md: a worm reads SMALLER in its cell than any other
// line's creature, because the replicas need the room beside it, and the family
// spends 1-bit line art where the other lines spend a signature hue. Its drawn rows
// come out of tools/gen_worm_art.py, which holds that style as code.
//
// Everything up to and including the Script row CRAWLS (Locomotion::Ground) — the
// line's identity rather than a per-row tuning choice, which is why it is stated the
// same way three times. Both Daemons then leave the floor, and that is the payoff:
// the one line whose creatures have never once been off the ground earns it last.
#pragma once

#include "core/content/defs.h"
#include "tunables.h"

namespace mal {

inline constexpr CreatureDef kWormCreatures[] = {
    // The egg. Its 8-frame sheet is both the idle loop (frames 0-1) and the hatch
    // sequence (0-7, walked by Game::hatchCrackFrame as the incubation clock runs
    // down) — the same shape SPR_PET_EGG_PHISH_HATCH uses. Drawn 1-bit, like the
    // replica glyphs the line fights with: a shell with one worm coiled inside it and
    // one byte in front of the head, which is the Isolation Protocol seen from outside.
    {"vermicell", "Vermicell", Stage::BootSector, "SPR_PET_EGG_WORM_HATCH", "nodeatode",
     nullptr, nullptr, 100, 100, "worm",
     "A soft translucent capsule with one worm coiled inside it, endlessly chasing a single loose byte around the shell wall.",
     "Worm eggs / a payload waiting on a host",
     {MoveKind::Attack, MoveKind::Attack, MoveKind::Attack, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Ground,
     // Declared only to slow the resting breathe below the shared idleFrame()
     // default (sprite.h) — frames 0-1 of the same 8-frame sheet the hatch
     // one-shot walks, at half that heuristic's cadence (holdBeats 1 -> 2).
     /*clips=*/{{"idle", /*row=*/0, /*frames=*/2, /*holdBeats=*/2}}},
    // Nodeatode is the family's Process pet, and the only row with real character so
    // far. Its own 1-bit sheet is four rows of four 56x48 frames, and the worm occupies
    // barely 30x24 of each cell — the draw-small rule kept literally. The idle row is
    // an S-wave travelling down the spine rather than a rocking of the whole body,
    // which is what stops a crawler's squiggle reading as a bob it no longer takes.
    // Only "idle" has a consumer today; "attack" is declared and waiting, and
    // droop/weak become reachable the moment a mood pose is wired.
    //
    // Its slot typing alternates Attack/Defend from the first slot, unlike the other
    // lines' attack-leaning Process rows. A worm whose kit is all attacks spawns only
    // attacking copies, and an attacking copy with no defender behind it multiplies by
    // the floor — so the alternation is what makes the line's own arithmetic reachable
    // as the slots unlock, not a flavour choice.
    {"nodeatode", "Nodeatode", Stage::Process, "SPR_PET_NODEATODE", "rootgrub",
     nullptr, nullptr, 100, 100, "worm",
     "A thread-thin nematode that chews from one node to the next. Small, slow, and by morning there is never just the one.",
     "Worms / self-replicating network propagation",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Attack, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Ground,
     /*clips=*/{{"idle", /*row=*/0, /*frames=*/4, /*holdBeats=*/2},
                {"attack", /*row=*/1, /*frames=*/4},
                {"droop", /*row=*/2, /*frames=*/2, /*holdBeats=*/3},
                {"weak", /*row=*/3, /*frames=*/2, /*holdBeats=*/3}}},
    // Rootgrub is the fork in the road, which is why the line has ONE Script row where
    // it used to have two. The care branch now resolves a stage later, at the Daemon,
    // the way every other line's does — and this row is what the branch is about: a
    // body that has run out of ways to get longer and has to choose a direction to
    // spend its mass in.
    //
    // Its sheet is the same 4x4 shape as Nodeatode's, and its drawing says the
    // opposite thing with the same vocabulary: short and thick where the Process row
    // is thin and long, and led by a mouth rather than by a head. It is also the one
    // row of the line whose single solid mass is not an eye but a THROAT — see
    // tools/gen_worm_art.py, where both are the same `Cell.solid`.
    {"rootgrub", "Rootgrub", Stage::Script, "SPR_PET_ROOTGRUB", nullptr,
     /*good=*/"shenloop", /*bad=*/"threadbore", 100, 100, "worm",
     "A thumb-thick grub that has stopped chewing between nodes and started chewing through them. Its mouth is the widest part of it now, and it has not settled whether to go on growing out or start growing up.",
     "Resource-exhausting payloads / a worm that trades reach for appetite",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Attack, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Ground,
     /*clips=*/{{"idle", /*row=*/0, /*frames=*/4, /*holdBeats=*/2},
                {"attack", /*row=*/1, /*frames=*/4},
                {"droop", /*row=*/2, /*frames=*/2, /*holdBeats=*/3},
                {"weak", /*row=*/3, /*frames=*/2, /*holdBeats=*/3}}},
    // The two Daemons are the answer to Rootgrub's question, and they are the first
    // rows of the line to leave the floor — which is why the family's Crawl stops
    // here. Both are drawn, in the same tool and the same vocabulary as the two rows
    // above, and neither carries a ground plant: every crawling row of the line ends
    // with a bar on the shelf and these two deliberately have nothing near the bottom
    // of the cell, so the sheets agree with the locomotion rather than relying on the
    // habitat to sell it.
    //
    // Shenloop grew UP. An eastern dragon with no talons, so there is nothing on it
    // for gripping — it does not take a host, it stays connected to one. Swim rather
    // than Fly because that is the mover with no altitude to hold: a serpent in air
    // drifts on both axes, which is the whole read.
    //
    // Its sheet spends the stage on LENGTH and POSTURE where its sibling spends it on
    // width: a constant-radius body with no taper and no head bulb, reared into a
    // column and folded, with the head carried level off the neck. It is the row that
    // takes the line's one solid mass back to an EYE after Rootgrub spent it on a
    // throat, and that single choice is most of what separates the two branches.
    {"shenloop", "Shenloop", Stage::Daemon, "SPR_PET_SHENLOOP",
     nullptr, nullptr, nullptr, kBranchGoodPowerPct, kBranchGoodFragPct, "worm",
     "The same appetite grown upward instead of outward - a long clawless serpent that holds one connection open across the whole network and waits at the far end of it for as long as that takes.",
     "Beacon loops / an implant that holds its channel open and calls home",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Swim,
     /*clips=*/{{"idle", /*row=*/0, /*frames=*/4, /*holdBeats=*/2},
                {"attack", /*row=*/1, /*frames=*/4},
                {"droop", /*row=*/2, /*frames=*/2, /*holdBeats=*/3},
                {"weak", /*row=*/3, /*frames=*/2, /*holdBeats=*/3}}},
    // Threadbore grew OUT. Rootgrub again with everything that was not mouth spent on
    // more mouth, and a pair of wings far too small for what they are lifting. Fly, not
    // Swim: it holds an altitude by working at it, and the difference between the two
    // Daemons is meant to be legible from the resting motion alone.
    //
    // It keeps the THROAT its parent spends its one solid mass on, where its sibling
    // goes back to an eye — both Daemons inherit exactly one thing from Rootgrub, and
    // which one they inherit is the branch. Its wings are the only form the line's
    // drawing vocabulary had to grow to draw: a bare limb out from the body, then a
    // membrane fanned off the wrist in panels, which is a general form in the tool
    // rather than anything this creature owns.
    {"threadbore", "Threadbore", Stage::Daemon, "SPR_PET_THREADBORE",
     nullptr, nullptr, nullptr, kBranchBadPowerPct, kBranchBadFragPct, "worm",
     "Wider than it is long, and almost all of that is jaw. It grew a pair of wings with no business lifting anything this heavy, and lifts anyway, and arrives.",
     "Thread-pool exhaustion / a payload that consumes the host it lands on",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Attack, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Fly,
     /*clips=*/{{"idle", /*row=*/0, /*frames=*/4, /*holdBeats=*/2},
                {"attack", /*row=*/1, /*frames=*/4},
                {"droop", /*row=*/2, /*frames=*/2, /*holdBeats=*/3},
                {"weak", /*row=*/3, /*frames=*/2, /*holdBeats=*/3}}},
};
inline constexpr int kWormCreatureCount =
    sizeof(kWormCreatures) / sizeof(kWormCreatures[0]);

}  // namespace mal
