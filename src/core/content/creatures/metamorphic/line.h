// The Metamorphic family — the line that has not decided what it is.
//
// Every other family converges on a plan: Ransomware toward a wall or a bill, Phishing
// toward a bubble it spends, Trojan toward a hijack, Worm toward a board. This one
// refuses to, and its whole design follows from that refusal — its slots roll out of a
// pool far wider than a kit, so it fields whatever comes to hand, and it pays for having
// no plan by acting more often than anything it faces. The identity mechanic, the
// Polymorph passive and the picker LOCK that lets an operator mutate toward a strategy
// are specified in ../../LINE_MOVE_IDENTITIES.md.
//
// Registered in creature_lines.h with NO passive flag: Polymorph gates on combat state
// this line's own rows populate — a slot holding a wildcard move — the way Perfect Bite
// gates on a live bubble rather than on an id, so the turn engine never learns the name.
//
// The roster's two-mod invariant is met the way every line meets it (test_combat.cpp walks
// kCreatureLines to enforce it): Junk Padding is the soft affinity in the Bayou band, and
// Mutation Engine is the hard-gated amplifier at the bottom of the dive. The amplifier
// pays on a DIFFERENT axis than the passive beside it — distinct effect KINDS landed
// rather than distinct moves cast — so a run of plain swings ramps the pet and pays the
// mod nothing.
//
// Slot typing runs Attack/Defend alternating rather than leaning one way. The line has to
// be able to field a metamorphic row of EITHER kind — a wild Attack row and a wild Defend
// row draw from different pools and are the line's two halves — so a kit that could only
// hold one of them would only ever play half of it. The Daemon branch then splits slot 3
// the way every branch does: Good takes the Defend, Bad takes the Attack.
#pragma once

#include "core/content/defs.h"
#include "tunables.h"

