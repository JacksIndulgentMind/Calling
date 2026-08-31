# BotBooks

Calling scripts pawn bots as **PlantUML BotBooks**, not as Unreal Behavior Trees, StateTree, or Blueprints. This file is the contract for agents and fork readers.

## What

- **BotBook** — one `.puml` activity file under `Config/BotBooks/`. A node is a **leaf verb** a pawn can run, or a **`ref`** to another BotBook. Recursion is `ref`, not a second type.
- **`UCLBotBookManager`** — loads the catalog, owns per-seat runtimes, ticks them on NetHz, serves hub splice, resolves map markers.
- **`UCLSeatMotor`** — who drives the seat (`human` / `algorithmic` / `remoteAgent` / `cursor`). Humans are not BotBooks. Algorithmic, cursor, and remoteAgent motors ask the manager to tick the current book.

**Brain** is reserved for a later neural-net layer. Do not name types, files, or hub ops `brain` / `play` / `playbook` / `ActionBook` / `TaskTree`.

## How agents script pawns

**Anytime an agent needs to drive a pawn, use a BotBook.** Catalog name or JIT PlantUML on hub `appendBotBook` / `branchBotBook`. That includes a one-off jump, a cover walk, the ring lap, and megalith hops. Do not invent MCP `plan` / `sequence` / `intent` / raw `goto` chains. Those stay **loopback debug** (Practice, no-invoice scenes).

Mid-match “personality” is still a BotBook. Why splice, monitor vs pawn, keep the objective spine: parent [design/bot-books-mid-match.md](../../../design/bot-books-mid-match.md). Engagement law: [design/feel-notes.md](../../../design/feel-notes.md). Space vs kills (10 kills in conflict with a place): [design/map-design.md](../../../design/map-design.md).

In-game pawn behavior:

1. Bind a **SeatMotor** (`kind: cursor` / `remoteAgent` / `algorithmic`).
2. Run a **BotBook** — catalog name or JIT PlantUML on hub `appendBotBook` / `branchBotBook`. Send **`agentId`** (the connecting agent’s UUID) and **`intendedTarget`**: `localHuman`, `headlessBot`, or a seat GUID. Every process mints an **`instanceId`** at startup. Hub/`/state` replies echo `instanceId` + `agentId`; `LogCallingHub` and `goto tick` stamp both. `originInstanceId` is the host when a **proxy** command runs on the guest (`Calling-Connect-Mode: proxy`, not per-op `via`). `LogCallingHub` errors `remote_player_pawn` if the body is another process’s net-human. Two-box ingress: [VirtualMp.md](VirtualMp.md) / [HubIngress.puml](HubIngress.puml).

To test one move, POST a **JIT BotBook** (`{ "type":"appendBotBook", "seatId", "puml":"@startuml..." }`), not a one-off stick sequence. Durable files **must** `goto` by **marker id**. JIT books **may** `goto` x,y,z; if that path lasts, stamp a marker and rewrite the file.

`appendBotBook` starts the book if the seat is idle. If a book is already running, it **queues** (FIFO, max 4) and does **not** `CancelGoto` — Tick still runs the live leaf. Re-append of the same catalog name while it is live or queued is a no-op. `branchBotBook` stops the current leaf and replaces remaining walk. **`cause` is required:**

| `cause` | Meaning | Effect |
|---------|---------|--------|
| `execution` (aliases `failure`, `fail`) | The bot failed the book **independent of outside factors** (nav/`goto` did not walk, timeout, stack leak, poller treated idle as done). Not combat, personality, or a world change. | Increments `/state.botBook.executionFails`. **One** fail is `executionError: true` and reports `botbook_execution` (`UCLErrorBoundary`, NonDeterministic) — find it and **fix**, do not hide it. Hub `branchBotBook` still starts the replacement book (`ok: true`) but stamps `executionError` / `alert: botbook_execution` / `error: botbook_execution` on the reply so a drive cannot treat it as a successful poke. Does **not** count toward cancel-storm. |
| `situation` (aliases `combat`, `personality`, `strategic`) | Combat replan, personality, or other world change. The book was not an execution defect. | Same cancel-storm as before: **8 situation branches in 4 seconds** is `botbook_cancel_storm`. |

