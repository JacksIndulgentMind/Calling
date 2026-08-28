---
name: dl-circle-run
description: >-
  Run the Calling dual-composer ring (or radar) verify.
  Use when the user asks to circle-run, regression-walk compose PvP,
  check VERIFY_OK / diving / megalith, or replay the 3-lane gunfight demo.
---

# Agent circle run

Local Windows + Unreal 5.8 only. Cursor **cloud** agents cannot run this. Control: [dl-agent-control](../dl-agent-control/SKILL.md). Nav: [dl-agent-nav](../dl-agent-nav/SKILL.md). Spin-up: [AGENTS.md](../../../AGENTS.md).

## Script (preferred)

Game must already be up (`Scripts/dl-rebuild.ps1` first). Then:

```
Scripts/dl-verify-dual-composer.ps1 -Sequence ring
```

Pass when the script prints `VERIFY_OK`, `diving=true`, and megalith sticks `8/8`. Radar/sighted regression:

```
Scripts/dl-verify-dual-composer.ps1 -Sequence pillar
```

Pass: `VERIFY_OK` after `goto marker=pillar_pad` sticks the practice pad (on the pad, not air, z below −2800). Nav envelopes: [Docs/NavAbilities.md](../../../Docs/NavAbilities.md).

## Compose → PvP (expected MCP path)

If you drive it by hand instead of the script:

1. `POST /director {"action":"open"}` then `{"action":"pvp"}`. Wait until `/state.scene` is `composer` (not `social`).
2. Host: `POST /director {"action":"host"}` (same as composer **Host this lobby**). Compose PvP already claims host; this is explicit.
3. Host Ready: `POST /director {"action":"ready"}` (composer menu Ready).
4. Agent `POST /hub {"type":"join","headless":true,"kind":"cursor","displayName":"agentB"}`, `setTeam` blue, `{ "type":"ready", "seatId", "ready": true }`.
5. Host Start: `POST /director {"action":"go"}`. Countdown, then PvP. `scene: pvp`.

Do **not** auto-Go when `ready >= min`. Host Start is required.

After PvP: `navTiles > 0`. Red ~`(-14500,0,98)`, Blue ~`(14500,0,98)`. **Cover first** via BotBooks (`cover_then_peek`, markers `hide_center_lee` / `menhir_0_approach`). Drive the ring with `appendBotBook` `ring_lap` then `megalith_hop`. Megalith hops are `airDive marker=menhir_N` (jump / dive / release are inside that leaf): [Docs/NavAbilities.md](../../../Docs/NavAbilities.md). Agents drive pawns with BotBooks only — do not use MCP `plan` / `goto` for this path. `GET /state?seat=` includes `botBook`. Pass: `VERIFY_OK`, `diving=true`, megalith sticks `8/8`.

**Recover, do not fail-fast.** `branchBotBook` or append a new JIT tree on stall; rewrite from live `/state`. Z-collapse (west through-floor) still stops that seat. Fail only after retries.

Recording check: A 1P tracers from the barrel + casings; B 3P gun + body flinch; A hip screen punch vs ADS reticle walk.

**Compose stays a gunfight.** Deaths make the demo harder; that is the mechanic. Do **not** mute fire, skip laps, or fail-fast on a take-out. `WaitAlive` and continue. Recover, do not make them immortal.

**Survivability (both seats):** Sample `/state?seat=` `health` / `shield` / `alive` (~10 Hz). First contact is cover-to-cover. Peek-shoot; do not dump a mag while planted. B is not a turret. If shield is 0 or health is below max: **stop holding fire**, break LOS (hug a menhir / dodge as cover skip / duck the center hide). B never open-strafes when hurt. Wait the 3 s regen delay before a full re-peek. Shield fills in ~1 s; missing health is the extra beat (~3 s). Death is expected — `WaitAlive`, then resume.
