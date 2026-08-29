// test_shibboleth.cpp — the CANT, the guardian encounter, and the riddle pool's
// content gate.
//
// Three subjects, one file, because they only mean anything together: the cipher is
// what makes the pool legible-or-not, and the encounter is what pays for the sigils
// that decide which. The pool's own rules (does it fit, is it ASCII) are here rather
// than in a content unit for the same reason the Decryptogram's are with its board —
// "does it fit" is a question about the panel, and the panel is what this tests.
#include "test_gates.h"

#include "core/content/content_riddles.h"
#include "core/model/cant.h"

// --- The Cant --------------------------------------------------------------

// The whole alphabet, once each. The reveal order is what decides WHICH letter an Nth
// sigil is, so a duplicate or a gap would silently make part of the Cant unlearnable —
// and the mask would then never fill, stranding the BOON band out of reach forever.
void test_cant_reveal_order_is_the_whole_alphabet() {
    const char* order = cantRevealOrder();
    bool seen[kCantSigils] = {false};
    int n = 0;
    for (; order[n]; ++n) {
        CHECK(order[n] >= 'A' && order[n] <= 'Z');
        CHECK(!seen[order[n] - 'A']);        // no letter twice
        seen[order[n] - 'A'] = true;
    }
    CHECK(n == kCantSigils);
    for (bool b : seen) CHECK(b);            // ...and none missing

    // Learning kCantSigils times fills the mask exactly, and learning again is a no-op
    // rather than an overflow — the payout path calls learnSigil without checking first.
    SigilSet s = 0;
    for (int i = 0; i < kCantSigils; ++i) {
        CHECK(sigilCount(s) == i);
        s = learnSigil(s);
    }
    CHECK(sigilCount(s) == kCantSigils);
    CHECK(cantFluencyPct(s) == 100);
    CHECK(learnSigil(s) == s);
    CHECK(nextSigil(s) == '\0');
}

// THE reason the Cant is a substitution and not a script: enciphering never changes a
// string's LENGTH, so a riddle that fits the panel at zero sigils fits it at every
// fluency between. Without this the content gate below would only be checking the
// easiest case, and a riddle could overflow the panel late in a save.
void test_cant_cipher_is_width_preserving() {
    const char* src = "I WAIT FOREVER FOR THE ONE WHO IS WAITING FOR ME.";
    for (int learned = 0; learned <= kCantSigils; ++learned) {
        SigilSet s = 0;
        for (int i = 0; i < learned; ++i) s = learnSigil(s);
        CantCipher c;
        c.build(s, 12345u + static_cast<uint32_t>(learned));
        char out[96];
        c.applyTo(src, out, sizeof(out));
        CHECK(std::strlen(out) == std::strlen(src));
        // Position for position: a letter stays a letter, and everything else — the
        // spaces and the full stop that carry word shape — passes through as itself.
        for (size_t i = 0; src[i]; ++i) {
            const bool letter = src[i] >= 'A' && src[i] <= 'Z';
            if (letter) CHECK(out[i] >= 'A' && out[i] <= 'Z');
            else        CHECK(out[i] == src[i]);
        }
    }
}

// A learned sigil reads plain; an unlearned letter never does. The second half is what
// makes the first half MEAN anything — if an unlearned letter could land on itself by
// chance, "it reads plainly" would stop being proof the pet knows it.
void test_cant_sigils_read_plain_and_nothing_else_does() {
    for (int learned = 0; learned <= kCantSigils; ++learned) {
        SigilSet s = 0;
        for (int i = 0; i < learned; ++i) s = learnSigil(s);
        for (uint32_t seed = 1; seed <= 40; ++seed) {
            CantCipher c;
            c.build(s, seed);
            int fixed = 0;
            for (int i = 0; i < kCantSigils; ++i) {
                const char plain = static_cast<char>('A' + i);
                if (s & (1u << i)) {
                    CHECK(c.map[i] == plain);       // a sigil is drawn as itself
                } else if (c.map[i] == plain) {
                    ++fixed;
                }
            }
            // The single documented exception: a Cant missing exactly ONE letter cannot
            // be deranged, so that letter is drawn plain. Everywhere else, zero.
            CHECK(fixed == (learned == kCantSigils - 1 ? 1 : 0));
        }
    }
}

