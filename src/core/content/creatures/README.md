# `src/core/content/creatures/` — one folder per creature FAMILY

A family is one evolution line — every row in it carries the same `CreatureDef::line`, which is
what gates the line's own moves and passives (`moveAllowedForLine`). Each family is defined by one
`<line_id>/line.h` and named once in [`creature_lines.h`](creature_lines.h); the registry reads
creatures family-by-family, so `ContentRegistry::creature(id)`, `allCreatures()` and the 'Pedia
dump all follow from that one list.

| Before you… | Read |
|---|---|
| Add a family, or move a chain between families | [CREATURE_CONTENT_STANDARD.md](CREATURE_CONTENT_STANDARD.md) — the folder shape, why the roster has no flat form, what a sheet row owes a clip |
| Add or retune a row's animation | [CREATURE_CONTENT_STANDARD.md](CREATURE_CONTENT_STANDARD.md) § *Clips* — `CreatureDef::clips`, and what a creature that declares none falls back to |
| Give a family its move identity | [`../LINE_MOVE_IDENTITIES.md`](../LINE_MOVE_IDENTITIES.md) |
| Draw a creature's sheet | [`../../../../assets/CREATURE_VISUAL_RULES.md`](../../../../assets/CREATURE_VISUAL_RULES.md) |

Adding a family is a folder plus one `kCreatureLines[]` entry — `kCreatureLineCount` and
`kCreatureCount` both derive from that list, so there is no second count to bump.
