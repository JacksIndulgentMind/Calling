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
Scripts/dl-verify-dual-composer.ps1 -Sequence radar
```

## Compose → PvP (expected MCP path)

If you drive it by hand instead of the script:

1. `POST /director {"action":"open"}` then `{"action":"pvp"}`. Wait until `/state.scene` is `composer` (not `social`).
2. Host: `POST /director {"action":"host"}` (same as composer **Host this lobby**). Compose PvP already claims host; this is explicit.
3. Host Ready: `POST /director {"action":"ready"}` (composer menu Ready).
4. Agent `POST /hub {"type":"join","headless":true,"kind":"cursor","displayName":"agentB"}`, `setTeam` blue, `{ "type":"ready", "seatId", "ready": true }`.
5. Host Start: `POST /director {"action":"go"}`. Countdown, then PvP. `scene: pvp`.

Do **not** auto-Go when `ready >= min`. Host Start is required.

After PvP: `navTiles > 0`. Red ~`(-14500,0,98)`, Blue ~`(14500,0,98)`. **Cover first:** do not rendezvous in the open pit. A behind a trilithon (~Polar 0° / 1250 cm). **B peeks from the lee of the center hide** (~`(0,400,-2000)`, north of the hide slab — not on A’s y=0 chord). Seat **B first**, then A; parallel `goto` through the pit shoves B off path. Return-fire is a glance then ADS (~0.55 s) with `lookAtSeat` A (not `FireUntilHit`, not a turret). Hurt (`shield` 0 or health missing): **no fire**, `goto` hide, crouch ~1.2 s — cover + regen, not a strafe in the pit. **A laps outside the menhirs** (~1200–1300 cm) with `lookAtSeat` B (no `yawAbs` snaps), **strafe-sliding** the full `SlideDuration` and holding `fire` when not hugging a post (commit only if predicted chord stays in-band; do not truncate). Heading slews at 420/280 deg/s; if B moves, ~100 ms then catch-up. Hub `{ "type":"view", "seatId" }` locks the window: lap 1 is A (1P + eased 3P peek on dodge/dash/dive), lap 2 is B **true 1P** (boom arm 0, body OwnerNoSee — not a stuck 3P boom). Then each back to **their** spawn. `GET /state?seat=` per seat. `/state` also reports `slideDuration`, `slideDistanceCm`, `dashDuration`, `dodgeDuration`.

**Recover, do not fail-fast.** Retry `goto` on stall or `partial`; rewrite a short `plan` from live `/state`. Do not chain six independent ring gotos. Z-collapse (west through-floor) still stops that seat. Fail only after retries.

Recording check: A 1P tracers from the barrel + casings; B 3P gun + body flinch; A hip screen punch vs ADS reticle walk.

**Compose stays a gunfight.** Deaths make the demo harder; that is the mechanic. Do **not** mute fire, skip laps, or fail-fast on a take-out. `WaitAlive` and continue. Recover, do not make them immortal.

**Survivability (both seats):** Sample `/state?seat=` `health` / `shield` / `alive` (~10 Hz). First contact is cover-to-cover. Peek-shoot; do not dump a mag while planted. B is not a turret. If shield is 0 or health is below max: **stop holding fire**, break LOS (hug a menhir / dodge as cover skip / duck the center hide). B never open-strafes when hurt. Wait the 3 s regen delay before a full re-peek. Shield fills in ~1 s; missing health is the extra beat (~3 s). Death is expected — `WaitAlive`, then resume.
