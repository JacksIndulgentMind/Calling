# BotBooks

Calling scripts pawn bots as **PlantUML BotBooks**, not as Unreal Behavior Trees, StateTree, or Blueprints. This file is the contract for agents and fork readers.

## What

- **BotBook** — one `.puml` activity file under `Config/BotBooks/`. A node is a **leaf verb** a pawn can run, or a **`ref`** to another BotBook. Recursion is `ref`, not a second type.
- **`UCLBotBookManager`** — loads the catalog, owns per-seat runtimes, ticks them on NetHz, serves hub splice, resolves map markers.
- **`UCLSeatMotor`** — who drives the seat (`human` / `algorithmic` / `remoteAgent` / `cursor`). Humans are not BotBooks. Algorithmic, cursor, and remoteAgent motors ask the manager to tick the current book.

**Brain** is reserved for a later neural-net layer. Do not name types, files, or hub ops `brain` / `play` / `playbook` / `ActionBook` / `TaskTree`.

## How agents script pawns

**Anytime an agent needs to drive a pawn, use a BotBook.** Catalog name or JIT PlantUML on hub `appendBotBook` / `branchBotBook`. That includes a one-off jump, a cover walk, the ring lap, and megalith hops. Do not invent MCP `plan` / `sequence` / `intent` / raw `goto` chains. Those stay **loopback debug** (Practice, no-invoice scenes).

In-game pawn behavior:

1. Bind a **SeatMotor** (`kind: cursor` / `remoteAgent` / `algorithmic`).
2. Run a **BotBook** — catalog name or JIT PlantUML on hub `appendBotBook` / `branchBotBook`.

To test one move, POST a **JIT BotBook** (`{ "type":"appendBotBook", "seatId", "puml":"@startuml..." }`), not a one-off stick sequence. Durable files **must** `goto` by **marker id**. JIT books **may** `goto` x,y,z; if that path lasts, stamp a marker and rewrite the file.

`GET /state?seat=` includes `botBook: { name, nodeId, stack, remaining, jit }`.

MCP tools `hub` / `director` / `state` / `boot` remain. MCP `plan`, `sequence`, `intent`, `goto`: loopback only; do not use them when a lobby seat exists.

## Restricted PlantUML (v1)

Allowed: `@startuml` / `@enduml`, `start` / `stop`, `:verb ...;`, `:ref name;`, `if (predicate) then (label)` / `else` / `endif`, `note` on an activity, `floating note` for fallbacks / `onRespawn` / `trySuccessFor`.

Disallowed: classes, sequence diagrams, includes except `ref`, arbitrary skinparam. Anything outside this subset is `UCLErrorBoundary` / load fail.

**Verbs:** `goto`, `setFocus`, `trackFocus`, `maintainADS`, `fire`, `useAbilitySelf`, `useAbilityFocus`, `jump`, `slide`, `airDive`, `dodge`, `dash`, `melee`, `wait`.

**`goto` vs *-to:** `goto` is the Recast composer (walk, drop, jump links, **AirDiveDown** / **AirDiveOver** area edges) when the path **reaches** the marker. A partial rim-crawl does not count. Jump-gen finds landings with **JumpLength** inside the nav bounds volume, not TileSize — [RecastLinks.md](RecastLinks.md). If Recast cannot connect: **jump-to** exec when that box passes, else **Launch** (dive-to) when `LaunchInEnvelope` passes. `goto` never runs slide-to or dash-to — chain `:goto marker=…;` then `:slide marker=…;`. `airDive marker=` jumps, dives, and releases (or pins) on its own — do not wrap it in `:jump;` / `:wait;`. Bare `jump` / `airDive` / `slide` / `dash` without a marker are pulses. Range boxes: [NavAbilities.md](NavAbilities.md).

**Qualify:** `alive`, `navTiles`, `distXY`, `hasFocus`, `air`, `hasMarker`, `z`, `sliding`, `diving`, `output is Success or GoodEnough`.

**Settle:** Success immediately. GoodEnough holds `trySuccessFor` seconds then settles. Probe `successImpossible` settles now (GoodEnough if in band, else Fail). `fail.timeout` is the hard cap. A hard Fail with no fallback **advances** to the next node (so a missed hop does not abort the rest of the book). Catalog `edge_pad` uses that: `:goto marker=edge_pad` then `:airDive marker=edge_pad`.

`goto` on a catalog file **must** use `marker=id`. xyz on a catalog file is a load error.

## Handler trace

Off by default. Enable any of: `DefaultCalling.ini` `[/Script/Calling.CLBotBookSettings] bTraceHandlers=true`, run arg `-BotBookTrace`, or `dl.BotBook.Trace 1`. Logs go to `LogCallingBotBook` in `Saved/Logs`.

Every leaf logs start and settle (verb, node, marker, DistXY, dZ, outcome). Jump / slide / dash / dodge / airDive / Launch also log the selected box, torus `slice=` / `ring=` (Launch / airDive-to), phase pulses (jump, hang, pinnedSteer, stick hold/release, slide latch, dash/dodge flag), ~4 Hz samples, per-phase velocity min/max/mean, and settle miss (`missXY`, `missZ`, `releaseMiss`, `onPad`). `goto` logs which arm it picked (`recast`, `recastAirDive`, `jumpTo`, `launch`).

## Why not Unreal Behavior Trees / StateTree / Blueprints

Unreal already has dedicated bot-decision systems. Blueprints are the wrong comparison (they are all game code). The first-party matches are:

- **Behavior Trees** (`AIModule`) — selectors, sequences, decorators, blackboard.
- **StateTree** (`GameplayStateTree`) — hierarchical states + transitions; `StateTreeAIComponent` on an `AIController`. Closest Epic analog.
- **EQS** — scores locations; used from a tree, not a tree itself.

Calling still uses BotBooks. Not because Unreal has nothing for this:

- **Agent-authorable text.** Cursor writes and diffs `.puml`. It cannot reasonably author `.uasset` StateTrees/BTs without the editor.
- **JIT splice.** Hub `appendBotBook` `{ puml }` is the poke-a-move path. StateTree has no first-class “POST inline tree text onto a live seat.”
- **OSS / git / engine-agnostic.** Books live in `Config/`, reviewable without Unreal. Same files could drive a non-UE client later.
- **Narrow verb set.** StateTree/BT are general (anim, gizmos, quests). We would still write custom tasks for marker-goto, ADS, abilities. The expensive part is the language + hub, not Epic’s tick graph.
- **Seat model.** Drivers are lobby SeatMotors sharing one pawn codec, not `AIController` + blackboard.
- **Settle semantics.** Success vs GoodEnough + `trySuccessFor` + probe is not native BT (`Success`/`Fail`/`InProgress`) or StateTree.

Recast, Enhanced Input, and the pawn codec stay Unreal.

**Not this pass:** do not enable GameplayStateTree or wrap Behavior Trees. Later, if we want Epic’s debugger, export BotBooks into StateTree — do not rewrite the language.

## Markers

Greybox stamps `ACLTaskMarker` actors when it builds. Same ids across maps; each layout re-stamps.

PvP minimum: `spawn_red`, `spawn_blue`, `court_center`, `hide_center_lee`, `menhir_0`…`menhir_7`, `menhir_*_approach`, `cover_west_cut`, `cover_east_cut`, `edge_lip`, `edge_pad`, `slide_end`, `dash_end`. Social/raid/practice: `spawn_default`.

`UCLSocialMarkerWidget` is UI chrome, not a nav target.
