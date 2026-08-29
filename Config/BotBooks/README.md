# BotBooks

Catalog PlantUML for pawn bots. Language and agent contract: [Docs/BotBooks.md](../../Docs/BotBooks.md). Jump / air-dive / slide landing: [Docs/NavAbilities.md](../../Docs/NavAbilities.md).

Durable files **must** `goto` by marker id. JIT hub trees may use x,y,z.

| Book | What it does |
|------|----------------|
| `slide_court` / `dash_court` | Court-floor *-to leaves. `goto` does not absorb slide-to. |
| `jump_lintel` | Approach `menhir_0_approach`, then `airDive marker=menhir_0`. |
| `megalith_hop` | Eight `airDive marker=menhir_N` sticks. |
| `hold_lee` | `goto hide_center_lee` then wait (no fire). Seat B parks through edge/megalith. |
| `court_gunfight` | `goto court_center` then slide/dash/approach with `trackFocus, fire`. |
| `track_fire` | `setFocus` + fire while tracking (ring gunfight, not 0.7s peek). |
| `edge_pad` | `goto marker=edge_pad` only. Recast AirDive off-mesh + DropDown terrace links; no JIT/catalog airDive fallback. Island stand recalls to the lip. |
| `pillar_dive` | `goto marker=pillar_pad` (practice void gap). |
| `ring_lap` | Ring walk + bare `:airDive;` pulse (not a landing). |
| `cover_then_peek` / `peek_fire` / patrols | Cover demo; no air-dive. |
