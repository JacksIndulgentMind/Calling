# Recast tiles, jump-gen, and off-mesh

How Calling bakes a Recast path from the south court lip to the greybox island, and how to connect the next disconnected slab. Feel locks stay `AirControl=0.35` and `BaseStrafeSpeed=380`. Landing recipes: [NavAbilities.md](NavAbilities.md). Pawn drive: [BotBooks.md](BotBooks.md).

This is one Recast mesh (`ARecastNavMesh`), not a second nav system. Tiles are chunks of that mesh. Off-mesh connections (jump-gen or simple `ANavLinkProxy`) are edges Recast stores on a **start tile** and snaps the far end onto walkable polys.

## What `/state` is telling you

| Flag | Meaning |
|------|---------|
| `lipOk` / `padOk` (log) | `ProjectPointToNavigation` found a poly at the lip / island. Geometry is in the bounds volume and was pushed to the octree. |
| `edgePadLinked` | Bake probe: `FindPath(lip, pad)` is complete, DistXY > 800, dZ < −1500. The pathfinder can ride an AirDive off-mesh, not a court-rim crawl. |
| `navTiles` | How many Recast tiles are active. 10 m tiles on the 3-lane were ~194; 36 m tiles were ~22. |

`padOk=yes` while `edgePadLinked=false` means both floors have nav. The off-mesh far end is not snapped. Do not treat projection as a path.

## Three ways to get an off-mesh (we used two)

| Kind | What it is | Greybox island |
|------|------------|----------------|
| **Jump-gen** | Recast samples walkable **contour edges** with a jump parabola (`NavTune.json` → `CLNavLinkPolicy`). `bGenerateNavLinks`. | **Yes.** AirDiveDown after JumpLength matched the island chord. Debug: `gen=yes`. |
| **Simple link** | `ANavLinkProxy` `PointLinks`: static Left→Right, area class, snap radius/height. Baked at generate time. | **Yes.** `SeedEdgePadAirDiveLink` (`CLGreyboxAirDiveLink`). Smart link **off**. Debug: `gen=no`. |
| **Smart link** | `UNavLinkCustomComponent`: enable/disable at runtime, optional area swap. | **No.** `bSmartLinkIsRelevant=false`, `SetSmartLinkEnabled(false)`. |

Jump-gen **discovered** AirDiveDown once the sampler end sat on the pad (six long generated links in the log). The proxy is a **manual hint** of the same chord with `UCLNavArea_AirDive`. Neither connected `FindPath` until the far end could snap in the start tile or an **edge-neighbor** tile.

`goto` Launchs only off-mesh whose Recast area is `UCLNavArea_AirDive`. Register that class on the Recast actor (`OnNavAreaAdded`) before `Build`, or the link bakes with a null area and the filter ignores it.

## Jump-gen sampler (spine end only)

Detour jump-gen floors the parabola **start and end**, not mid-chord ground. `MAX_SPINE` is 8 samples for collision, landing is the last point.

- **JumpMaxDepth** = strain from the **walkable edge** (`maxFallBeforeCriticalCm` = **3000**). Policy overwrites JSON.
- **JumpHeight** = **0** for AirDive (hang+pin, not a hop). End of the spine is always −JumpMaxDepth below the edge.
- **JumpLength** for AirDiveDown = island chord: 90% of `MaxLaunchXY` at the **apex-drop** hull, minus rim inset, plus `JumpDistanceFromEdge` (`AirDiveBakeJumpLengthCm` ≈ **3107**). Full `MaxLaunchXY` at the 30 m walk-off drop (~3783) looks **past** the 8 m pad into void (and can collide with the pad mid-spine).
- **JumpEndsHeightTolerance** must cover island Z vs end-of-spine Z (island is ~2300 below the lip; spine end wants −3000).
- **AirDiveOver** still uses `SearchRadiusCm` at the strain drop (long same-level gaps).

`LinkSpillDistance` ≈ JumpLength − edge. That expands **voxelization** so the generating tile can *sample* landing ground. It does **not** make the far end snap across extra tiles (see below).

## Far-end snap: same tile or edge neighbor

UE 5.8 `dtNavMesh::connectExtOffMeshLinks` walks **immediate neighbor tiles** only (plus internal if both ends classify inside the start tile AABB).

A DistXY **3067** cm hop with `TileSizeUU=1000` spans **three** tiles. Debug: `validEnds=1` (Left only). Path is a 232 cm rim crawl (`partial=yes`) even with 124 off-mesh in the mesh.

Raising `TileSizeUU` to **3600** (island span + 500, clamped in `CLNavLinkPolicy`) puts lip and pad in the **same tile or an edge neighbor**. Probe: `edgePadLinked=true`, DistXY ~3067, dZ ~−2310.

