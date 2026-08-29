---
name: dl-agent-nav
description: >-
  Navigate Calling maps via BotBooks (appendBotBook) and pawn probes.
  Use when the user asks to stroll, walk a map, test movement, or
  regression-walk the 3-lane after greybox changes. Improve when a drive fails.
---

# How to nav (any map)

Anytime an agent drives a pawn, hub `appendBotBook` on a **seat** (catalog name or JIT PlantUML — see [Docs/BotBooks.md](../../../Docs/BotBooks.md) and [dl-agent-control](../dl-agent-control/SKILL.md)). Do **not** memorize wall coordinates or gap sides for a layout. Sense, then move. Durable books `goto` by **marker**; JIT may use x,y,z.

Prefer Recast `goto` (as a BotBook leaf) when `navTiles > 0` on **that seat’s** `/state`. Confirm the book settles Success/GoodEnough.

1. Prefer `appendBotBook` with `goto marker=...` (or JIT xyz) when `navTiles > 0`. Recast: **drop-down**, **jump-down**, **cover-over**, **jump-up** (full RocketPulse triple), **jump-over** (**high cost** — walk around when you can). If the leaf fails `no_path` / `no_project_*`, rewrite the book from `/state`.
2. Face the goal (`setFocus` / `trackFocus` or JIT look), pitch 0, sprint forward.
3. **Stop short of walls:** if `fwdKind` is `wall`, `fwdDist` < **400**, and `headDist` is also short, do not kiss the face.
4. **`fwdKind`:** `walk` = connected rising floor. `drop` = lower platform you can walk off onto. `jumpDown` / `jumpUp` / `cover` / `wall` as in probe.
5. **Stay on the floor you are on.** Void is the drop. If Z drops well below the standing surface, stop that seat’s book.
6. Never dodge unless the book names `dodge`. Do not leave a strafe latched — the book unpresses when the leaf settles.
7. Sample `/state?seat=` at most ~10 Hz. Log a short x/y/z trail **per seat**. After a failed stroll, add a **general** lesson to [lessons.md](lessons.md).
8. **Recover instead of throw:** `branchBotBook` or append a new JIT tree if stalled; rewrite from `/state`. Z-collapse still stops that seat.

Do **not** use MCP `plan` / `sequence` / `intent` / raw `goto` when a seat exists. Those are loopback debug.

## Landing (*-to leaves)

`goto` is Recast (walk, drop, jump-up to full triple apex, **AirDiveDown/Over** off-mesh). It follows Recast when the path **reaches** the marker. Lintel climbs stay catalog/JIT **`airDive marker=`**. Catalog `edge_pad` is Recast `goto` only. Jump-gen JumpLength is a look radius from the edge (Epic); search limited by NavMeshBoundsVolume. Read [Docs/NavAbilities.md](../../../Docs/NavAbilities.md).

Air dive is how you **get to a place on the ground**, not a stylish jump. `airDive marker=` jumps as high as DistXY needs, dives when the hang+slam box covers the point, then **releases** at `success distXY − coast`. Every tick: still in the envelope? If not, Fail, zero stick, fall. Bare `:airDive;` with `success: diving` is a pulse, not a landing.

`goto` may compose jumps internally. You can still run jump-to / airDive-to / slide-to as their own book steps.

Dead-reckon along the travel axis: sprint `v ≈ 714` cm/s (`420 * 1.70`), accel ~2048 so ~0.35 s to full sprint. Uphill is slower — measure cruise from the first second of free movement on this stroll, do not assume 714.

## Look (required on every stroll)

Pawn spawn often leaves pitch at **−12** (looking at the floor). That is not a demo hold.

1. **Yaw toward the objective**, not whatever heading agent-nav inherited.
2. **Pitch 0** — same-height headshot hold. Do not look down the slope or at the destination’s Z.
3. Put `trackFocus` / `setFocus` on movement leaves, or JIT look in a BotBook note. Hub `goto` slews toward the path at the same rates (no snap on Start) and holds pitch 0.
4. Confirm `/state?seat=` yaw/pitch after the first sample. Heading eases in; a large turn takes up to ~0.4 s (180° at 420 deg/s). If yaw never moves, the look never applied — stop and fix.

## 3-lane PvP (locked dimensions, not a route)

cm. Red west / Blue east. Spawn pads Z = 0 (pawn ~98). Courtyard Z = **−2000**. Stonehenge π trilithons ~9.5 m radius (8 stations, long axis tangential). Mid strip is 12 m wide (void beyond). Crossing cover is ~8 m across the lane with a gap on one side. Map bug: west ramps can drop you **through** the floor near mid (~x −85 to −122 m); stop if Z collapses, do not treat that as a nav lesson.

## Verb tests (Social pad is fine)

WASD, strafe, crouch (`crouchAlpha→1`), sprint (farther than walk), jump (Z peak ~+210 from stand, `MaxJumps=3`; mash Space mid-climb — air pulses **add**, no apex wait), dodge/dash flags (wait until the flag clears — do not truncate), `"slide": true` pulse (`/state.sliding` for full `slideDuration`; commit from `slideDistanceCm`, do not `goto` mid-slide), air-dive **to a point** as in [NavAbilities.md](../../../Docs/NavAbilities.md) (`airDive marker=` — jump, dive, release; do not script those as extra verbs). Combat locomotion is a digital dance: land-from-dive into slide the same beat; slide cancel into dash; ADS on/off mid-slide; sprint+strafe slide follows move dir. Over-commit verticality is recovered by air dive, not gravity. Default AirDive bind is **Z** (not Alt; Alt is modifier-only).

## Improve over time

This skill is the planner. Lessons are for **methods** (traces, lookAbs, stall vs hitch, PowerShell JSON, per-seat state). They are not a cookbook for one corridor.
