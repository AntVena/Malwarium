// canvas.h — fixed logical-canvas geometry for the render pipeline.
//
// The engine always draws at the logical resolution and upscales the whole
// canvas by kScaleNum/kScaleDen. Board-specific physical values live in
// include/config.h; these engine constants are board-agnostic.
#pragma once

namespace mal {

constexpr int kLogicalW = 128;   // square authoring base
constexpr int kLogicalH = 128;
constexpr int kScaleNum = 7;     // x1.75 = 7/4 integer ratio
constexpr int kScaleDen = 4;
constexpr int kActiveW = kLogicalW * kScaleNum / kScaleDen;  // 224
constexpr int kActiveH = kLogicalH * kScaleNum / kScaleDen;  // 224
constexpr int kPanelW = 240;
constexpr int kPanelH = 240;
constexpr int kBezel = 8;        // hardware-ignored guard band per edge

constexpr int kHeartbeatMs = 250;  // ~4fps event-driven repaint budget
// Combat sprite motion (game_core.cpp) ticks on this faster, LOCAL cadence instead of
// kHeartbeatMs while a fight is on screen — precedented by CORRUPTION ticking faster
// only while active. Doesn't touch kHeartbeatMs itself, so turn pacing / idle / battery
// cost everywhere else is unaffected; only combat's own redraw rate doubles.
constexpr int kCombatAnimMs = kHeartbeatMs / 2;        // ~8fps while Nav::Combat is up

// Idle-canvas vertical zones, in active (224) space.
constexpr int kTrackH = 40;                       // top / bottom carousel tracks
constexpr int kLivingTop = kTrackH;               // 40
constexpr int kLivingBottom = kActiveH - kTrackH; // 184 — the "shelf" pet feet rest on
constexpr int kSlotCols = 4;
constexpr int kSlotW = kActiveW / kSlotCols;      // 56

// Hybrid render model: creature/pixel-art authored in 128-logical space and
// upscaled x1.75 into the active canvas; UI chrome drawn at native active res.
constexpr int logicalToActive(int v) { return v * kScaleNum / kScaleDen; }

} // namespace mal
