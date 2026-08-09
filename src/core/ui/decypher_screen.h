// decypher_screen.h — DISK DECYPHER, the Ransomware line's hatch minigame.
//
// One screen for the whole game: the history, the row being built, and the verdict.
// Every played row stays on it, because the deduction is the game and nothing here is
// worth deducing off memory.
//
// The rules are core/model/disk_decypher.h; this file is the board and the copy.
#pragma once

namespace mal {

class Framebuffer;
class DiskDecypher;

// The board. `showExactHints` is the EASY setting: a history cell that was the right
// colour in the right place is outlined, which is information the standard rules keep
// anonymous — so nothing but the arcade's easy cabinet may pass true. `arcade` swaps the
// prize line, which is otherwise priced in incubation a cabinet run doesn't have.
void drawDiskDecypher(Framebuffer& fb, const DiskDecypher& d, bool showExactHints,
                      bool arcade);

}  // namespace mal
