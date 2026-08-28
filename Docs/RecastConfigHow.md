# How Calling arrived at Recast jump-gen config

Knob reference: [RecastLinks.md](RecastLinks.md). This file is rationale and probe history, not a second knob table.

## Verified canary (2026-08-28)

Bake `FindPath(lip, pad)` **valid**, **not partial**, path start on **lip nav mesh**, path end on **pad nav mesh** (`findPathMeshOk=true`).

| Signal | Value |
|--------|--------|
| `findPathMeshOk` | **true** |
| `edgePadPartial` | false |
| `edgePadValidEndsMax` | **3** (`OMLE_Both`) |
| `airDiveJumpLength` | **1508** |
| `airDiveJumpHeight` | **1580** |
| `airDiveJumpMaxDepth` | **1420** (`apexFallAbs`) |
| `edgePadOffMesh` | 1524 |
| `navTiles` | 184 |
| `edgePadBakeMs` | ~81 |
| lipOk / padOk | true / true |

Cheap re-check: `Scripts/dl-verify-dual-composer.ps1 -Sequence nav` (bake probe only). Catalog/JIT `airDive` remains **play**, not a bake pass.

## What Recast is

Recast is **air-dive agnostic**: walkable mesh plus all auto-gen nav links. The pawn’s **largest traversal envelope** is input to JumpLength / JumpHeight / JumpMaxDepth so that reach counts as jumpable. Play flavor is **Down / Up area class** only. UE 5.8 has no Nav Link Bounds Volume; search is the **Nav Mesh Bounds Volume** plus jump-gen knobs. TileSizeUU **1024** is a dirty chunk. CellSize **32**, CellHeight **30** are locked. No Engine patches. Feel locks `AirControl=0.35` / `BaseStrafeSpeed=380`.

Do not invent Calling-only knob units. Do not write “must not be X” on an Unreal knob unless Epic says what that value does.

## JumpMaxDepth (the miss that looked like JumpLength)

Strain `maxFallBeforeCriticalCm` = **3000** (never **30000**).

**JumpMaxDepth** for long recipes = `Abs(JumpApexUpCm(3) − maxFallBeforeCriticalCm)` ≈ `Abs(1580 − 3000)` = **~1420** (NavTune token `apexFallAbs`).

Earlier probes pasted raw **3000** into JumpMaxDepth. That is not the apex−fall difference. With wrong depth, JumpLength **1508** and mid-iterate **4100** both failed the mesh bar (`validEndsMax=1`, rim crawl). After correcting depth to **1420**, clipped JumpLength **1508** passed on the first bake probe.

## Clip JumpLength

Epic JumpLength is the horizontal span of the sampled parabola (to the deep end at −JumpMaxDepth). Calling clips fall-extra XY: JumpLength ≈ same-plane launch (~**1508**), not MaxLaunchXY-with-fall (~4100 with `PinnedDiveXY`). Place sits inside launch-plane intercept x0 ≈ **0.74 L ≈ 1116** minus rim inset **200**; Z locked at apex-survivable lip drop (~1120). Do not retune JumpLength to the place chord; do not move the island to pass FindPath.

**4100 was too far to freeze** as the clip target. Mid-iterate at 4100 with corrected depth still failed mesh FindPath. Frozen send is **1508**.

## Success bar (Calling)

Only:

1. Unreal `FindPath(lip, pad)` valid and not partial.
2. Path[0] on from (lip) nav mesh (`NodeRef`).
3. Path.Last on dest (pad) nav mesh (`NodeRef`).

Scrubbed from success / gates / verify end assertions: DistXY thresholds, dZ / NeedDz, longDive, live-land / goto-stick as Recast canaries. `validEndsMax=1` means Right not snapped; mesh FindPath needs `OMLE_Both` (3).

## What we tried (probe history)

- Volume grow + JumpDistanceFromEdge **80** then **20** — pad in nav; FindPath still incomplete while depth was wrong.
- JumpLength **1508** with JumpMaxDepth pasted as **3000** — incomplete / `validEndsMax=1`.
- JumpLength **4100** / **10000** mid-iterate — hops elsewhere or rim crawl; not a freeze target.
- JumpLength **1508** + JumpMaxDepth **1420** (`apexFallAbs`) + Filter **120** + both `airDive` areas + ends **2500** + edge **20** — **`findPathMeshOk=true`**, `validEndsMax=3`.

Ends / default-area trials were not required after the depth correction.

## Frozen long-recipe NavTune (canary-general)

| Knob | Value |
|------|--------|
| JumpLength | **1508** |
| JumpDistanceFromEdge | **20** |
| JumpMaxDepth | `apexFallAbs` (~**1420**) |
| JumpHeight | `jumpApex` (**1580**) |
| JumpEndsHeightTolerance | **2500** |
| SamplingSeparationFactor | **1** |
| FilterDistanceThreshold | **120** |
| downArea / upArea | both `airDive` |

Same numbers on every map. Island is the canary only.

## What we did not do

Engine under `UE_5.8/Engine`. Grow TileSizeUU. Fake `findPathMeshOk`. Weaken FindPath (skip partial / skip from-dest mesh). Hidden crumbs. Move the island to pass. Shrink JumpLength to hide a miss. Invent Calling ceilings on Epic jump knobs. Add Link Proxy Class to “fix” FindPath.
