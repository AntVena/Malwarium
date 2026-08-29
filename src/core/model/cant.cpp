#include "core/model/cant.h"

namespace mal {

namespace {

// The same LCG constants every other roll in the engine advances on, so a cipher seed
// behaves like any other seed a caller already holds.
uint32_t step(uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s >> 16;
}

bool isLetter(char c) { return c >= 'A' && c <= 'Z'; }

char upper(char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c; }

}  // namespace

const char* cantRevealOrder() {
    // Frequency-descending over the riddle pool (content_riddles.cpp), which is close
    // enough to plain English that the usual ETAOIN order is what falls out. The tail
    // is where the two differ and it hardly matters: by the time a pet is owed its
    // twentieth sigil the riddle already reads.
    return "ETAOINSRHLDCUMFPGWYBVKXJQZ";
}

char nextSigil(SigilSet learned) {
    const char* order = cantRevealOrder();
    for (int i = 0; i < kCantSigils; ++i) {
        const char c = order[i];
        if (!(learned & (1u << (c - 'A')))) return c;
    }
    return '\0';
}

SigilSet learnSigil(SigilSet learned) {
    const char c = nextSigil(learned);
    return c ? (learned | (1u << (c - 'A'))) : learned;
}

int sigilCount(SigilSet learned) {
    int n = 0;
    for (int i = 0; i < kCantSigils; ++i)
        if (learned & (1u << i)) ++n;
    return n;
}

int cantFluencyPct(SigilSet learned) {
    return sigilCount(learned) * 100 / kCantSigils;
}

void CantCipher::build(SigilSet learned, uint32_t seed) {
    // Learned letters stand for themselves; that is what a sigil IS, and it is also
    // what keeps the mapping honest when the shuffle below only gets the rest.
    for (int i = 0; i < kCantSigils; ++i) map[i] = static_cast<char>('A' + i);

    int unlearned[kCantSigils];
    int n = 0;
    for (int i = 0; i < kCantSigils; ++i)
        if (!(learned & (1u << i))) unlearned[n++] = i;
    if (n < 2) return;   // nothing to shuffle — see the header's single-letter note

    // Fisher-Yates over the unlearned letters only, so the shuffle can never disturb a
    // sigil the player earned.
    int perm[kCantSigils];
    for (int i = 0; i < n; ++i) perm[i] = unlearned[i];
    uint32_t s = seed ? seed : 1u;
    for (int i = n - 1; i > 0; --i) {
        const int j = static_cast<int>(step(s) % static_cast<uint32_t>(i + 1));
        const int t = perm[i]; perm[i] = perm[j]; perm[j] = t;
    }

    // Derange: a letter that landed on itself is swapped with its neighbour. One sweep
    // is enough because a swap can only introduce a fixed point at the index it came
    // FROM, which the sweep has already passed — except at the last index, which is
    // why that one swaps backward instead.
    for (int i = 0; i < n; ++i) {
        if (perm[i] != unlearned[i]) continue;
        const int j = (i + 1 < n) ? i + 1 : i - 1;
        const int t = perm[i]; perm[i] = perm[j]; perm[j] = t;
    }

    for (int i = 0; i < n; ++i) map[unlearned[i]] = static_cast<char>('A' + perm[i]);
}

char CantCipher::apply(char c) const {
    const char u = upper(c);
    return isLetter(u) ? map[u - 'A'] : c;
}

void CantCipher::applyTo(const char* text, char* out, int cap) const {
    if (!out || cap <= 0) return;
    int n = 0;
    if (text)
        for (const char* p = text; *p && n < cap - 1; ++p) out[n++] = apply(*p);
    out[n] = '\0';
}

}  // namespace mal
