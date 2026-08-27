---
name: dl-agent-control
description: >-
  Drive Calling seats via the session hub (join, mind-control, ready, go).
  Use when the user asks to MCP a pawn, compose PvP, host/guest, or
  distinguish director overlay from hub motor.
---

# Control (hub + seats)

Agents are seats. Drive them through the **session hub**, not the MCP loopback motor. Director HTTP is **I-menu overlays only**. Rebuild and spin-up: [AGENTS.md](../../../AGENTS.md). Stroll nav: [dl-agent-nav](../dl-agent-nav/SKILL.md). Ring verify: [dl-circle-run](../dl-circle-run/SKILL.md).

### Control model (do not flatten to flags)

| Mechanism | What it is |
|-----------|------------|
| Seat + playbook | `UCLParticipantSeat` + polymorphic `UCLControllerPlaybook`. Join `kind`: `human` / `algorithmic` / `remoteAgent` / **`cursor`**. MCP defaults to `cursor`. |
| Possession | `UCLPossessionComponent` is the only input owner: own pawn, mind-control, headless. Hub `goto`/`plan`/`GET /state?seat=` use **that seat’s driven pawn**. |
| Mind-control | Headless `join` spawns `ACLCombatPawn` + `ACLSeatController` and possesses it. Second Cursor seat `mindControl`s the **host** pawn so the listen-server human is not Enhanced Input. |
| Composer | Real scene on `CL_Social` (`CLComposerGameMode`), not Social and not “lobby on the arena.” Overlay Compose PvP / director `pvp`. Menu Host/Guest, Ready, Start. |
| Ready / Start | Same lobby calls from UI and hub. Local host Ready+Start; local guest Ready only; remote guest hub `ready`; remote host hub `go`. No auto-Go at min ready. |
| Snapshots | `WantsHubSnapshot`. Cursor: stale only (no MCP filter). `remoteAgent`: also lobbyDirty + lowLookahead. Push never cancels plan/`goto`. Pull: `GET /state?seat=`. |

| Plane | Where | Use for |
|-------|--------|---------|
| Overlay | `POST /director` on **127.0.0.1:18765** | I-menu: `open`, `composer` / `pvp` (Compose PvP), `host`, `guest`, `ready`, `go`/`start`, `social`, `raid`, `practice`, `arena` (solo PvP skip) |
| Hub | `POST /hub` on 18765 **and** `ws://127.0.0.1:18766` (same `FCLHubCommandRegistry`, including `plan`) | `join` (`headless: true`, `kind: cursor`), `subscribe`, `mindControl`, `setTeam`, `ready`, `go`, `plan`, `goto` |
| Loopback codec | `POST /intent` `/sequence` `/goto` | No-lobby tests only. Not the default stroll path. |

Loopback is enforced in game. If `/state` is unreachable, spawn the editor/game then Compose PvP: MCP `boot` (or Shell `UnrealEditor.exe` + `POST /director {"action":"pvp"}`). Default boot is standalone `-game`. `mode: editor` opens UnrealEditor and requests PIE.

If `GET /state` `scene` is **`boot`**, there is no locked-in profile (saves are `Saved/Calling/Profiles/`). `POST /director {"action":"enter"}` creates a default **Player** / Vanguard, locks it, and travels to Social. Rebuild and verify do this automatically.

**Rebuild (required):** every C++ / link change is stop editor → build → relaunch. Never `Build.bat` while `UnrealEditor` is running — it locks `UnrealEditor-Calling.dll`.

```
Scripts/dl-rebuild.ps1
```

Defaults: standalone `-game`, then `POST /director {"action":"pvp"}` → **composer** (SocialSquare), not the arena. `-Activity none` skips director. `-Activity arena` is the old solo courtyard skip. After a rebuild, do not leave the editor down.

Composer has an invoice and **no combat gate** (walk, join, pick teams). Host **Go** queues countdown only if `ready >= minPlayers` (composer default **2**). Ready toggles until Go; after Go, ready is locked. Unready before Go cancels a premature countdown. Direct `arena` / overlay Launch PvP still uses a gated min-1 skip.

`GET /state?seat=<id>` samples **that seat’s driven pawn**. Two seats must not share one probe. `/state.lobby.unlocked` is true in composer (no gate). After composer launch, PvP consumes the invoice roster, spawns by team, and skips a second Ready round.

## Hub (default stroll)

Every hub body includes `seatId` once you have one (join returns it). Same JSON on WebSocket 18766.