Missing `cause` is `missing_branch_cause`. Unknown values are `invalid_branch_cause`. `clearBotBook` and append-queue are not this signal. `PushBook` deeper than **32** frames is `botbook_stack`. Live book is `/state.botBook.name` (not `catalog`).

`GET /state?seat=` includes `botBook: { name, nodeId, verb, whiles, leafElapsed, pawnStill, focusSeat, enemyDistXY, enemyX, enemyY, stack, stackLen, remaining, remainingLen, queued, queueLen, jit, lastBranchCause, lastBranchNodeId, lastBranchBook, executionFails, executionError, followAlert, lastFollowAlert, followed }`. `followAlert` is **this tick** (`botbook_unknown_verb`, `botbook_unknown_while`, `botbook_missing_marker`, `botbook_not_local`, `botbook_goto_start_failed`, `botbook_goto_stick_clobber`, `botbook_goto_no_stick`, `botbook_goto_loc_still`, `botbook_idle`). `lastFollowAlert` is the latched code after the condition clears. `followed` is false only while `followAlert` is live. `loc_still` is pawn loc not moving ~80 cm for 0.5 s **and** velocity Size ≤ 80, with goal DistXY still outside the leaf’s outer settle (GoodEnough distXY if set, else Success distXY, else 300) — not Recast waypoint `gotoStuck`. Airborne travel (jump/drop) resets the still clock; a grounded perch does not. One **execution** hit (`unknown_*` / `not_local` / `goto_start_failed` / `stick_clobber` / `missing_marker`) sets `executionError`. The **first** followAlert or `cause=execution` also **fails the PvP match** (`modeResult=fail`, `modeFailReason` is that code) and appends `/state.events`. Pollers dump `events` and stop — do not resume `in_progress`. A pass is `modeResult=winner` with `botbook_append` / `kill` / `shrine_held` / `mode_winner` in `events`. `/state` also has `agentMoveX` / `agentMoveY` / `agentFire` / `lookTrack` / `lookTrackSeat`. Helper: `Scripts/dl-assert-command-followed.ps1`.

MCP tools `hub` / `director` / `state` / `boot` remain. MCP `plan`, `sequence`, `intent`, `goto`: loopback only; do not use them when a lobby seat exists.

## Restricted PlantUML (v1)

Allowed: `@startuml` / `@enduml`, `start` / `stop`, `:verb ...;`, `:ref name;`, `if (predicate) then (label)` / `else` / `endif`, `note` on an activity, `floating note` for fallbacks / `onRespawn` / `onStop` / `trySuccessFor`.

Disallowed: classes, sequence diagrams, includes except `ref`, arbitrary skinparam. Anything outside this subset is `UCLErrorBoundary` / load fail.

**Verbs:** `goto`, `setFocus`, `trackFocus`, `maintainADS`, `fire`, `useAbilitySelf`, `useAbilityFocus`, `jump`, `slide`, `airDive`, `dodge`, `dash`, `melee`, `wait`.

**`while:` on `goto`:** `trackFocus` / `fire` / `maintainADS` latch holds. They must **not** replace the Recast stick. `ApplyAgentIntent` assigns `Move` every call — a fire-only pulse with Move=0 plants the pawn at spawn and still shoots. Latch those holds; do not write Move. If that happens anyway, `/state.botBook.followAlert` is `botbook_goto_stick_clobber` on the first tick. `trackFocus` also must **not** own look during Recast Launch (`goto` driver `bFlight`): Launch `FaceGoal` waits until heading is within 18° of the hop, then jumps. If `while:` retargets look at the enemy every tick, the pawn never jumps and loc_still is skipped for the whole Launch. Fire/ADS still latch; look returns to the enemy after land.

