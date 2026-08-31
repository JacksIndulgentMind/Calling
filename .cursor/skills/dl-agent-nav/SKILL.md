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
7. Sample `/state?seat=` at most ~10 Hz. On the **first** sample after `appendBotBook`, throw if `botBook.followAlert`, `executionError`, or `followed` is false — that is a broken book or a pawn that is not executing, not a combat stall. Log a short x/y/z trail **per seat**. After a failed stroll, add a **general** lesson to [lessons.md](lessons.md).
8. **`cause=execution` fails the drive.** Do not use it as a silent unstick. Throw `command_not_followed` (or let hub `alert: botbook_execution` / `/state.botBook.executionError` throw). Combat recover is `situation`. A poller that branches `execution` then keeps waiting on `modeResult` hid “bot failed the book.”
9. **First followAlert / `botbook_execution` fails the PvP match** (`modeResult=fail`, `modeFailReason` is the code). Dump `/state.events` and **stop**. Do not resume the same `in_progress` instance, do not weaken `loc_still`, do not keep polling until `zero_kills`. A pass is `modeResult=winner` plus an event log (`botbook_append`, `kill`, `shrine_held`, `mode_winner`).
10. **Incoming damage and take-out have source.** `/state.lastHit` is the last applied amount of any kind (DoT, void, gun). `/state.lastShot` is the last instigated combat hit (`hitscan` / `ability`, with `killerName` + killer x/y/z) — it is **not** overwritten by status ticks. Pawn `hit` / `shot` are the same for that body. `alive=false` / `lastDeath.valid` is a take-out: dump `kind` / `source` / `killerName` and `events` `code=death`. Do not chase `loc_still` (a dead goto does not fire it). Raid/stroll: fail that sample. PvP circle-run still `WaitAlive` — see [dl-circle-run](../dl-circle-run/SKILL.md). Intermittent HP/shield drop on the raid overlook: dump `lastShot` (chamber fire) vs `lastHit` (DoT if off-pad).

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

cm. Red west / Blue east. Spawn pads Z = 0 (pawn ~98), `x ≈ ±6380`. Courtyard Z = **−600**. Stonehenge π trilithons ~9.5 m radius (8 stations, long axis tangential). Mid strip is 12 m wide (void beyond). Crossing cover is ~8 m across the lane with a gap on one side. Corner shrines: `shrine_well` / `_tree` / `_heel` / `_cairn` (tag `space.shrine`). Spawns are `spawn.player.red` / `.blue`. Mode **shrine_clash**: `/state.liveShrine`, `teamAKills` / `teamBKills`, `modeResult`. Catalog book `shrine_clash_fight`. The old west through-floor band (~x −85 to −122 m) is **off this compressed layout**. If Z still collapses on a lane, stop; do not treat it as a nav lesson.

## Raid Obelisk (`CL_Raid_01`)

Rebuild: `Scripts/dl-rebuild.ps1 -Activity raid` (parent wrapper is the same). `scene=raid`, `lobby.gameMode=obelisk_raid`. Drive with BotBooks (`goto marker=…`); catalog NPCs already run `patrol_to_live_occupy` → `occupy_flank_orbit`.

**Spawn / monitor**

| What | How to read it |
|------|----------------|
| Player start | Elevated **west overlook** (`spawn_player`, tag `space.safe`). Stand ~`(−2670, 0, 900)` — **not** chamber origin / `ch1_portal`. Drop east over the west wall into chamber 1 (walls batter out `wallSlopeDeg` 5°; Recast drop-down). |
| Safe from DoT | Off-volume DoT does not apply (and does not burn grace) inside `space.safe`. After they drop in, occupy `liveOccupy` / `ch1_ring_I` or take the DoT after grace. |
| Adds | Waves from `chN_portal` (south interior), disk `radiusCm` + empty capsule `clearRadiusCm`. They are Blue; host is Red. A bot that cannot place is `raid_spawn_unclear` / `raid_spawn_collide` — dump `/state.events` and `modeFailReason`. That **fails the stroll**. Widen the spawn volume; do not skip the add. |
| Damage | `/state.lastHit` (any applied amount) and `/state.lastShot` (last gun/ability with `killerName` + killer x/y/z). Pawn `hit` / `shot`. `kind` / `source` / `amount` / remaining `health`/`shield`. Chamber adds can tag the overlook; DoT does not clobber `lastShot`. |
| Take-out | `/state.lastDeath` (match-wide) and pawn `death`. Same fields as a hit. `events` `code=death` `detail=kind\|source\|killerName`. **Fail the stroll** on that dump — a melt looks like a stuck pawn if you only watch loc. |
| Progress | `encounterId`, `phaseId`, `wavesDone`, `liveOccupy` / `liveShrine`. `navTiles > 0` after bake. |
| Rescue | Min Z is **walkable floor − buffer**, not spawn height. Elevated spawn must not yank floor NPCs onto the overlook. Player void recall is spawn/lip for this layout (no PvP edge pad). |

Do not treat `fwdKind=cover` on the overlook (looking at the west wall) as a stall. Drop into the room, then `goto marker=ch1_ring_I` (or `live_occupy`).

## Verb tests (Social pad is fine)

WASD, strafe, crouch (`crouchAlpha→1`), sprint (farther than walk), jump (Z peak ~+210 from stand, `MaxJumps=3`; mash Space mid-climb — air pulses **add**, no apex wait), dodge/dash flags (wait until the flag clears — do not truncate), `"slide": true` pulse (`/state.sliding` for full `slideDuration`; commit from `slideDistanceCm`, do not `goto` mid-slide), air-dive **to a point** as in [NavAbilities.md](../../../Docs/NavAbilities.md) (`airDive marker=` — jump, dive, release; do not script those as extra verbs). Combat locomotion is a digital dance: land-from-dive into slide the same beat; slide cancel into dash; ADS on/off mid-slide; sprint+strafe slide follows move dir. Over-commit verticality is recovered by air dive, not gravity. Default AirDive bind is **Z** (not Alt; Alt is modifier-only).

## Improve over time

This skill is the planner. Lessons are for **methods** (traces, lookAbs, stall vs hitch, PowerShell JSON, per-seat state, take-out vs loc_still, lastHit vs lastShot). They are not a cookbook for one corridor. A frozen loc with `alive=false` / `lastDeath.valid` is a death dump, not a Recast lesson — do not weaken loc_still. HP dropping while alive: dump `lastHit` / `lastShot`, not a nav guess.