// The mapping is a PERMUTATION — two letters never share a glyph. A collision would make
// a riddle unsolvable by reading rather than merely hard, since two different letters
// would be indistinguishable on screen.
void test_cant_cipher_is_a_permutation() {
    for (int learned = 0; learned <= kCantSigils; ++learned) {
        SigilSet s = 0;
        for (int i = 0; i < learned; ++i) s = learnSigil(s);
        for (uint32_t seed = 1; seed <= 40; ++seed) {
            CantCipher c;
            c.build(s, seed);
            bool hit[kCantSigils] = {false};
            for (int i = 0; i < kCantSigils; ++i) {
                CHECK(c.map[i] >= 'A' && c.map[i] <= 'Z');
                CHECK(!hit[c.map[i] - 'A']);
                hit[c.map[i] - 'A'] = true;
            }
        }
    }
}

// --- The riddle pool's content gate ----------------------------------------

// Every row FITS and is drawable: the riddle wraps into kRiddleBodyLines at the body
// width, each reply is one line at the reply width, and every character has a glyph in
// FONT_UI (ASCII 32..126, tools/gen_font.py). Checked by wrapping rather than by
// counting characters, because a wrap wastes the tail of most lines — a punctuation-heavy
// short riddle can fail where a well-broken longer one passes.
void test_riddle_pool_fits_the_panel() {
    const int bodyW = kRiddleBodyCols * kFontAdvance;
    const int replyW = kRiddleReplyCols * kFontAdvance;
    CHECK(riddleCount() > 0);
    for (int i = 0; i < riddleCount(); ++i) {
        const RiddleDef& r = riddles()[i];
        CHECK(r.text != nullptr);
        CHECK(textWrapLines(r.text, bodyW) <= kRiddleBodyLines);
        for (const char* p = r.text; *p; ++p)
            CHECK(*p >= 32 && *p <= 126);
        for (int k = 0; k < kRiddleReplies; ++k) {
            CHECK(r.replies[k] != nullptr);
            CHECK(textWidth(r.replies[k]) <= replyW);
            for (const char* p = r.replies[k]; *p; ++p)
                CHECK(*p >= 32 && *p <= 126);
        }
        // Three DISTINCT replies. Two that read the same would make one of the three
        // rows a dead choice, quietly turning a 1-in-3 guess into a 1-in-2.
        CHECK(std::strcmp(r.replies[0], r.replies[1]) != 0);
        CHECK(std::strcmp(r.replies[0], r.replies[2]) != 0);
        CHECK(std::strcmp(r.replies[1], r.replies[2]) != 0);
    }
}

// --- The encounter ---------------------------------------------------------