**`goto` vs *-to:** `goto` is the Recast composer (walk, drop, jump links, **AirDiveDown** / **AirDiveOver** area edges) when the path **reaches** the marker. A partial rim-crawl does not count as Success. Jump-gen finds landings with **JumpLength** inside the nav bounds volume, not TileSize — [RecastLinks.md](RecastLinks.md). When FindPath is partial (A* node budget), `goto` walks that polyline and repaths — it does **not** arm jump-to or Launch as a substitute for Recast. **Do not drop Recast walk waypoints** because a rise or lid is hard; execute the polyline (walk-advance / SKIP_LAUNCH only when DistXY is already inside `GotoWaypointRadius`). `goto` never runs slide-to or dash-to — chain `:goto marker=…;` then `:slide marker=…;`. `airDive marker=` jumps, dives, and releases (or pins) on its own — do not wrap it in `:jump;` / `:wait;`. Bare `jump` / `airDive` / `slide` / `dash` without a marker are pulses. Range boxes: [NavAbilities.md](NavAbilities.md).

**Qualify:** `alive`, `navTiles`, `distXY`, `enemyDistXY`, `hasFocus`, `air`, `hasMarker`, `z`, `sliding`, `diving`, `output is Success or GoodEnough`. `enemyDistXY` is DistXY to the other combat pawn (`/state.botBook.enemyDistXY`); missing or dead enemy is far (`< N` is false).

**Settle:** Success immediately. GoodEnough holds `trySuccessFor` seconds then settles. Probe `successImpossible` settles now (GoodEnough if in band, else Fail). `fail.timeout` is the hard cap. A hard Fail with no fallback **advances** to the next node (so a missed hop does not abort the rest of the book). `goto` Success/`distXY` also requires on ground, not diving, standing on the goal floor (same as `*-to`). Catalog `edge_pad` is Recast `goto` only; megalith books still chain `:airDive marker=` as an authored verb.

`goto` on a catalog file **must** use `marker=id`. xyz on a catalog file is a load error. `marker=live_shrine` (alias `liveShrine`) resolves to `/state.liveShrine` when the leaf starts.

**`onStop`:** floating-note catalog name to start when this book hits `stop` with an empty queue. Same idea as `onRespawn`, not a stack `ref` (self-`ref` is a load cycle). A combat book with `onRespawn` and no `onStop` that exhausts while `modeResult=in_progress` is `botbook_idle` (match fail). Do not wait on an empty book.

## Handler trace

Off by default. Enable any of: `DefaultCalling.ini` `[/Script/Calling.CLBotBookSettings] bTraceHandlers=true`, run arg `-BotBookTrace`, or `dl.BotBook.Trace 1`. Logs go to `LogCallingBotBook` in `Saved/Logs`.

Every leaf logs start and settle (verb, node, marker, DistXY, dZ, outcome). Jump / slide / dash / dodge / airDive / Launch also log the selected box, torus `slice=` / `ring=` (Launch / airDive-to), phase pulses (jump, hang, pinnedSteer, stick hold/release, slide latch, dash/dodge flag), ~4 Hz samples, per-phase velocity min/max/mean, and settle miss (`missXY`, `missZ`, `releaseMiss`, `onPad`). `goto` logs which arm it picked (`recast`, `recastAirDive` for Recast off-mesh AirDive links).

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

Greybox stamps `ACLTaskMarker` actors when it builds. Same ids across maps; each layout re-stamps. Markers carry **tags** (spawn, space) from the map catalog. A marker may have more than one tag. Game modes require tags, not raw ids — [design/game-modes.md](../../../design/game-modes.md).

PvP minimum: `spawn_red`, `spawn_blue`, `court_center`, `hide_center_lee`, `menhir_0`…`menhir_7`, `menhir_*_approach`, `cover_west_cut`, `cover_east_cut`, `shrine_well`, `shrine_tree`, `shrine_heel`, `shrine_cairn`, `edge_lip`, `edge_pad`, `slide_end`, `dash_end`. Tags: `spawn.player.red` / `.blue`, `space.center`, `space.shrine`. Do not use `edge_pad` as a mode objective. Social/raid/practice: `spawn_default`.

`UCLSocialMarkerWidget` is UI chrome, not a nav target.

## Parked (return)

Two-box shrine clash has landed a `modeResult=winner` (both teams ≥1 kill, both `shrine_held`, `mode_winner` in `/state.events`). Guest hitscan is server-applied (`ServerHitscanFire`). FollowAlert / `cause=execution` still fails the match — dump events and stop; do not resume a fail instance.
