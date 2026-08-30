# Agents

This is the **Calling** Unreal 5.8 C++ game. Types and assets use the `CL*` prefix. Do not change `AirControl=0.35` or `BaseStrafeSpeed=380` unless the user is chasing a new feel bug.

Cursor **cloud** agents cannot build or run Unreal. The recipes below need a Windows machine with UE 5.8. On cloud, read this file and the skills; do not pretend a circle run passed.

## Skills (read these)

| Skill | When |
|-------|------|
| [`.cursor/skills/dl-agent-control/SKILL.md`](.cursor/skills/dl-agent-control/SKILL.md) | Hub vs director, seats, SeatMotors, join / mind-control / ready / go |
| [`.cursor/skills/dl-agent-nav/SKILL.md`](.cursor/skills/dl-agent-nav/SKILL.md) | BotBooks (`appendBotBook`), markers, probes; loopback `/goto` is debug |
| [`.cursor/skills/dl-circle-run/SKILL.md`](.cursor/skills/dl-circle-run/SKILL.md) | Compose PvP ring verify (`VERIFY_OK`, diving, megalith 8/8) |
| [`.cursor/skills/dl-virtual-mp/SKILL.md`](.cursor/skills/dl-virtual-mp/SKILL.md) | Two Unreal windows on one PC (listen + loopback join); guest MCP on 18767 or host hub `connectMode=proxy` |

Pawn scripting is **SeatMotor + BotBook** (catalog or JIT PlantUML). Anytime an agent drives a pawn — stroll, cover, ring, megalith, one test move — POST hub `appendBotBook` / `branchBotBook`. Read [Docs/BotBooks.md](Docs/BotBooks.md). Landing a point with jump / air-dive / slide / dash (vs Recast `goto`): [Docs/NavAbilities.md](Docs/NavAbilities.md). Recast knobs: [Docs/RecastLinks.md](Docs/RecastLinks.md). Recast policy (TileSize, no Engine patches, no cheat) lives in the parent design repo `.cursor/rules/recast-*.mdc` if this nested git is opened alone. Do not invent MCP `plan` / `sequence` / `intent` / `goto`.

## Spin up

1. Stop Unreal Editor if it is running. It locks `UnrealEditor-Calling.dll`; `Build.bat` while the editor is up will fail. Live Coding will not pick C++ up.
2. From this repo root:

```
Scripts/dl-rebuild.ps1
```

Defaults: standalone `-game`, then Compose PvP **through** host/ready/guest/go into the **pvp** match (waits for `navTiles`). `-Activity composer` stops in the lobby. `-Activity arena` is the solo skip. `-Activity none` skips director. `-Mode editor` opens the editor and requests PIE.

3. If `GET http://127.0.0.1:18765/state` `scene` is `boot`, `POST /director {"action":"enter"}` creates a default Player / Vanguard and travels to Social. Rebuild does this automatically.

Agent HTTP is **localhost only** (`127.0.0.1:18765`; two-box guest **18767**). WebSocket hub is `ws://127.0.0.1:18766` (guest **18768**). Same JSON codec as `POST /hub`. Each Unreal process mints **`instanceId`**; send **`agentId`** on hub/director/state. Drive pawns with `appendBotBook` / `branchBotBook`. After a book starts, `GET /state?seat=` `botBook.followAlert` / `executionError` is an immediate fail — the engine also sets `modeResult=fail` and appends `/state.events`. Dump the event log and stop; do not wait for shrine/`zero_kills`. Loopback `type: plan` / `/intent` / `/sequence` / `/goto` are debug only.

Stdio MCP: `Scripts/dl-agent-mcp/index.mjs` (tools `hub`, `state`, `director`, `boot`; loopback `intent` / `sequence` / `goto` are no-lobby only).

## Overlay vs hub

| Plane | Where | Use for |
|-------|--------|---------|
| Overlay | `POST /director` | I-menu: `open`, `pvp` / `composer`, `host`, `guest`, `ready`, `go`, `virtualhost` / `virtualjoin`, `social`, `raid`, `practice`, `arena` |
| Hub | `POST /hub` and WS 18766 | `join`, `subscribe`, `mindControl`, `setTeam`, `ready`, `go`, `appendBotBook`, `branchBotBook`, `view`. Loopback: `plan`, `goto` |

Director is not the pawn motor. Drive seats through the hub. See the control skill.

## Circle run

With the game already up after rebuild:

```
Scripts/dl-verify-dual-composer.ps1 -Sequence ring
```

Pass: script prints `VERIFY_OK`, `diving=true`, megalith sticks `8/8`. Fail only after retries in the script. Details: circle-run skill.

## Rebuild ritual

Every C++ / link change: stop editor → `Scripts/dl-rebuild.ps1` → leave the game running. Do not leave the editor down after a rebuild if the user still wants to play or verify.
