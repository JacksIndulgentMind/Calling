---
name: dl-agent-nav
description: >-
  Navigate Calling maps via hub goto/plan and pawn probes.
  Use when the user asks to stroll, walk a map, test movement, or
  regression-walk the 3-lane after greybox changes. Improve when a drive fails.
---

# How to nav (any map)

Prefer hub `goto` / `plan` on a **seat** (see [dl-agent-control](../dl-agent-control/SKILL.md)). Do **not** memorize wall coordinates or gap sides for a layout. Sense, then move.

Prefer hub `goto` when `navTiles > 0` on **that seat’s** `/state`. Confirm `partial: false` at the destination.

1. Prefer hub `/goto` when `navTiles > 0`. Recast: **drop-down**, **jump-down**, **cover-over**, **jump-up** (~4 m), **jump-over** (**high cost** — walk around when you can). If `goto` returns `no_path` / `no_project_*`, fall back to hub `plan`.
2. Face the goal (`yawAbs` or `lookAtSeat`), pitch 0, sprint forward.
3. **Stop short of walls:** if `fwdKind` is `wall`, `fwdDist` < **400**, and `headDist` is also short, zero forward *before* you kiss the face.
4. **`fwdKind`:** `walk` = connected rising floor. `drop` = lower platform you can walk off onto. `jumpDown` / `jumpUp` / `cover` / `wall` as in probe. Zero forward only on `wall` void.
5. **Stay on the floor you are on.** Void is the drop. If Z drops well below the standing surface, stop that seat’s plan.
6. Never dodge. Never leave a strafe latched — use hub `plan` so the game unpresses.
7. Sample `/state?seat=` at most ~10 Hz. Log a short x/y/z trail **per seat**. After a failed stroll, add a **general** lesson to [lessons.md](lessons.md).
8. **Recover instead of throw:** retry `goto` if stalled/`partial`; rewrite the next `plan` from `/state`. Z-collapse still stops that seat.

Dead-reckon along the travel axis: sprint `v ≈ 714` cm/s (`420 * 1.70`), accel ~2048 so ~0.35 s to full sprint. Uphill is slower — measure cruise from the first second of free movement on this stroll, do not assume 714.

## Look (required on every stroll)

Pawn spawn often leaves pitch at **−12** (looking at the floor). That is not a demo hold.

1. **Yaw toward the objective**, not whatever heading agent-nav inherited.
2. **Pitch 0** — same-height headshot hold. Do not look down the slope or at the destination’s Z.
3. Put `look.yawAbs` + `look.pitchAbs: 0` on the **first movement step**, or `lookAtSeat` when tracking another pawn. Hub `goto` slews toward the path at the same rates (no snap on Start) and holds pitch 0.
4. Confirm `/state?seat=` yaw/pitch after the first sample. Heading eases in; a large turn takes up to ~0.4 s (180° at 420 deg/s). If yaw never moves, the look never applied — stop and fix.

## 3-lane PvP (locked dimensions, not a route)

cm. Red west / Blue east. Spawn pads Z = 0 (pawn ~98). Courtyard Z = **−2000**. Stonehenge π trilithons ~9.5 m radius (8 stations, long axis tangential). Mid strip is 12 m wide (void beyond). Crossing cover is ~8 m across the lane with a gap on one side. Map bug: west ramps can drop you **through** the floor near mid (~x −85 to −122 m); stop if Z collapses, do not treat that as a nav lesson.

## Verb tests (Social pad is fine)

WASD, strafe, crouch (`crouchAlpha→1`), sprint (farther than walk), jump (Z peak ~+210 from stand, `MaxJumps=3`; mash Space mid-climb — air pulses **add**, no apex wait), dodge/dash flags (wait until the flag clears — do not truncate), `"slide": true` pulse (`/state.sliding` for full `slideDuration`; commit from `slideDistanceCm`, do not `goto` mid-slide), `"airDive": true` while `air` (`/state.diving` then land; stick **carves** the dive; **release** kills XY so you drop straight down). Lintel stick: jump **up** outside the stone until `z > -1580`, dive in, release at DistXY **~140** (not over the center). Mute B for that window only. Combat locomotion is a digital dance: land-from-dive into slide the same beat; slide cancel into dash; ADS on/off mid-slide; sprint+strafe slide follows move dir. Over-commit verticality is recovered by air dive, not gravity. Default AirDive bind is **Z** (not Alt; Alt is modifier-only).

## Improve over time

This skill is the planner. Lessons are for **methods** (traces, lookAbs, stall vs hitch, PowerShell JSON, per-seat state). They are not a cookbook for one corridor.
