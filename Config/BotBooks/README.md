# BotBooks

Catalog PlantUML for pawn bots. Language and agent contract: [Docs/BotBooks.md](../../Docs/BotBooks.md). Jump / air-dive / slide landing: [Docs/NavAbilities.md](../../Docs/NavAbilities.md).

Durable files **must** `goto` by marker id. JIT hub trees may use x,y,z.

| Book | What it does |
|------|----------------|
| `slide_court` / `dash_court` | Court-floor *-to leaves. `goto` does not absorb slide-to. |
| `jump_lintel` | Approach `menhir_0_approach`, then `airDive marker=menhir_0`. |
| `megalith_hop` | Eight `airDive marker=menhir_N` sticks. |
| `edge_pad` | `goto marker=edge_pad` only. Recast AirDive off-mesh + DropDown terrace links; no JIT/catalog airDive fallback. Island stand recalls to the lip. |
| `pillar_dive` | `goto marker=pillar_pad` (practice void gap). |
| `ring_lap` | Ring walk + bare `:airDive;` pulse (not a landing). |
| `cover_then_peek` / `peek_fire` / patrols | Cover demo; no air-dive. |
