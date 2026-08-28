# Nav abilities (jump / dive / slide / dash / strafe)

Agents drive pawns with **BotBooks**. This file is how a **leaf** lands a point in space. Recast `goto` is a different leaf: it may *compose* these. Tiles, jump-gen, and off-mesh snap: [RecastLinks.md](RecastLinks.md). Do not retune `AirControl=0.35` or `BaseStrafeSpeed=380`.

Numbers below come from `FCLMovementTune` / `DefaultCalling.ini` and Recast `Config/Nav/NavTune.json`. They are **range boxes**, not a neural net. A later brain can sit on top; the leaf still evaluates the box **every tick**.

## Two layers

| Layer | BotBook verb | What it is | What it is not |
|-------|----------------|------------|----------------|
| **Composer** | `goto marker=…` (or JIT xyz) | Recast path. May walk, drop, jump-up/down, cover-over. Stops at XY arrive (~150 cm). | Not a promise to stick a lintel or carve a dive. |
| **Single task** | `jump`, `airDive`, `slide`, `dash`, `dodge`, plus hold-stick strafe | One mechanic, C++ tick until Success / GoodEnough / Fail. | Cannot swap in a different mechanic mid-leaf. |

A book that needs a slab in the air is **`airDive marker=`**. That leaf looks up the torus cell (height slice × DistXY ring) and runs that recipe: jumps, hang, pin or release. Do not author a separate `:jump;` / `:wait;` around it. `goto` may still walk you to an approach, or delegate to the same leaf on an AirDive hull edge.

Anytime an agent needs to drive a pawn, POST a BotBook (`appendBotBook` / `branchBotBook`). See [BotBooks.md](BotBooks.md).

## Per-tick contract (every *-to leaf)

On **Start**, compute an envelope from current pose + tune (not from memorized map centimeters).

Every **Tick** (NetHz, 20):

1. **Still in envelope?** DistXY and ΔZ vs the box, plus “do I still have the resource” (jumps left, dive cooldown, on-ground vs air). If **no**: Fail (`successImpossible`). Graceful exit: zero stick, let gravity / floor below catch you. Do not keep holding W.
2. **Approach (~90%).** Use the mechanic to close DistXY / height. Jump pulses, dive stick, slide commit, etc.
3. **Release.** When DistXY ≤ `releaseXY` (success radius **minus** brake coast), zero directional input. The body still travels `coastXY` while air-brake (dive) or friction (slide) kills XY.
4. **Settle.** Success if inside the landing band (DistXY + Z + not-air as the leaf requires). GoodEnough holds `trySuccessFor`. Timeout Fails; the book advances unless a fallback is set.

`releaseXY` is **not** the landing radius. Lintel stick: land DistXY **~180**, release at **~140**, coast **~40 cm** (~0.10–0.17 s at sprint-ish XY). That coast is `v² / (2a)` with dive brake `MaxAcceleration * AirDiveXYBrake` (accel ~2048, brake mul **3.5**).

## Constraint sources (do not silently drift)

| Concern | Source | Consumer |
|---------|--------|----------|
| Walk / sprint / slide / dash / dodge / dive hang+slam / JumpZ / AirControl | `FCLMovementTune` ← `DefaultCalling.ini` `CLMovementFeelSettings` | envelopes, *-to ticks, `CLNavAbilityValidate` |
| Recast agent + short jump links + `airDiveSearchMaxCm` | `Config/Nav/NavTune.json` | baked links via `CLNavLinkPolicy` |
| **Strain** | `Config/Strain/AugmentedHumanoid.json` | Recast AirDiveDown/Over depth, Launch search XY, later fall damage. `maxFallBeforeCriticalCm` = **3000** (30 m). |
| Map rescue Z | greybox `SurvivingDropCm` | JumpDown depth; rescue yank. Stays below strain so a 30 m dive is not yanked. |
| Locks | `AirControl=0.35`, `BaseStrafeSpeed=380` | validator asserts; do not retune |

