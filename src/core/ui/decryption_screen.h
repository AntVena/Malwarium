// decryption_screen.h — DISK DECRYPTION, the Ransomware line's hatch minigame.
//
// One screen for the whole game: the history, the row being built, and the verdict.
// Every played row stays on it, because the deduction is the game and nothing here is
// worth deducing off memory.
//
// The rules are core/model/disk_decryption.h; this file is the board and the copy.
#pragma once

#include <cstdint>

namespace mal {

class Framebuffer;
class DiskDecryption;

// How a played row's fragmentation damage lands. Exact is the EASY setting: it spends
// the damage PER CELL, so a cell that was exactly right stays clean. That is information
// the standard rules keep anonymous, which is why nothing but the arcade's easy cabinet
// may pass Exact.
enum class DecryptHints : uint8_t { Anonymous, Exact };
// Where the board is played. Arcade swaps the prize line, which is otherwise priced in
// incubation a cabinet run doesn't have.
enum class DecryptStakes : uint8_t { Hatch, Arcade };

// The board.
//
// `beat` re-rolls the corruption dither on every repaint. A played row's damage is
// drawn as rolls into a 3x3 grid that may collide, so any ONE frame is a sample of the
// damage and not a reading of it; animating it lets the eye average the samples back
// into the real number. It shares the dither with the FRAG gauge (widgets.h), because
// both are drawing the same stat.
void drawDiskDecryption(Framebuffer& fb, const DiskDecryption& d, DecryptHints hints,
                        DecryptStakes stakes, int beat);

}  // namespace mal
