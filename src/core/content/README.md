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

**Renaming an id is free** — rewrite the row and add a rename row to the save codec
(`core/model/save.h`'s `renamedIds`). Never leave an alias behind in a content table.

After a struct-shape or row change, `make pedia && make pedia-check` — the web 'Pedia reads these
tables through the firmware's own code, so its data file needs regenerating and committing.