| `type` | Body | Notes |
|--------|------|--------|
| `join` | `{ "displayName", "headless": true, "kind": "cursor" }` | Cursor playbook (MCP default). `kind: remoteAgent` for a lightweight LLM. Spawns a combat body and mind-controls it |
| `subscribe` | `{ "seatId" }` | Bind this WebSocket to a seat already joined over HTTP |
| `mindControl` | `{ "seatId", "targetSeatId" }` | Drive another seat’s pawn (host pawn: Cursor is not Enhanced Input) |
| `setTeam` | `{ "seatId", "team": "red"\|"blue"\|"unassigned" }` | Own seat, or host any seat |
| `ready` | `{ "seatId", "ready": true\|false }` | Toggle until Go queues start |
| `go` | `{}` | Host. Queues countdown if `ready >= min`. Composer then stamps roster and travels to PvP |
| `plan` | `{ "seatId", "steps", "replaceFrom" }` | Timed motor on **that** driven pawn (NetHz) |
| `goto` | `{ "seatId", "x", "y", "z?" }` | Recast on **that** driven pawn |
| `view` | `{ "seatId" }` | Blend the listen-server camera onto that seat’s driven pawn (~0.45s ease). Empty `seatId` restores the host pawn |

Headless join of the host: join, then `mindControl` the host seat so the agent drives the human pawn.

Hub **push** (`{type:"state", reason}`) is a playbook virtual, not a Cursor-side filter. `UCLCursorPlaybook` holds and only pushes `reason: stale` (`PlanStaleSeconds`, default 3) when there is no live plan/`goto`. `UCLRemoteAgentPlaybook` also gets `lobbyDirty` and `lowLookahead`. A push never cancels plan/`goto`. `GET /state?seat=` remains pull when you choose to sample.

## Overlay vs net (ready / start)

Ready and Start are the same lobby calls from two places:

| Who | Ready | Start match |
|-----|--------|-------------|
| Local host | Composer menu **Ready**, overlay Ready, or `POST /director {"action":"ready"}` | Composer menu **Start match**, overlay Host Go, or `POST /director {"action":"go"}` |
| Local guest | Composer menu **Ready** (own seat) | Hidden — wait for host |
| Remote guest | Hub `{ "type":"ready", "seatId", "ready" }` | Rejected (`host_only`) |
| Remote host | Hub `ready` on host seat | Hub `{ "type":"go", "seatId": "<host>" }` |

Composer menu is on the composer scene (not Social): Host/Guest, Ready, Start, team. Confirm `GET /state` `scene` is **`composer`** and `lobby.localHost` before Ready. If you still see `social`, travel failed — wait until not `boot`, then `pvp` again.

Do **not** auto-Go when `ready >= min`. Host Start is required.

## Loopback codec (no lobby)

Keep `/intent` `/sequence` `/goto` for Practice or a scene with no invoice. Game sim is **30 Hz**. Queue timed sticks **in game**. `POST /intent` (including `{}`) **cancels** the HTTP sequence/goto singleton — it does not cancel a hub seat’s goto.

Do **not** poll HTTP at render rate. Sample `/state` at most ~10 Hz.

MCP tools: `hub`, `state` (`seat` query), `director`, `boot`. `hold`/`sequence`/`goto`/`intent` are the no-lobby codec. `boot` waits for HTTP.

| Field | Kind | Notes |
|-------|------|--------|
| `move.x` / `move.y` | hold | **x = strafe**, **y = forward** (controller yaw) |
| `look.yaw` / `look.pitch` | pulse | Degrees, one-shot **delta** at step start. Clamped per tick to agent max yaw/pitch rate (420 / 280 deg/s). Does not cap human mouse. |
| `look.yawAbs` / `look.pitchAbs` | goal | World control rotation **goal**, not a snap. Character tick slews at 420 / 280 deg/s. Prefer `lookAtSeat` when tracking a pawn. |
| `lookAtSeat` | hold | Seat id. Sticky world point = last accepted target location. Ego-motion updates look every tick at the turn cap. If the target pawn moves more than ~40 cm, wait **100 ms** then refresh the sticky. Fire/look during that wait still aims at the old point. |
| `sprint` `crouch` `ads` `fire` | hold | JSON **booleans**. PowerShell `ConvertTo-Json` can emit the wrong type — prefer raw JSON strings |
| `jump` `dodge` `dash` `reload` `swap` `slide` `airDive` | pulse | Once at step start / intent. `slide` calls `RequestSlide()`. `airDive` is airborne-only (ground is a no-op, no cooldown) |
| `weapon` | pulse | `"primary"` or `"special"` — dedicated slot, not the Swap toggle |
| `sight` | pulse | `"iron"` / `"red_dot"` / `"scope"` — any sight on the equipped gun |

Unreal yaw **0 = +X**. `/state` also has `air`, `diving`, `sliding`, `health`, `shield`.

If `/state` says `no_local_pawn` on a live scene, `POST /respawn` once (void / KillZ) for the listen-server human only. Greybox also auto-teleports below rescue Z. If `/respawn` 404s, PIE is the old binary or not running — stop.