**Smaller tiles plus “scan more tiles outward” is not a Recast knob.** Spill grows the heightfield for sampling; connect is still 0–1 tile hop. Scanning N hops would be an engine change, or a chain of short off-mesh. Do not set `TileSizeUU` to the old ~40 m hull (too few tiles + coarse CellHeight used to kill spawn→court walking). Recast `AgentMaxStepHeight` for bake already covers those voxel ledges; pawn `MaxStepHeight` stays 70.

## Tile size is a knob (this island is a stretch)

The south pad is a **worst-case jump-then-dive fixture** ~31 m off the court. Most real hops (cover, lintels, lane cuts) are much closer. Do **not** treat 3600 as the forever default.

Costs of 36 m tiles (nav **194 → 22** on the 3-lane):

- Coarser walkable mesh: ramps, 9° slopes, small cover voxelize worse.
- Short jump-up / drop / cover-over sampling sees fewer contours; megalith **7/8** after this bake is the leftover to revisit.
- Path corridors are blockier.
- A dirty rebuild is a 36 m square.

**Set `TileSizeUU` to the longest off-mesh you actually need.** Policy today: `Clamp(islandChord + 500, 2000, 3600)` plus `DefaultEngine.ini`. Shrink when the far pad goes away or hops stay inside one 10–20 m tile. Grow only when a disconnected landing is farther than one tile hop. A named NavTune field is optional later; do not bake the greybox island into every map.

## Both floors in one mesh

One Recast, two islands of polys. Not a second nav bounds actor for the pad.

- Court cubes and the island `AddPlatform` must sit in the **nav bounds volume** (greybox expands the box by the pad AABB).
- Create Recast **first**, then `UpdateComponentData` on meshes (octree ignores geometry until Recast exists).
- `lipOk` + `padOk` = both project. That is necessary and not sufficient for `edgePadLinked`.

`MaxFallDownLength` / `LeftProjectHeight` on a simple link that already sits on stand height: leave **0**. Projecting down from inside the pad volume can yank the Right point into void (`validEnds` Left-only). Use `SnapRadius` / `SnapHeight` instead. `bSnapToCheapestArea=false` so Recast does not snap both ends onto the court.

## Greybox bake order

In `ACLGreyboxFloors::RebuildNavigation`:

1. Bounds volume from platforms (+ pad box on PvP).
2. Create Recast if missing.
3. `CLNavLinkPolicy::ApplyToRecast`: agent, CellHeight, Recast step, jump configs, AirDive/LongJump areas, `TileSizeUU`, `ConditionalConstructGenerator`.
4. Seed simple AirDive `ANavLinkProxy` (deferred spawn so `PointLinks` exist before register).
5. `UpdateComponentData` on all platform meshes; `UpdateActorAndComponentData` on the proxy.
6. `NavSys->Build()`.
7. Probe path lip→pad; log `validEnds`, long DistXY off-mesh, `gen=yes/no`, `areaId`.

`JumpMaxDepth` stays 3000 from the walkable edge. Island Z stays apex-survivable (~2300 below the lip).

## Debug

Logs (`LogCalling`):

- `Greybox edgePad island` — DistXY, dZ, bake JumpLength.
- `Greybox AirDive Recast link` — proxy spawned.
- `Greybox AirDive areaId=` — must not be `INDEX_NONE`.
- `Greybox offMesh long` — DistXY, dZ, `area`, `validEnds`, `gen`.
- `Greybox edgePad Recast linked=` — probe. `partial=yes` + DistXY ~200 = rim crawl.

`validEnds`: `1` = Left only (far end not snapped). Both ends needed for `FindPath` to leave the court.

## Playbook: next disconnected slab

1. Put **both** floors in the same nav bounds; rebuild after Recast exists. Confirm both `ProjectPointToNavigation`.
2. If jump-gen should find it: JumpLength = landing DistXY from the walkable edge (sampler **end** on the pad, not past it). JumpMaxDepth / end Z-tol cover the drop.
3. Optional simple `ANavLinkProxy` (AirDive area) if you want a guaranteed chord. Not a smart link unless you need runtime toggle.
4. Tile size ≥ DistXY **only if Recast must neighbor-connect that hop**. Typical cover/lintel hops can keep ~10 m tiles.
5. Rebuild. Probe: complete path, DistXY and dZ match the hop, `validEnds` both, area is AirDive if `goto` should Launch.
6. Catalog `goto` then `airDive` Fail-advance if XY-settle can fire in the air. JIT `airDive` is not a substitute for the Recast link on the island probe.