On nav rebuild, `CLNavAbilityValidate` checks locks, `jumpApexCm` ≥ single-jump apex, and that `airDiveSearchMaxCm` does not exceed `MaxLaunchXY` at the strain 30 m drop. Failures go through `UCLErrorBoundary`. Recast `JumpMaxDepth` is the strain survivable fall from the walkable edge (3000). The island sits **strain minus end-tol minus apex** below the lip. How Recast bakes AirDiveDown, simple hints, and neighbor-tile snap: [RecastLinks.md](RecastLinks.md). The validator does **not** rewrite NavTune.json.

## Search radius

Recast does not search the whole courtyard. Each link kind looks only as far as that mechanic can go.

`MaxLaunchXY(drop) = JumpSteerXY(3) + PinnedDiveXY(drop)`. Jump steer is AirControl **0.35** through the triple (a few meters). Pinned dive is hang **0.44 s** at `AirDiveMaxXY` **1200**, then the rest of the drop at **1g** with stick pinned — **not** 8G slam. At 30 m that is **~30 m+ XY**. Bake and `goto` share `min(MaxLaunchXY, airDiveSearchMaxCm)` with cap **0** meaning uncapped. Slam stays landing **feel** on short hops.

**DropDown** is a short XY chord (~2.8 m) with strain depth: pad right under the lip is a walk-off, not a Launch. **AirDiveDown** is the long-XY drop Recast searches from the **walkable edge** (JumpMaxDepth = strain **30 m**). Walk-off-then-dive can use that full 30 m. **AirDiveOver** is the long spanning gap. Both AirDive* use `UCLNavArea_AirDive`; `goto` Launchs those edges only.

The **greybox test island** is the **worst-case jump**: lowest floor you can survive from **triple-jump apex** (strain minus 300 cm end-tol ≈ **27 m** below apex ≈ **23 m** below the lip). Lateral placement is **90% of `MaxLaunchXY` at that apex drop**, inset from the rim so you are not jumping from the exact edge. Recast must connect that chord before catalog `goto marker=edge_pad` can ride it — see [RecastLinks.md](RecastLinks.md). `/state.edgePadLinked` is the bake-time path (complete, DistXY > 800, dZ < −1500).

## Shared speeds (do not retune the two locks)

| Quantity | Value | Notes |
|----------|--------|--------|
| Walk | `BaseWalkSpeed` **420** cm/s | |
| Strafe cap | `BaseStrafeSpeed` **380** | **Locked.** |
| Sprint | `420 * 1.70` ≈ **714** cm/s | Accel ~2048 → ~0.35 s to full sprint |
| Air control | **0.35** | **Locked.** Jump-to does not fly like a jet. |
| Look slew | yaw 420 / pitch 280 deg/s | Not a snap |

## Jump-to

**When:** Gain height, clear a lip, or start an airDive-to from above the landing Z. **Zero jumps** when the landing is *below* and a walk-off / drop link is enough.

**How:** `MaxJumps=3`. Ground jump sets `JumpsRemaining = 2` and `JumpZVelocity=640`. Air pulses **add** `DoubleJumpZVelocity=560` to current Vz (Rocket Pulse). No apex wait. Mash while climbing.

Single-jump apex ≈ `640² / (2×980)` ≈ **209 cm** (matches “+210 from stand”). Recast **JumpUp** budget is `jumpApexCm=400` (stacked triple, ~4 m). Use Recast’s 4 m for “can I jump-up this ledge”; use ~210 cm for a *single* hop.

| Jumps used | Typical ΔZ (up) | Typical DistXY while jumping | Notes |
|------------|-----------------|------------------------------|--------|
| 0 | ≤ 0 (down) | walk / drop | Walk-off if `fwdKind=drop`. Do not jump. |
| 1 | ~0–210 cm | short (air control 0.35) | Lip / crate. |
| 2 | mid | more XY if you push stick | Second pulse before apex stacks higher. |
| 3 | up to ~4 m (Recast) | still modest XY | Triple for JumpUp links. |

**Envelope abandon:** no jumps left and still below the lip; or DistXY so large that air control cannot close before landing. Then Fail; `goto` or a new jump-to from a better floor.

**Stick:** short DistXY (lintel, ≤ ~5 m) jumps **still** so you do not kiss the posts. Longer gaps hold forward. The leaf chooses this; the book does not.

