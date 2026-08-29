// cant.h — the CANT: the guardians' language, its sigils, and the ciphers it is
// spoken in. No rendering and no Game, the way stacker.h and cryptogram.h are kept.
//
// A guardian does not speak the 'net's alphabet. What it puts to a pet is a
// SHIBBOLETH — a riddle drawn in the Cant — and the pet answers by picking one of
// three replies drawn the same way. Understanding is not required to answer: three
// replies means a blind pick is one in three, which is the deal offered from the very
// first encounter.
//
// WHAT A SIGIL IS. One letter of the Cant the pet has learned to read. A learned
// letter is drawn AS ITSELF; every letter still unlearned is drawn as some other. So
// fluency is literally legibility — the same riddle clarifies in place as the sigils
// come in, and the last one is the difference between a guess and a reading.
//
// WHY A SUBSTITUTION AND NOT A SCRIPT. The cipher is one-letter-for-one-letter, which
// makes it WIDTH-PRESERVING: a riddle that fits the panel at zero sigils fits it at
// twenty-six, so the layout gate has one text to check and never a worst case that
// only appears late in a save. It also needs no glyph the face does not already
// have — FONT_UI is ASCII 32..126 (tools/gen_font.py), so a runic or dingbat script
// would have to ship a second face to say the same thing.
//
// The cipher is REROLLED for every encounter, so a mapping cannot be memorised in
// place of learning the Cant, and it is a DERANGEMENT of the unlearned letters, so a
// letter drawn as itself always means a sigil and never a coincidence.
#pragma once

#include <cstdint>

namespace mal {

// The Cant is the 26 letters of the alphabet. Digits, spaces and punctuation are
// shared with the 'net and are never enciphered — they carry word shape, which is what
// makes a half-learned riddle worth reading instead of a wall of noise.
constexpr int kCantSigils = 26;

// A set of learned sigils: bit i = the letter 'A'+i reads plain. One uint32_t, which
// is the whole of what the save keeps about the Cant (save v59).
using SigilSet = uint32_t;

// The ORDER sigils are learned in — frequency-descending over the riddle pool's own
// English, so each sigil earned buys the most reading. Fixed rather than random: a
// player's Nth sigil is the same letter on every device, which is what makes the
// progression something two operators can compare and a test can assert.
// kCantSigils characters, no terminator promise beyond the NUL.
const char* cantRevealOrder();

// The letter the NEXT sigil would teach, or '\0' once the whole Cant is known.
char nextSigil(SigilSet learned);

// `learned` plus its next sigil. A no-op on a complete Cant, so a caller that pays a
// sigil for something never has to check first.
SigilSet learnSigil(SigilSet learned);

// How many sigils are in the set (0..kCantSigils).
int sigilCount(SigilSet learned);

// Fluency as a percentage, 0..100 — the roll the guardian's welcome is graded against
// (Game::startShibboleth). Derived rather than stored: fluency IS the sigil count, and
// a second field would be the same fact written twice.
int cantFluencyPct(SigilSet learned);

// One encounter's cipher. Built once when a guardian speaks and thrown away when it
// stops, so nothing about it is persisted — the SIGILS are the durable half, and the
// mapping is deliberately not.
struct CantCipher {
    // What each letter is DRAWN as: map[i] is the glyph standing in for 'A'+i. A
    // learned letter maps to itself; every other maps to some other letter.
    char map[kCantSigils] = {};

    // Build the mapping for `learned`, from `seed`. Deterministic in both, so a test
    // can name a cipher and a device can rebuild the one it is showing.
    //
    // The unlearned letters are shuffled among themselves and any letter that landed
    // on itself is swapped away, so "drawn as itself" means "learned" with exactly one
    // exception: a Cant missing a SINGLE letter cannot be deranged, and that letter is
    // drawn plain. It is deducible by elimination at that point anyway, so the
    // alternative would be a lie rather than a puzzle.
    void build(SigilSet learned, uint32_t seed);

    // `c` as the guardian draws it. A-Z (and a-z, folded up) go through the map;
    // everything else — digits, spaces, punctuation — passes through untouched.
    char apply(char c) const;

    // `text` enciphered into `out` (NUL-terminated, truncated to fit `cap`).
    void applyTo(const char* text, char* out, int cap) const;
};

}  // namespace mal
