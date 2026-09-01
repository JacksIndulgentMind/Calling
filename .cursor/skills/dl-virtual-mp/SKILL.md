---
name: dl-virtual-mp
description: >-
  Bring up two Calling Unreal windows on one PC (listen host + loopback join)
  and start driving both pawns as agents. Use when the user asks for virtual
  multiplayer, two instances, two-box, virtual host/join, loopback 127.0.0.1:7777,
  or two Cursor agents in one PvP match. Guest MCP 18767 or host hub connectMode=proxy.
---

# Virtual multiplayer (two process)

Same-PC two-box is Unreal **listen host** plus **ClientTravel** to `127.0.0.1`. Loopback UDP **is** the virtual socket. Do not add a named-pipe / TAP / shared-file NetDriver. No Engine patches.

Hub vs director vs BotBooks: [dl-agent-control](../dl-agent-control/SKILL.md). Drive: [dl-agent-nav](../dl-agent-nav/SKILL.md). How it is wired: [Docs/VirtualMp.md](../../../Docs/VirtualMp.md). Component diagram: [Docs/HubIngress.puml](../../../Docs/HubIngress.puml). Lessons: [lessons.md](lessons.md).

Guest CharacterMovement must originate on the **guest** (`ServerMove`). Do not mind-control the guest human from the host hub (that puppets the server copy). Do not skip guest `ServerMove` or server-sim the remote pawn.

## Ports (do not mix)

| Port | Who binds | Role |
|------|-----------|------|
| **7777 UDP** | Host listen, client connects | World replication. Same port on localhost is correct. |
| **18765 HTTP** | Host | MCP / director / `GET /state` / `POST /hub` |
| **18766 WS** | Host | Session hub |
| **18767 HTTP** | Guest (override) | Second MCP / guest `/hub` / guest `/state` (local prediction) |
| **18768 WS** | Guest (override) | Guest session hub |

Default two-box: guest cmdline `-CallingAgentHttpPort=18767 -CallingSessionHubPort=18768` (after Game.ini). Second Cursor MCP: `CALLING_AGENT_HTTP=http://127.0.0.1:18767` or `DL_AGENT_PORT=18767`. Each window has its own **`instanceId`**. Each MCP process has its own **`agentId`**. Send `agentId` on hub/director/state; do not reuse one MCP against both ports without knowing which `instanceId` you bound.

If the guest is launched **without** those flags, 18765/18766 bind fail is expected. Director `virtualhost` / `ready` / `go` only on the **host**. Do not Compose PvP as standalone on both windows. Do not run two `?listen` hosts.

## Social two-box

Login lands in **private** Social (no listen). Overlay **Lobby** is the social composer: Public reloads a listen Social; Join is IP:port (`socialjoin`, not hub `join`). Save as default; Join saves host+port with a private|public fallback. Raid/Practice/Composer **drop** social listen. Script:

```
Scripts/dl-rebuild.ps1 -Activity social
Scripts/dl-verify-social-two-box.ps1
```

Director on host 18765 / guest 18767: `lobbytab`, `socialaudience` `{kind:public}`, `socialjoin` `{host,port}`, `savedefaultsocial` `{kind,host,port,fallback}`. `/state.lobby.access` is `private` when closed and not listening; `listening` / `netMode`; `socialDefault`; events `join_unavailable`.

## Bring up (verified)

1. Stop extra UnrealEditor processes. Host first:

```
Scripts/dl-rebuild.ps1 -Activity composer
```

2. Host: `POST http://127.0.0.1:18765/director` `{"action":"virtualhost"}` (or composer/I-menu **Virtual host**). Wait until `Saved/Calling/loopback-host.json` has `"net":"listen"`, `connect` `127.0.0.1:7777`, and the host **`instanceId`**.

3. Guest (second process, own UserDir so logs/profiles do not fight). Beacon stays under **project** `Saved/Calling/` even when UserDir differs:

```
UnrealEditor.exe Calling.uproject 127.0.0.1:7777 -game -WINDOWED -UserDir=<Calling>/Saved/CallingClient2 -CallingAgentHttpPort=18767 -CallingSessionHubPort=18768
```

Or from the guest I-menu: **Virtual join**, combo **Loopback (127.0.0.1)** (or **Beacon**), still pass the port flags on that process.

4. Host: `GET /state` until `lobby.seats >= 2`. Guest auto-readies. Host `{"action":"ready"}` then `{"action":"go"}`. Wait `scene=pvp`.

