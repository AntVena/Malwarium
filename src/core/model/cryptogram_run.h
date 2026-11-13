// cryptogram_run.h: one Decryptogram board, a run (CONTRIBUTING, "Where state lives").
// From a ticket cashed at the VAULT (or the quote cabinet's START) until B leaves the
// verdict. Cryptogram is the board's rules; this adds which quote is being played and
// the stake it is played for. Runtime-only and never saved: a reboot mid-quote forfeits
// the ticket. What does persist is each quote's standing (OperatorTallies::quoteStates).
//
// Game holds it only while the board is up, verdict included, and drops it on the way
// out to the VAULT or the arcade's till.
#pragma once

#include "core/content/content_quotes.h"
#include "core/model/cryptogram.h"

namespace mal {

struct CryptogramRun {
    Cryptogram board;
    int quote = -1;          // roster row being played (content_quotes.h index)
    // The stake, fixed at the start so the board can state what the next placement is
    // riding on: the Bits a solve pays, and the unlock beside them (None for a replay,
    // and for a cabinet run, which the arcade's till prices instead).
    int prizeBits = 0;
    QuoteReward prize = {};

    // The caller then deals the board (Cryptogram::reset) at the quote's rung.
    static CryptogramRun started(int quote, int prizeBits, const QuoteReward& prize) {
        CryptogramRun r;
        r.quote = quote;
        r.prizeBits = prizeBits;
        r.prize = prize;
        return r;
    }
};

}  // namespace mal
