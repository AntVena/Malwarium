// away_digest_screen.h: WHILE YOU WERE AWAY (Nav::AwayDigest), the summary the first
// press after a long powered-on quiet is spent on.
//
// Lockouts expire, errors land, fights are won and lost, caches turn up, mostly while
// nobody is holding the device. The game keeps the tally of it (AwayTally) as it
// happens and decides when the summary is due; the screen holds how long the quiet was,
// lays the tally out as rows and draws them, and B or C closes it.
#pragma once

#include <cstdint>
#include <vector>

#include "core/device/platform.h"

namespace mal {

class Framebuffer;
struct CreatureDef;

// What happened since the last button press. Runtime-only: a reboot starts a fresh one.
struct AwayTally {
    int hungry = 0;          // Lockouts fired (FED hit zero)
    int missed = 0;          // ...of them expired unanswered
    int errors = 0;          // care errors charged (addCareMistakeShielded)
    int won = 0, lost = 0;   // wild fights on the walk
    int caches = 0;          // sealed caches found
    const CreatureDef* hatched = nullptr;   // the egg hatched into this
    const CreatureDef* evolved = nullptr;   // the pet evolved into this
};

// Whether a row cost the pet something, which the summary inks as a warning. The word
// carries it in grayscale; the colour only grades it.
enum class AwayRowTone : uint8_t { Plain, Cost };

struct AwayRow {
    const char* label = "";
    char value[20] = "";
    AwayRowTone tone = AwayRowTone::Plain;
};

// What the summary needs from the game. Game implements it; a test can too.
class AwayDigestHost {
public:
    virtual const AwayTally& awayTally() const = 0;
    virtual int careMistakes() const = 0;   // the pet's total, which the ERRORS row ends on
    virtual void closeAwayDigest() = 0;     // a fresh tally, back to the habitat

protected:
    ~AwayDigestHost() = default;
};

class AwayDigestScreen {
public:
    void open(uint32_t quietMs) { quietMs_ = quietMs; }
    void onButton(AwayDigestHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const AwayDigestHost& host) const;

    // The tally's rows, in the order they are drawn. Empty when nothing happened, which
    // is when the summary is not worth a press.
    static std::vector<AwayRow> rows(const AwayTally& t, int careMistakes);

private:
    uint32_t quietMs_ = 0;   // the absence the summary covers
};

}  // namespace mal