namespace mal {

inline constexpr CreatureDef kMetamorphicCreatures[] = {
    // The egg. Deepstaria is the jellyfish that reads as a loose bag turning itself inside
    // out, which is the line's thesis before it has a body to state it with; POLY- is the
    // tech half, off polymorphic code rather than off any one shape.
    //
    // The 8-frame sheet is both the resting loop and the hatch one-shot, the shape both
    // other eggs use. Frames 0-2 all read as sealed because idleFrame() breathes between
    // 0 and 1 and blinks to 2 while the egg is only sitting there; the collapse runs 3-7,
    // where Game::hatchRevealFrame's one-shot is the only thing that reaches it. It sags
    // and splays rather than cracking, because a bell has no shell to break.
    //
    {"polystaria", "Polystaria", Stage::BootSector, "SPR_PET_EGG_META_HATCH", "cuttlefork",
     nullptr, nullptr, 100, 100, "metamorphic",
     "A translucent veil folded around something that has not settled on a shape. Maybe what it holds... is potential.",
     "Metamorphic malware / mutation engines",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Attack, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, /*evolvesToTrojanBadId=*/nullptr, Locomotion::Swim},

    {"cuttlefork", "Cuttlefork", Stage::Process, "SPR_PET_CUTTLEFORK", "morphopus",
     nullptr, nullptr, 100, 100, "metamorphic",
     "A palm-sized cuttlefish running a new colour down its skin every few seconds, forking off a copy of itself for each pattern it likes and losing track of which one it started as.",
     "Code permutation / process forking",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Attack, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, /*evolvesToTrojanBadId=*/nullptr, Locomotion::Swim,
     // Four sculpts, one creature: the line's whole argument is that it does not hold a
     // shape, so the shape is what changes between contexts rather than a pose over one
     // drawing. Row 0 hovers and takes every settled moment. Row 1 is the stretched
     // travelling torpedo and plays only while the wander is actually travelling. Row 2
     // is the flamboyant threat display — papillae up, violet flushed — and row 3 is the
     // blanched flinch.
     //
     // BOTH ATTACK FRAMES ARE A COMPLETE STRIKE, and that is a requirement rather than a
     // style. frameAt() indexes off the global anim beat and nothing restarts a clip when
     // a swing begins, so a swing's kAttackHopPeriod window (ui/combat_screen.cpp) can
     // open on either column and end on either. A windup-then-strike pair would show its
     // windup last as often as first. So the two frames differ in DETAIL — which arm is
     // forward, where the bands sit — never in phase, and holdBeats=2 walks the pair
     // across the window exactly once.
     /*clips=*/{{"idle", /*row=*/0, /*frames=*/4, /*holdBeats=*/2},
                {"walk", /*row=*/1, /*frames=*/1},
                {"attack", /*row=*/2, /*frames=*/2, /*holdBeats=*/2},
                {"hurt", /*row=*/3, /*frames=*/1}}},
    
    {"morphopus", "Morphopus", Stage::Script, "SPR_PET_MORPHOPUS", nullptr,
     /*good=*/"syncaelia", /*bad=*/"tentaclone", 100, 100, "metamorphic",
     "Eight arms, countless forms. Whatever you were about to do, it has already been something that beats it.",
     "Mimicry / instruction substitution",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Attack, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, /*evolvesToTrojanBadId=*/nullptr, Locomotion::Swim,
     /*clips=*/{{"idle", /*row=*/0, /*frames=*/2, /*holdBeats=*/2},
                {"attack", /*row=*/1, /*frames=*/3, /*holdBeats=*/2},
                {"walk", /*row=*/2, /*frames=*/2, /*holdBeats=*/2},
                {"hurt", /*row=*/3, /*frames=*/1}}},
    
    {"syncaelia", "Syncaelia", Stage::Daemon, "SPR_PET_SYNCAELIA",
     nullptr, nullptr, nullptr, kBranchGoodPowerPct, kBranchGoodFragPct, "metamorphic",
     "It can hold a very convincing surface-level conversation for a creature that's never bothered to learn what words actually mean. Don't let yourself get pulled in too deep. It can hold a conversationalist too.",
     "Behavioural mimicry / signature synchronisation",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Attack, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, /*evolvesToTrojanBadId=*/nullptr, Locomotion::Swim,
     // One row of eight 71x64 cells — the widest cell on the line, because the frame is
     // sized to the creature rather than the creature trimmed to a frame. Its sibling's
     // constraint applies here too: 64 is the Daemon box's ceiling, so the head cannot
     // rise and the idle lives below it — the arm tips curling and uncurling, the two
     // raised shoulder arms swaying in and back out, and a pulse travelling along the
     // disguise lights.
     //
     // WHAT THE ROW IS ACTUALLY HOLDING STILL IS THE FACE. This creature's whole claim
     // is that it holds a convincing surface, and a surface that re-resolves its own
     // eyes every column stops being one — so the eye box occupies identical
     // coordinates in all eight cells and only its LIT AREA breathes, which reads as a
     // slow narrowing rather than as a face being redrawn. The bead collar and the
     // swept-back horn arms beside it are pinned for the same reason: the face is where
     // this line keeps its signature (assets/CREATURE_VISUAL_RULES.md §2), so it is the
     // one region an eight-column loop may not spend.
     //
     // holdBeats=2 matches its Script parent above rather than its sibling below, and
     // that is the branch read: the pulse is a travelling wave and wants the slower
     // clock, where Tentaclone's pinned crown wants the faster one.
     //
     // ROW 1 IS A TORQUE, AND THE TORQUE IS WHAT MAKES IT LEGAL. 64 is the Daemon box's
     // ceiling and the drawing already reaches it, so a strike that gains reach has
     // nowhere to go; what the row spends instead is the pair of small arms rising off
     // the torso, slashing forward as the body twists — one leading as the twist goes
     // one way, the other as it comes back. The mantle surges about 2 logical px with
     // it, which is why two of the four columns carry the crest flush to the cell's top
     // row rather than at the drawing's usual 2px inset.
     //
     // A twist is CYCLIC where a swing is PHASED, and that is the whole reason to draw
     // the attack this way. frameAt() indexes off the global beat and nothing restarts a
     // clip when a swing begins, so the window can open on any column — a row drawn as
     // wind-up-then-strike shows its wind-up last as often as first. A row where every
     // column is mid-twist has no wind-up to show: each one already has an arm swung
     // out, and they differ in WHICH arm and how far, never in how ready the creature
     // is. holdBeats=1 against kAttackHopPeriod=4 (core/ui/combat_screen.cpp) walks all
     // four across the window exactly once.
     //
     // Columns 4-7 are deliberately empty rather than holding the frames that were cut.
     // Two were dropped for surging past the cell and two for being the calm ends of the
     // twist, and a calm column is exactly what must not be reachable by raising
     // `frames` later.
     /*clips=*/{{"idle", /*row=*/0, /*frames=*/8, /*holdBeats=*/2},
                {"attack", /*row=*/1, /*frames=*/4, /*holdBeats=*/1}}},
    
    {"tentaclone", "Tentaclone", Stage::Daemon, "SPR_PET_TENTACLONE",
     nullptr, nullptr, nullptr, kBranchBadPowerPct, kBranchBadFragPct, "metamorphic",
     "Eight arms folded into the shape of a person. It holds the pose well at a distance, and not at all once it decides it no longer needs to.",
     "Malware cloning / entry-point obscuring",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Attack, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, /*evolvesToTrojanBadId=*/nullptr, Locomotion::Swim,
     // One row of eight 64x64 cells. The Daemon cell is 64 tall, which is the box's
     // ceiling, so the pose has no room to rise and falls: the crown is pinned and the
     // whole idle lives in the arms — the chest curtain swaying and the side arms
     // breathing. gen_assets cuts this sheet only because FRAME_W_OVERRIDES names it;
     // 512 does not divide by 56, and 64 does not divide by the 48 a row is measured in,
     // so the height buys one row rather than more.
     /*clips=*/{{"idle", /*row=*/0, /*frames=*/5, /*holdBeats=*/1}}},
};
inline constexpr int kMetamorphicCreatureCount =
    sizeof(kMetamorphicCreatures) / sizeof(kMetamorphicCreatures[0]);

}  // namespace mal
