# `src/core/content/` — the content tables

One content type per file (`content_<type>.cpp`), declared in `content_tables.h`, assembled by
`embedded_content.cpp`. A thing's effects are a structured `kind + magnitude` list on its row, and
a magnitude only one entity reads lives **on that row**, never in `tunables.h`.

| Before you… | Read |
|---|---|
| Add or edit any row, or add a new content type | [CONTENT_STANDARD.md](CONTENT_STANDARD.md) — effect vocabulary · magnitudes-on-the-row · one-file-per-type · why ids stay readable words |
| Name a creature | [CREATURE_NAMING.md](CREATURE_NAMING.md) — the phonetic-integration standard; run every candidate through its four-point checklist |
| Name an item | [ITEM_NAMING.md](ITEM_NAMING.md) — the functional-clarity standard |
| Give a line its combat identity, or add a move to one | [LINE_MOVE_IDENTITIES.md](LINE_MOVE_IDENTITIES.md) — what each family's kit is *for* |
| Add or edit an EXPL area | [areas/README.md](areas/README.md) — the folder shape and the naming tiers |
| Add a wild malbeast, or change what one fights with | `wildMalbeast` in `../model/combat_factory.cpp` — the roster is a body, a tier, and the creature's own signature move; the signature's ROW is here, under "Wild SIGNATURES" in [content_moves.cpp](content_moves.cpp) |
| Add an entrant handle to ROCK THE DOCK, or retune the arena | [content_tournament.h](content_tournament.h) — the bracket's shape, the level band and stage rungs an entrant is rolled at, the Exploit triggers, and the purse. `test_tourney_handles_fit_an_operator_tag` is the gate on the handle pool. |
| Add or retune an OWNABLE background, or change what earns one | [content_backgrounds.h](content_backgrounds.h) — the picker's rows, their display names, and the one line each says it is earned by. Ownership is NOT stored: it derives from the raised species, the cleared areas, the bracket tally and the achievement bitset, so a row states a rule rather than setting a flag. An `Achieve` row names an achievement id and nothing else — the threshold stays on that achievement's own row. `wire` is the save's name for a row and is never reused. |
| Say where a creature is at HOME — which engine-drawn backdrop stands behind it | [content_homes.h](content_homes.h) — a line with a place of its own overrides how its creatures move, and how they move is the floor. Not a field on `CreatureDef`: thirty-five rows would each be restating their line's answer. `test_every_creature_has_a_home` is the gate. |
| Add a DECRYPTOGRAM quote | [content_quotes.h](content_quotes.h) — the two rules a row must pass (it WRAPS into the grid; ASCII only), the wire-number discipline, and where a per-quote prize would go. `test_cryptogram_quotes_fit_the_panel` is the gate: write the row, run it. |
| Add a SHIBBOLETH riddle — what a guardian can ask | [content_riddles.h](content_riddles.h) — the same two rules (it WRAPS into the panel; ASCII only), plus: the FIRST reply is the true one and the three shown are shuffled, so authoring position carries no information. No wire number, because the save keeps SIGILS and nothing per-riddle. `test_riddle_pool_fits_the_panel` is the gate. |
| Change what a guardian IS, or what beating one teaches | [areas/README.md](areas/README.md) — a guardian is one `GuardianDef` row on its area, carrying only its banner and its own move. How far it out-classes the area is the same step everywhere (`kGuardian*`, `tunables.h`), per rule 2 below. |

**Renaming an id is free** — rewrite the row and add a rename row to the save codec
(`core/model/save.h`'s `renamedIds`). Never leave an alias behind in a content table.

After a struct-shape or row change, `make pedia && make pedia-check` — the web 'Pedia reads these
tables through the firmware's own code, so its data file needs regenerating and committing.