Pass: two viewports; host Red `x ≈ -6380`; guest Blue `x ≈ 6380`; `lobby.seatList` two `kind: human`. Movement replicates. `GET :18767/state` responds on the guest. Composer invoice `gameMode` is `shrine_clash`. After Go, `/state.modeResult` starts `in_progress`; play until `winner` or `fail` (0 kills on either side is fail). Drive `shrine_clash_fight` and splice to `/state.liveShrine`.

UI (config `bShowLoopbackJoin=true`, off in shipping): composer + I-menu **Virtual host** / **Virtual join**. Director aliases `virtualhost` / `virtualjoin`. Debug crumbs: `Saved/Calling/loopback-ipc.log` (join/bind only — not replication).

## Two agents (favor guest port)

Both combat bodies are **net humans** on the **host** lobby (`BoundController`). Replication probe is always **host** `GET http://127.0.0.1:18765/state` (and `?seat=` for a host cursor). Guest `/state` is that client’s prediction.

**Path A (default):** second MCP on 18767.

1. Host 18765: `join` cursor A → `mindControl` **host human** → `appendBotBook` on A with `"intendedTarget":"localHuman"`. `GET /state?seat=<A>`.
2. Guest 18767: `appendBotBook` with `"intendedTarget":"localHuman"`. Guest hub binds a local cursor to the possessed pawn. No host `mindControl` of Blue.

**Path B (one MCP):** only host 18765. Hub HTTP/WS **`Calling-Connect-Mode: proxy`** (JSON `connectMode: proxy`). Optional **`Calling-Target-Instance`** = guest `instanceId` from host `/state` seat rows; if omitted and there is one listen guest, that PC is the target. Host forwards into the guest ingress — same as connecting on 18767. Then `join` / `appendBotBook` with `"intendedTarget":"localHuman"` (the guest executes it). Sample that cursor on **18767**. Host 18765 `?seat=` cannot see the guest-minted cursor; for replicated Blue loc use **this lobby’s** Guest human id (`boundLocal: false` on host `seatList`), not a GUID from a previous session. MCP env: `CALLING_CONNECT_MODE=proxy`. Drive against the other window’s human is `ok: false` `remote_player_pawn`.

Do not MCP `plan` / `sequence` / `intent` / raw `goto`. Two seats must not share one `/state?seat=` probe.

**Fail immediately if the pawn does not follow the book.** After `appendBotBook`, `GET /state?seat=` once. Throw `command_not_followed` if `botBook.followAlert` is set, `executionError` is true, `followed` is false, **or** `modeResult=fail` with `modeFailReason` starting `botbook_` — do **not** wait for `failTimeoutSeconds` or shrine rotation. The engine **ends the match** on the first followAlert / `botbook_execution` (`FinishMode` fail). Dump `/state.events` and stop; never resume that instance. Frozen loc with `alive=false` / `lastDeath.valid` is a take-out (`kind`/`source`/`killerName`), not `loc_still` — this PvP path still `WaitAlive`. A pass is `modeResult=winner` with a readable event log (`botbook_append`, `kill`, `shrine_held`, `mode_winner`). Helper: [Scripts/dl-assert-command-followed.ps1](../../../Scripts/dl-assert-command-followed.ps1). Engine codes: unknown leaf/`while:`, missing marker, book on a non-local pawn, `StartGoto` failed, `while:` zeroing Recast stick, goto with no stick, `loc_still`. The earlier two-box walk used a JIT `goto` with no `while:`; `shrine_clash_fight` adds `while: trackFocus, fire`, which used to zero Recast stick each tick (sitting + shooting). That is `botbook_goto_stick_clobber` on tick 1, not a match clock.

## Do not

- Two processes both `?listen`, or both binding 18765 as servers (guest uses 18767).
- Composer Host/Guest on the **guest** window (that is same-process role swap).
- Grow `TileSizeUU`, patch Engine, or move the island to “pass” nav.
- Treat a guest `18765` bind failure as a join failure.
- Poll `/state` at render rate.
- Wait for `modeResult` / `failTimeout` while loc is still spawn and a BotBook is live. That is `command_not_followed` on the first sample (`followAlert` if the rebuilt DLL is up).
- Ignore `botBook.followAlert` / `executionError` / hub `alert: botbook_execution` and keep polling the match clock. `branchBotBook` `cause=execution` is a failed test, not a recover poke. A PowerShell throw while `modeResult` stays `in_progress` is not a match fail — the engine must `FinishMode`. After detection, dump `/state.events` and stop; do not resume.
- Server-simulate the guest pawn so host BotBooks “look like” they moved Blue.
