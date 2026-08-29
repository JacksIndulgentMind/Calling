---
name: dl-virtual-mp
description: >-
  Bring up two Calling Unreal windows on one PC (listen host + loopback join)
  and start driving both pawns as agents. Use when the user asks for virtual
  multiplayer, two instances, two-box, virtual host/join, loopback 127.0.0.1:7777,
  or two Cursor agents in one PvP match.
---

# Virtual multiplayer (two process)

Same-PC two-box is Unreal **listen host** plus **ClientTravel** to `127.0.0.1`. Loopback UDP **is** the virtual socket. Do not add a named-pipe / TAP / shared-file NetDriver. No Engine patches.

Hub vs director vs BotBooks: [dl-agent-control](../dl-agent-control/SKILL.md). Drive: [dl-agent-nav](../dl-agent-nav/SKILL.md). How it is wired: [Docs/VirtualMp.md](../../../Docs/VirtualMp.md). Lessons: [lessons.md](lessons.md).

## Ports (do not mix)

| Port | Who binds | Role |
|------|-----------|------|
| **7777 UDP** | Host listen, client connects | World replication. Same port on localhost is correct. |
| **18765 HTTP** | Host only | MCP / director / `GET /state` / `POST /hub` |
| **18766 WS** | Host only | Session hub |

The second `-game` **will** fail 18765/18766. That is OK. Director and hub stay on the **host** process. Do not Compose PvP as standalone on both windows and expect them to merge. Do not run two `?listen` hosts.

## Bring up (verified)

1. Stop extra UnrealEditor processes. Host first:

```
Scripts/dl-rebuild.ps1 -Activity composer
```

2. Host: `POST http://127.0.0.1:18765/director` `{"action":"virtualhost"}` (or composer/I-menu **Virtual host**). Wait until `Saved/Calling/loopback-host.json` has `"net":"listen"` and `connect` `127.0.0.1:7777`.

3. Guest (second process, own UserDir so logs/profiles do not fight). Beacon stays under **project** `Saved/Calling/` even when UserDir differs:

```
UnrealEditor.exe Calling.uproject 127.0.0.1:7777 -game -WINDOWED -UserDir=<Calling>/Saved/CallingClient2
```

Or from the guest I-menu: **Virtual join**, combo **Loopback (127.0.0.1)** (or **Beacon**). Guest has no director HTTP.

4. Host: `GET /state` until `lobby.seats >= 2`. Guest auto-readies. Host `{"action":"ready"}` then `{"action":"go"}`. Wait `scene=pvp`.

Pass: two viewports; host Red `x ≈ -14500`; guest Blue `x ≈ 14500`; `lobby.seatList` two `kind: human`. Movement replicates.

UI (config `bShowLoopbackJoin=true`, off in shipping): composer + I-menu **Virtual host** / **Virtual join**. Director aliases `virtualhost` / `virtualjoin`. Debug crumbs: `Saved/Calling/loopback-ipc.log` (join/bind only — not replication).

## Two agents (start building here)

Both combat bodies are **net humans** on the **host** lobby (`BoundController`). MCP still talks to the **host** HTTP. The guest window has no `/hub`.

To script both pawns, stay on the host hub:

1. `GET /state` — copy host and guest `seatList[].id` (`host: true` = Red human; the other human is Blue).
2. `join` cursor A (`headless: true`, `kind: cursor`) → `mindControl` **host human** → `appendBotBook` on **A**. `GET /state?seat=<A>`.
3. `join` cursor B → `mindControl` **guest human** → `appendBotBook` on **B**. `GET /state?seat=<B>`.

Mind-control circumvents that seat’s Enhanced Input (guest window will not fight the book). Two seats must not share one `/state?seat=` probe. Do not MCP `plan` / `sequence` / `intent` / raw `goto`.

If mind-control of the bound guest fails, say so and stop — do not invent a second HTTP server on the guest, and do not `join` a fake hub guest *instead of* the net client (that is the old same-process composer path).

## Do not

- Two processes both `?listen`, or both binding 18765 as servers.
- Composer Host/Guest on the **guest** window (that is same-process role swap).
- Grow `TileSizeUU`, patch Engine, or move the island to “pass” nav.
- Treat a guest `18765` bind failure as a join failure.
- Poll `/state` at render rate.
