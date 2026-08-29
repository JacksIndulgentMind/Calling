# Recast tiles, cells, and jump-gen

How Calling bakes courtyard Recast and finds off-mesh landings. Feel locks stay `AirControl=0.35` and `BaseStrafeSpeed=380`. Landing recipes: [NavAbilities.md](NavAbilities.md). Pawn drive: [BotBooks.md](BotBooks.md).

**Read Epic’s navigation docs before testing or changing nav mesh gen:**

- Hub: [Navigation System in Unreal Engine](https://dev.epicgames.com/documentation/en-us/unreal-engine/navigation-system-in-unreal-engine)
- [Basic Navigation](https://dev.epicgames.com/documentation/en-us/unreal-engine/basic-navigation-in-unreal-engine)
- [Automatic Navigation Link Generation](https://dev.epicgames.com/documentation/en-us/unreal-engine/automatic-navigation-link-generation)

Do **not** edit Engine source. Do **not** grow `TileSizeUU` to make a hop fit. Do **not** invent Calling-only knob units. Do **not** write “must not be X” on an Unreal knob unless Epic says what that value does.

## Recast is air-dive agnostic

Recast still does **walkable nav mesh** and **all** auto-gen nav links (cover, drop, jump-up, jump-down, jump-over, long hops). It is **air-dive agnostic**. There is no Recast “air dive knob.” Jump-gen samples a **jump parabola** (`JumpLength` / `JumpHeight` / `JumpMaxDepth`).

The pawn’s **largest traversal envelope** must count as **jumpable**: feed that envelope into JumpLength, JumpHeight, and JumpMaxDepth so the parabola can reach as far as the pawn can. That is the whole Recast↔air-dive relationship. Play flavor (how the pawn actually traverses) is **Down / Up area class** only (`goto` Launchs `UCLNavArea_AirDive` off-mesh). Register that class on Recast (`OnNavAreaAdded`) before `Build`.

**Link Proxy Class** (`UGeneratedNavLinksProxy`) is optional path-follow (`OnLinkMoveStarted`). It does not create or snap the Recast edge.

Epic auto-gen: one **Nav Mesh Bounds Volume** (walk + jump-gen sample — UE 5.8 has no separate Nav Link Bounds Volume), Recast **Generate Nav Links**, then the jump config. Large Jump Length increases tile **rasterization** (LinkSpill), not TileSize.

## Knobs (do not mix them)

Do not grow **TileSizeUU**. Do not move the greybox canary island. Do not patch Engine. Sources: Recast **Generation** + **Nav Link Jump Configs** (`ARecastNavMesh`, `FNavLinkGenerationJumpConfig`), **Nav System** (`UNavigationSystemV1`), one **Nav Mesh Bounds Volume**. Display flags are debug only. Values are Epic defaults or pawn physics / strain (uu/cm).

### Auto-gen jump config (`FNavLinkGenerationJumpConfig`)

Epic wording ([Automatic Navigation Link Generation](https://dev.epicgames.com/documentation/en-us/unreal-engine/automatic-navigation-link-generation)). `UIMax` on JumpLength is an **editor slider**, not `ClampMax`.

| Knob | Epic / Calling value |
|------|----------------------|
| **bEnabled** | This jump recipe is used. |
| **Name** | CoverOver, DropDown, JumpUp, JumpDown, JumpOver, plus the two long-hop recipes in NavTune. |
| **JumpLength** | Epic: **horizontal length of the jump** (span to the deep end at −JumpMaxDepth). Calling send: clipped same-plane launch XY (stick-forward triple, full up+down of JumpHeight, walk cap, no PinnedDiveXY) ≈ **1508**. Launch-plane intercept x0 ≈ **0.74 L ≈ 1116**. Frozen. Do not retune to hide a miss. Large values grow LinkSpill, not TileSize. Limited by the **Nav Mesh Bounds Volume**. |
| **JumpDistanceFromEdge** | Epic: how far from the NavMesh edge the jump starts. Long recipes **20** (80 then 20; neither produced a canary off-mesh). Rim inset stays **200** and does not follow this knob. |
| **JumpMaxDepth** | Epic: how far below the start to look for landing ground. Calling long recipes **Abs(JumpApex − maxFallBeforeCriticalCm)** ≈ **1420** (NavTune token `apexFallAbs`; maxFall **3000**, never 30000). Negative = look up (JumpUp). |
| **JumpHeight** | Epic: peak height relative to the starting point. Calling long recipes / JumpUp: full RocketPulse triple peak **1580** (`(640+560+560)²/(2×980)`). |
| **JumpEndsHeightTolerance** | Epic: tolerance at both ends to find ground. Long recipes **2500**. |
| **SamplingSeparationFactor** | Epic: × CellSize between sampling trajectories. Epic ClampMin **1**. Larger = faster, can miss. Long recipes **1**. |
| **FilterDistanceThreshold** | Epic: when filtering similar links, distance to compare **segment endpoints**. Blends **adjacent** almost-duplicate hops (granularity). **0** turns merge off (noisier, not more reach). Recommended **120**. Long recipes **120**. DropDown **120** (terrace → ramp). JumpDown **180**. |
| **LinkBuilderFlags** | `CreateCenterPointLink` and/or `CreateExtremityLink`. |
| **DownDirectionAreaClass** / **UpDirectionAreaClass** | Traversal flavor only. Same class → one bidirectional link. **Null** → that direction is not generated (Epic). Long recipes: both directions `UCLNavArea_AirDive` (**DefaultCost 50** vs walk **1** / LongJump **25**) so FindPath prefers the walk/DropDown corridor when both exist. `goto` Launchs that hop when FindPath still picks AirDive (island, no walk). JumpUp: Up = default. DropDown: Up may be null (walk-off). |
| **LinkProxyClass** | Optional path-follow only. Not required for FindPath. |

### Recast generation (`ARecastNavMesh`)

| Knob | Role / Calling |
|------|----------------|
| **bGenerateNavLinks** | Master switch. **True**. |
| **TileSizeUU** | Dirty chunk. **1024**. Never a search radius. |
| **bFixedTilePoolSize** / **TilePoolSize** | Unbounded (`false`) / **4096**. |
| **AverageLayersPerTile** / **ExpectedMaxLayersPerTile** | **8** / **12**. Z layers per XY tile. |
| **bMinimizeLinkPoolSize** | Default true shrinks the off-mesh pool. **False**. |
| **NavMeshResolutionParams** | **CellSize 32**, **CellHeight 30** (255×30 = 7650 cm span), Recast **AgentMaxStepHeight 120**. Pawn step stays **70**. Locked; do not iterate voxels so a hop fits. |
| **AgentRadius** / **AgentHeight** / **AgentMaxSlope** | **42** / **192** / **55**. |
| **MinRegionArea** / **MergeRegionSize** | **0** / **400**. Too aggressive merge can eat a small pad. |
| **MaxSimplificationError** / **SimplificationElevationRatio** | **1.3** / **1**. |
| **MaxSimultaneousTileGenerationJobsCount** | Parallel bake. |
| **TileNumberHardLimit** | Hard cap (power of 2). |
| **NavMeshOriginOffset** | Tile grid origin. Does not change DistXY. |
| **DefaultMaxSearchNodes** / **DefaultMaxHierarchicalSearchNodes** | **4096** / **4096**. A* cap. |
| **HeuristicScale** | **0.999**. |
| **LedgeSlopeFilterMode** | Ledge filtering (Recast default). |
| **RegionPartitioning** / **LayerPartitioning** | Watershed / Monotone / ChunkyMonotone. |
| **RegionChunkSplits** / **LayerChunkSplits** | ChunkyMonotone splits. |
| **bSortNavigationAreasByCost** | **True**. |
| **bPerformVoxelFiltering** | **True**. Clip voxels to nav bounds. |
| **bMarkLowHeightAreas** / **bUseExtraTopCellWhenMarkingAreas** | Low-height and flush-top. Extra top **True**. |
| **bFilterLowSpanSequences** / **bFilterLowSpanFromTileCache** | Low-span storage. |
| **bDoFullyAsyncNavDataGathering** | Gather off game thread. |
| **bAllowNavLinkAsPathEnd** | Epic default **false**: path cannot end on an off-mesh poly. Calling **True**. |
| **bStoreEmptyTileLayers** / **bUseVirtualFilters** / **bUseVoxelCache** | Storage / filter / voxel cache. |
| **RuntimeGeneration** | Dynamic. |
| Display | `bDrawNavLinks`, `bDrawFailedNavLinks`, `bDrawTileBuildTimes`. Not bake. |

### Nav system (`UNavigationSystemV1`)

| Knob | Role |
|------|------|
| **bAutoCreateNavigationData** | Spawn Recast when bounds exist. |
| **bInitialBuildingLocked** | Greybox releases the lock before Build. |
| **bAllowClientSideNavigation** | Client nav. |
| **bWholeWorldNavigable** | Broken/generic; greybox still uses a bounds volume. |
| **bGenerateNavigationOnlyAroundNavigationInvokers** | Invoker-only tiles. Off. |
| **DataGatheringMode** | Instant vs lazy gather. |
| **SupportedAgents** | Agent list. |

### Bounds

**NavMeshBoundsVolume** (`CLGreyboxNavBounds`): walk raster **and** jump-gen sample. No second link-only volume in UE 5.8. Must contain lip, pad, and the hop corridor (XY + drop Z). Grow the **volume**; do not move the pad.

### Manual proxy (not auto-gen)

`ANavLinkProxy` / `FNavigationLink`: **SnapRadius**, **SnapHeight**, **LeftProjectHeight**, **MaxFallDownLength**, direction. Greybox destroys leftover tagged proxy actors. Auto-gen uses jump config, not these.

## Success: `findPathMeshOk=true`

The south island is a **canary** for the general config (any map). Bake success is Unreal `FindPath(lip, pad)` **valid**, **not partial**, with **path[0] on the lip nav mesh** and **path.Last on the pad nav mesh** (same `NodeRef` as the projected from/dest). No DistXY / dZ / NeedDz / longDive gates. Catalog/JIT `airDive` is play, not a bake pass. `validEndsMax=1` is Right not snapped (`OMLE_Left` only); FindPath needs `OMLE_Both` (3).

| Signal | Meaning |
|--------|---------|
| `edgePadLipOk` / `edgePadPadOk` | Both floors project. Geometry in bounds. |
| `edgePadValidEndsMax=1` | Off-mesh far end not snapped (Left only). |
| `findPathMeshOk=true` | FindPath from/dest mesh bar. |
| `edgePadPartial` | FindPath returned a rim crawl (end not on pad mesh). |
| `edgePadBakeMs` | `NavSys->Build()` wall time. |

Cheap check (no ring / megalith / composer):

```
Scripts/dl-verify-dual-composer.ps1 -Sequence nav
```

Uses `director arena` if not already `pvp`. `VERIFY_OK` if `navTiles > 0` and `edgePadPadOk`. Canary verified `findPathMeshOk=true` (JumpLength **1508**, JumpMaxDepth **1420**). Re-probe if NavTune or bounds change; `findPathMeshOk=false` is printed, not a throw.

## What `/state` is telling you

| Flag | Meaning |
|------|---------|
| `edgePadLipOk` / `edgePadPadOk` | Projection. |
| `findPathMeshOk` | FindPath valid + !partial + start on lip mesh + end on pad mesh. |
| `edgePadPartial` / `edgePadOffMesh` / `edgePadValidEndsMax` | Generated vs connected. |
| `navTiles` | Active tiles. ~100s at TileSize 1024. **22** meant the 36 m tile hack. |

`padOk=true` while `findPathMeshOk=false` is a seeded platform: both floors have nav. Projection is not a path.

## Greybox canary island

Place from **movement + strain**, then one pull into Recast’s launch-plane volume. Chord = x0 (parabola intercept with the lip plane). DistXY = x0 − rim inset **200**. Z = (3000 − 300) − triple apex (~1120 cm below the lip). Recast JumpLength / JumpDistanceFromEdge must not move the pad after that. Do not lift Z to pass FindPath.

| Kind | Role |
|------|------|
| Jump-gen | JumpLength / LinkSpill search that envelope. Recast knobs do not move the pad. |
| Link Proxy Class | Path-follow only. Not used to create the edge. |
| Seeded polys | Island stays in the **same** bounds volume (corridor included). |
| Play | Catalog `:airDive marker=` is an authored leaf (lintels). Island hop is Recast `goto` (AirDive area off-mesh). |

## Jump-gen sampler

Epic knobs above. Detour floors the parabola **start and end**.

- **JumpLength** = horizontal length of the jump (clipped same-plane ≈ **1508**). **Bounds volume** limits where generation can see geometry.
- **JumpHeight** = peak above the start (**1580**).
- **JumpMaxDepth** = **Abs(apex − maxFall 3000) ≈ 1420** (`apexFallAbs`), not raw 3000.
- Ends / sample / filter from `NavTune.json`. FilterDistance **120** on long recipes (adjacent merge).
- Do **not** `min(JumpLength, TileSizeUU)`.

## Greybox bake order

In `ACLGreyboxFloors::RebuildNavigation`:

1. Bounds volume from **all** platforms **and** the lip→pad corridor (XY + drop Z). Walk + jump-gen.
2. Create Recast if missing.
3. `CLNavLinkPolicy::ApplyToRecast`: CellSize 32, TileSize 1024, CellHeight for span, jump configs, area classes, `ConditionalConstructGenerator`.
4. Destroy leftover island proxies (old `ANavLinkProxy` tags, not LinkProxyClass).
5. `UpdateComponentData` on platform meshes.
6. `NavSys->Build()`. Log bake ms.
7. Probe lip→pad. Store flags on `/state`.

## Debug

- `Greybox edgePad island` — place chord / L / x0.
- `Greybox offMesh nearPad` — validEnds, gen.
- `Greybox edgePad FindPath meshOk=` — from/dest mesh probe.
- `Greybox edgePad AABB` — geo vs nav Z vs pad.
- `Greybox nav bake ms=` — Build wall time.
- `-Sequence nav` — bake probe only.

## Playbook: disconnected slab

1. Read the Epic hub linked above.
2. Put both floors **and** the hop corridor in the **NavMeshBoundsVolume**. Rebuild after Recast exists. Confirm both project.
3. JumpLength from clipped same-plane launch XY (~**1508**); keep lip+pad inside the **Nav Mesh Bounds Volume**. JumpMaxDepth **Abs(apex − maxFall) ≈ 1420**, JumpHeight **1580**, end Z-tol covering the drop. FilterDistance **120**.
4. `-Sequence nav` until `findPathMeshOk=true`. Do not change TileSizeUU. Do not patch Engine.

How we arrived at these numbers: [RecastConfigHow.md](RecastConfigHow.md) (**how we arrived at the proper config** — verified canary). Do not shrink JumpLength, grow TileSize, patch Engine, or move the island to hide a miss.
