# `src/core/content/areas/` — one folder per EXPL area

An area is one `AreaDef` (`area_defs.h`) defined by one `<area_id>/area.cpp`: its name, tier and
Title, its 5 sub-areas and their bosses, its apex threat move, its guardian, its shop, and its
mod pool. The EXPL
list, boss composition and explore-loot rolls all read areas through `mal::area(idx)` — none of them
owns this data.

| Before you… | Read |
|---|---|
| Add an area, or change one's bosses/shop/mod pool | [AREA_CONTENT_STANDARD.md](AREA_CONTENT_STANDARD.md) — the folder shape, what stays cross-cutting, why the DeepWeb Dive isn't an `AreaDef` |
| Change an area's GUARDIAN — who watches it, what it says, what beating one teaches | its `area.cpp`'s `guardian` row (`GuardianDef`). A guardian is met on the WALK, not the ladder, so it has no sub-area and no clear flag; its `GuardianLine` pairs are its voice — `lines` for how it meets a pet, `outcomes` for what it makes of the answer. See *The guardian* in [AREA_CONTENT_STANDARD.md](AREA_CONTENT_STANDARD.md) |
| Name an area or a sub-area | [AREA_NAMING.md](AREA_NAMING.md) — two tiers, different rules: an Area is a traceable pun on one real brand; a sub-area is a generic behaviour + a landscape word from that area's terrain family |

Adding an area is a folder plus one `kAreaList[]` entry — `kAreaCount`, `kExplSectors` and every
save-flag array follow automatically. There is no second count to bump.