## AirDive-to (landing a ground point)

Air dive is **how you get to a place on the ground**, not a stylish jump. The book names the point (`airDive marker=` or JIT xyz). An **envelope** is that **place**: one toroidal band around the pawn (DistXY ring at a height slice) plus the whole recipe to land there. It is not a phase list along one flight. Recast **AirDiveDown / AirDiveOver** is the **outer hull** of every ring (`MaxLaunchXY` × strain 30 m), not a chord per ring. Inner rings exist only at runtime.

Bind: **Z**. Activation (`TryAirDive`): must be **air**; not already diving; cooldown **0.8 s**. Keeps **90%** of XY (`AirDiveXYKeep`). Hang **0.44 s** at gravity **0.15**, then the recipe’s pin (1g) or release (8G slam). Per-ring air-steer is `AirDiveSteer` (outer **1.75**; inner **0**, still). XY capped at **max(sprint, 1200)**. `AirControl` stays **0.35** on normal jumps.

### Torus lookup (`FCLLaunchRecipe`)

`LookupLaunchRecipe(From, To)` picks the nearest **slice** by ΔZ and the nearest **ring** by DistXY. Empty cells (stand + outer, DistXY past the hull, climb ΔZ) are invalid → leaf Fail, zero stick.

**Height slices** (pawn floor = 0):

| Slice | Z | Jumps |
|-------|---|-------|
| stand | 0 | 0 |
| jump1 / jump2 / jump3 | single / stacked / Recast `jumpApexCm` (~4 m) | 1 / 2 / 3 |
| drop1 / drop2 / drop3 | same apex steps **down** | 1 / 2 / 3 |
| strain | −`maxFallBeforeCriticalCm` (30 m) | `MaxJumps` |

**DistXY rings** (reachable cells only):

| Ring | DistXY | Recipe |
|------|--------|--------|
| **Inner** | ≤ ~2.2 m (on the slab) | still-jump, release at `releaseXY`, 8G slam, air-steer 0 |
| **Mid** | ~2.2 m → ~2× hang | steer, hang, short pin then release |
| **Outer** | up to `MaxLaunchXY` at that slice’s drop | **pin until land**, 1g after hang, full `AirDiveSteer` |

The tick still runs that recipe in time (jump pulses, hang, then pin or release). Handler-trace logs `slice=` / `ring=` plus hang / pin / release. Do not treat every landing as one global timeline.

| Box | Rough figure | Why |
|-----|----------------|-----|
| Hull DistXY | `SearchRadiusCm` = `MaxLaunchXY` vs drop (~**30 m+** on a 30 m pinned dive) | Recast AirDive* is this hull, not slam-shortened ~15–18 m. |
| Min DistXY | **0** | Inner ring: still-jump and drop straight down |
| ΔZ | start may be **below** (jump slices, up to triple apex); dive only once above | Slam does not climb; the leaf jumps first |
| `releaseXY` | success DistXY **− coastXY** | Inner: release **140**, land **180**. Outer does **not** release. |
| `coastXY` | ~**35–100 cm** | `v²/(2a)` at current XY vs brake; ~0.10–0.17 s |

**Do not** hold forward until DistXY = 0 on an inner ring. That is how you overshoot a slab and fall.

Bare `:airDive;` with `move: strafe` is a pulse for the ring lap, not a landing task.

## Slide-to

**When:** Ground, sprint (or sprint speed), rising-edge crouch/slide, floor along the commit for the full travel. ADS allowed while sliding.

**How:** Duration **1.10 s**. Peak **1.45×** sprint, end **0.85×**. `/state.slideDistanceCm` is `EstimateSlideTravelCm()` — commit that distance, do not `goto` mid-slide. Cancel into dodge/dash if tune allows.

**Envelope:** DistXY ≈ slide travel; ΔZ ≈ 0 (flat). Abandon if `CanCommitSlideInDir` would fail (no floor samples). Not for crossing void.

## Dash-to / dodge-to

| | Dash | Dodge |
|--|------|--------|
| Dist | **950** cm | **820** cm (forward × **0.72**) |
| Time | **0.32** s | **0.58** s |
| I-frames | no | **0.2** s |
| Cooldown | **4** s | **4** s |
| Extra | hop Z **200** | |

