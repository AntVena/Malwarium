// decryption_screen.h — DISK DECRYPTION, the Ransomware line's hatch minigame.
//
// One screen for the whole game: the history, the row being built, and the verdict.
// Every played row stays on it, because the deduction is the game and nothing here is
// worth deducing off memory.
//
// The rules are core/model/disk_decryption.h; this file is the board and the copy.
#pragma once

namespace mal {

class Framebuffer;
class DiskDecryption;

// The board. `showExactHints` is the EASY setting: it spends a played row's
// fragmentation damage PER CELL, so a cell that was exactly right stays clean — which
// is information the standard rules keep anonymous, and why nothing but the arcade's
// easy cabinet may pass true. `arcade` swaps the prize line, which is otherwise priced
// in incubation a cabinet run doesn't have.
void drawDiskDecryption(Framebuffer& fb, const DiskDecryption& d, bool showExactHints,
                      bool arcade);

}  // namespace mal