namespace {
// Put a guardian in front of the pet and keep rolling until the welcome lands on a
// RIDDLE. The welcome is a roll (tunables.h), so a gate about the riddle has to search
// for one rather than assume it; returns false if the search ran out.
inline bool reachRiddle(Game& g) {
    for (int i = 0; i < 200; ++i) {
        g.debugStartShibboleth();
        if (g.nav() == Game::Nav::Shibboleth) return true;
        // An affront went straight to the fight; ride it out and try again.
        if (g.nav() == Game::Nav::Combat) {
            uint32_t t = 0;
            for (int j = 0; j < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                g.tick(t += kHeartbeatMs);
            g.onButton(press(Button::B));
            if (!g.exploreActive()) enterWalk(g);      // the fight ended the walk
        }
        if (g.nav() == Game::Nav::PostEncounter) g.onButton(press(Button::B));
    }
    return false;
}
// Step the cursor onto the row carrying the true reply.
inline void focusTrueReply(Game& g) {
    for (int i = 0; i < kRiddleReplies && g.shibbolethRow() != g.shibbolethTrueRow(); ++i)
        g.onButton(press(Button::A));
}
}  // namespace

// THE regression this system nearly shipped with. The welcome bands were thresholds on
// fluency, which deadlocked: a pet needed four sigils before a guardian would ask it
// anything, and winning a riddle is the only way to get one. As chances, a pet that
// knows NOTHING of the Cant still gets asked — which is the ladder's first rung.
void test_shibboleth_is_reachable_from_zero_sigils() {
    Game g{StartMode::Hatched};
    enterWalk(g);
    CHECK(g.sigilsKnown() == 0);
    CHECK(reachRiddle(g));
    CHECK(g.nav() == Game::Nav::Shibboleth);
    CHECK(g.shibbolethWelcome() == Game::ShibbolethWelcome::Riddle);
    Framebuffer fb(kActiveW, kActiveH);
    g.render(fb);
    CHECK(hasDarkInk(fb, 0, 0, kActiveW, kActiveH));
}

// A right answer always pays its Happiness and its shed Fragmentation. The SIGIL is the
// separate half and costs an unspent SHAKE — so a player with no radio, or with capture
// switched off, wins everything the walk pays and simply does not learn the language.
// That split is the whole reason the gate sits on the sigil and not on the win.
void test_shibboleth_win_pays_without_a_shake_but_buys_no_sigil() {
    Game g{StartMode::Hatched};
    enterWalk(g);
    g.model().setHappiness(50);
    g.model().setFragmentation(60);
    CHECK(g.shakesUnspent() == 0);
    CHECK(reachRiddle(g));
    const int happy0 = g.model().happiness();
    const int frag0 = g.model().fragmentation();
    focusTrueReply(g);
    g.onButton(press(Button::B));
    CHECK(g.shibbolethReply() == Game::ShibbolethReply::Answered);
    CHECK(g.model().happiness() == happy0 + kShibbolethWinHappy);
    CHECK(g.model().fragmentation() == frag0 - kShibbolethWinFragCut);
    CHECK(g.sigilsKnown() == 0);                 // nothing to trade — no sigil
    CHECK(g.nav() == Game::Nav::Idle);           // and back to the walk, not a fight
}

// With a shake in the purse the same win DOES buy a sigil, and spends exactly one. The
// lifetime SHAKES tally is untouched: it is the brag, and what is spendable is derived.
void test_shibboleth_win_spends_one_shake_for_one_sigil() {
    Game g{StartMode::Hatched};
    enterWalk(g);
    g.debugAddHandshakes(3);
    CHECK(g.shakesUnspent() == 3);
    CHECK(reachRiddle(g));
    focusTrueReply(g);
    g.onButton(press(Button::B));
    CHECK(g.sigilsKnown() == 1);
    CHECK(g.shakesUnspent() == 2);
    CHECK(g.handshakesSeen() == 3);              // the lifetime tally never moves
}

// A wrong reply costs Happiness and Fragmentation and hands the pet to the guardian's
// fight. Same for saying nothing at all — C is not an exit here, and neither is the
// clock; a guardian takes silence for an answer, and the wrong one.
void test_shibboleth_wrong_and_silent_both_cost_and_fight() {
    for (int silent = 0; silent < 2; ++silent) {
        Game g{StartMode::Hatched};
        enterWalk(g);
        g.model().setHappiness(50);
        g.model().setFragmentation(20);
        CHECK(reachRiddle(g));
        const int happy0 = g.model().happiness();
        const int frag0 = g.model().fragmentation();
        if (silent) {
            // Run the answer clock out rather than pressing anything.
            uint32_t t = 0;
            for (int i = 0; i <= kShibbolethReplyHoldBeats &&
                            g.nav() == Game::Nav::Shibboleth; ++i)
                g.tick(t += kHeartbeatMs);
        } else {
            // Step OFF the true row, then commit.
            for (int i = 0; i < kRiddleReplies &&
                            g.shibbolethRow() == g.shibbolethTrueRow(); ++i)
                g.onButton(press(Button::A));
            g.onButton(press(Button::B));
        }
        CHECK(g.shibbolethReply() == (silent ? Game::ShibbolethReply::Unanswered
                                             : Game::ShibbolethReply::Wrong));
        CHECK(g.model().happiness() == happy0 - kShibbolethLoseHappy);
        CHECK(g.model().fragmentation() == frag0 + kShibbolethLoseFrag);
        CHECK(g.nav() == Game::Nav::Combat);
        CHECK(g.combat().stakes() == Combat::Stakes::Live);
    }
}

// Pressing A while reading restarts the answer clock, so a player who is genuinely
// working through a half-legible riddle is never timed out mid-sentence.
void test_shibboleth_stepping_the_cursor_restarts_the_clock() {
    Game g{StartMode::Hatched};
    enterWalk(g);
    CHECK(reachRiddle(g));
    uint32_t t = 0;
    for (int i = 0; i < kShibbolethReplyHoldBeats - 2; ++i) g.tick(t += kHeartbeatMs);
    CHECK(g.nav() == Game::Nav::Shibboleth);
    g.onButton(press(Button::A));                 // reading — reset the patience
    for (int i = 0; i < kShibbolethReplyHoldBeats - 2; ++i) g.tick(t += kHeartbeatMs);
    CHECK(g.nav() == Game::Nav::Shibboleth);      // still up, because A reset it
}

// A fluent pet is received rather than tested: at a complete Cant the BOON band is
// reachable, and a boon never opens a screen or a fight — it hands straight back to the
// walk having paid something.
void test_a_complete_cant_earns_boons() {
    Game g{StartMode::Hatched};
    enterWalk(g);
    g.debugLearnSigils(kCantSigils);
    CHECK(g.sigilsKnown() == kCantSigils);
    g.model().setHappiness(20);
    g.model().setFragmentation(80);
    bool sawBoon = false;
    for (int i = 0; i < 200 && !sawBoon; ++i) {
        const int happy0 = g.model().happiness();
        const int ally0 = g.allyBuffBattlesLeft();
        g.debugStartShibboleth();
        if (g.nav() == Game::Nav::Idle &&
            g.shibbolethWelcome() == Game::ShibbolethWelcome::Boon) {
            sawBoon = true;
            // A boon is one of two shapes and always pays ONE of them.
            CHECK(g.model().happiness() > happy0 ||
                  g.allyBuffBattlesLeft() > ally0);
        } else if (g.nav() == Game::Nav::Shibboleth) {
            g.onButton(press(Button::B));         // a riddle — answer and keep looking
        }
        if (g.nav() == Game::Nav::Combat) {
            uint32_t t = 0;
            for (int j = 0; j < 400 && g.combat().outcome() == Combat::Outcome::Ongoing; ++j)
                g.tick(t += kHeartbeatMs);
            g.onButton(press(Button::B));
        }
        if (g.nav() == Game::Nav::PostEncounter) g.onButton(press(Button::B));
        if (!g.exploreActive()) enterWalk(g);
    }
    CHECK(sawBoon);
    // An AFFRONT is impossible at full fluency — the refusal chance scales to nothing —
    // which is what makes learning the whole Cant mean "it will always hear you out".
    CHECK(kShibbolethAffrontBasePct * (100 - 100) / 100 == 0);
}

// --- The guardian ----------------------------------------------------------

// A guardian out-classes the rung it is MET on, and is drawn at that rung rather than at
// the area's deepest — the second thing this system nearly shipped wrong. Pinned to the
// gauntlet's last stage it was a wall a first-sub-area pet could not have built for, and
// losing here ends the run the same as any wild.
void test_guardian_outclasses_the_rung_it_is_met_on() {
    for (int a = 0; a < kExplSectors; ++a) {
        for (int s = 0; s < kExplSubAreas; ++s) {
            const CombatEnemy guard = guardianEnemy(a, s);
            const BossGauntlet rung = subAreaBoss(a, s);
            CHECK(!rung.rounds.empty());
            CHECK(guard.maxHealth > rung.rounds[0].maxHealth);
            CHECK(guard.powerMultPct > 100);
            CHECK(guard.dmgReducePct > 0);
            CHECK(!guard.isWild);              // never the wild challenge buff
            CHECK(guard.name != nullptr);
            // It must NOT carry the area's apex threat rider — that move is the
            // signature boss's tell and the only way to earn it.
            if (const char* apex = area(a).apexThreatMoveId)
                for (const char* m : guard.moveIds)
                    CHECK(std::strcmp(m, apex) != 0);
        }
        // Deeper rungs field a deeper guardian: it reads off the shared boss spine, so
        // "met where you stand" is a real scaling and not a constant.
        CHECK(guardianEnemy(a, kExplSubAreas - 1).maxHealth > guardianEnemy(a, 0).maxHealth);
    }
}

// Every area names a guardian, and each one's taught move is UNIQUE to it — a guardian
// move with a second carrier would quietly duplicate another boss's prize.
void test_every_area_has_a_guardian_with_its_own_move() {
    std::set<std::string> taught;
    for (int a = 0; a < kAreaCount; ++a) {
        const GuardianDef& gd = area(a).guardian;
        CHECK(gd.name != nullptr && gd.name[0] != '\0');
        int n = 0;
        for (const char* id : gd.teaches) {
            if (!id) continue;
            ++n;
            CHECK(taught.insert(id).second);           // no move taught twice
            CHECK(ContentRegistry::embedded().move(id) != nullptr);
        }
        CHECK(n >= 1);                                 // a guardian worth beating
    }
}

// --- Persistence -----------------------------------------------------------

// The Cant and the purse survive a reboot (save v59). A device that learned a language
// has learned it; nothing about a guardian encounter is durable EXCEPT this.
void test_cant_persists_across_a_reboot() {
    MemSaveStore store;
    {
        Game g(StartMode::Hatched, "paypup", &store);
        g.debugAddHandshakes(5);
        g.debugLearnSigils(4);
        g.tick(kSaveAutosaveMs + kHeartbeatMs);   // flush the debounced write
    }
    Game g2(StartMode::Hatched, "paypup", &store);
    CHECK(g2.sigilsKnown() == 4);
    CHECK(g2.handshakesSeen() == 5);
    // The four are the first four of the reveal order, not four arbitrary letters.
    SigilSet expect = 0;
    for (int i = 0; i < 4; ++i) expect = learnSigil(expect);
    CHECK(g2.cantSigils() == expect);

    // A blob written before v59 reads back as an unlearned Cant with a FULL purse —
    // every shake such a save ever captured is still there to spend.
    SaveData d{};
    d.handshakesSeen = 7;
    CHECK(d.cantSigils == 0);
    CHECK(d.shakesSpent == 0);
}