**When:** Burst on floor (or cancel slide). **How:** Pulse once, **wait for the flag to clear** — do not chop. Envelope is almost a line segment: DistXY near the tune distance, ΔZ ~ 0. Abandon if blocked in the first meters.

## Strafe-to

Hold stick (strafe **380** or forward **420**/sprint). No pulse. Use for a short crab, not a 40 m commute (`goto` or sprint). Envelope = speed × seconds you are willing to hold. Release = Success DistXY; coast is small on ground (friction).

## Recast `goto` vs these leaves

`goto` asks Recast for a path (drop-down, jump-down, cover-over, jump-up ~4 m, jump-over = expensive, **AirDiveDown** / **AirDiveOver** = strain-depth from the walkable + `MaxLaunchXY`). It follows Recast only when the path **reaches** the destination (not `IsPartial()`, not a rim crawl). Off-mesh polys with `UCLNavArea_AirDive` run the shared Launch executor (`recastAirDive`). DropDown stays a walk-off even at 30 m if the pad is under the lip. If Recast cannot connect, `goto` tries **jump-to** when that box passes, else **Launch** when `LaunchInEnvelope` passes. A Launch / recastAirDive arm does **not** settle on DistXY while airborne — same on-ground / on-pad check as `airDive-to`. Jump-to / slide-to / strafe-to still fail a void gap; airDive-to is why goto passes. `goto` does **not** run slide-to or dash-to.

If `goto` fails `no_path` / `no_project_*` **and** both jump and launch boxes miss, rewrite the book from `/state`. Do not invent MCP `plan` / raw `/goto` when a seat exists.

## Choosing a leaf (any map)

1. Same floor, Recast up, DistXY large → **`goto`**.
2. Same floor, DistXY ≈ slide/dash/dodge box → that pulse-to.
3. Landing **below**, connected drop → **`goto`** or walk-off (0-jump).
4. Landing **above** ≤ 4 m with a jump-up face → **`goto`** (JumpUp link) or jump-to if you are already at the face.
5. Landing on a **small high slab** (lintel, pipe) → **`airDive marker=`**. The leaf jumps, dives, and releases.

Re-evaluate the box every tick. Leaving it is Fail, not “try harder with W”.

## Implementation

C++: `CLNavAbilityEnvelope` derives the torus (`FCLLaunchRecipe`) from `FCLMovementTune` + `CLStrainLimits`. `MaxLaunchXY` is the hull. `FCLNavAbilityExec` ticks the looked-up recipe (used by BotBook leaves **and** `FCLAgentGotoDriver`). `airDive marker=` and `goto` onto an AirDive hull edge share that lookup. Off-pad land retries jump-then-dive once when the launch box still holds. `goto` Launchs **only** when the Recast off-mesh poly is `UCLNavArea_AirDive` (not a geometry ΔZ guess).

`airDive-to` Success/GoodEnough on `distXY` also requires **on ground, not diving, and standing on the goal floor** (capsule center ~40–220 cm above a floor-top marker). Same floor check applies to jump / slide / dash / dodge `-to` leaves. That rejects the pit under a menhir lintel. Menhir `menhir_N` markers sit on the **lintel top**, not the court floor; `menhir_N_approach` stays on the pit slab.

Practice greybox `PracticePillar` + catalog `pillar_dive` (`goto marker=pillar_pad`) is the void-gap demo. PvP 3-lane stamps a south **edge pad** (`edge_lip` / `edge_pad`): island Z is apex-survivable below the lip; XY is 90% of the apex-drop hull, rim-inset. Catalog `edge_pad` is `:goto` then `:airDive` (Fail-advance: Recast Launch can stick it; airDive finishes if goto XY-settled early). `/state` reports `edgePadLinked` / `edgePadDistXY` / `edgePadDeltaZ`. **Standing on the island 0.45 s recalls to the lip** (`UCLGreyboxRescue`); falling below the island (Z < pad − 500) uses the same lip teleport. Court-floor `slide_end` / `dash_end` are for catalog `slide_court` / `dash_court`.

Neural-net “brain” is reserved; do not name types `brain`.
