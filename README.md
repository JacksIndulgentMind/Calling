# Calling

This is an FPS looter-shooter **game starter** inspired by Destiny. Please fork it and make some awesome games. If you have suggestions, leave comments. If this helps you, consider [hitting that coffee button](https://www.buymeacoffee.com/ahkilleux).

**Work in progress.** We will keep improving it over time.

Calling is an Unreal Engine **5.8** C++ project: gunplay, movement, greybox maps, a listen-server session hub, and a localhost agent API so tools (and Cursor) can drive pawns the same way a player does. Apache 2.0 — see [LICENSE](LICENSE).

## Overview

You boot into a local profile, land in a social square, then compose a match or jump into practice / raid placeholders. Combat is a **digital dance**: slide, dodge, dash, and air-dive chain without dead frames. Loot is data-driven JSON (weapon classes, sights, modifiers). There is no online account and no live service — saves live under `Saved/Calling/Profiles/`.

Agents are **seats**, not extra HTTP pawn ids. `POST /hub` on `127.0.0.1:18765` and WebSocket `ws://127.0.0.1:18766` share one codec. `POST /director` is the I-menu overlay only (compose PvP, ready, go). The game rejects non-loopback HTTP.

Types and assets use the `CL*` / `Calling` prefix. Do not retune `AirControl=0.35` or `BaseStrafeSpeed=380` unless you are chasing a new feel bug.

## Requirements

- Unreal Engine 5.8 (`EngineAssociation` in `Calling.uproject`)
- Windows, Visual Studio with the UE 5.8 C++ workload
- Engine plugins: Enhanced Input, Online Subsystem Null

## Build

1. Clone this repo.
2. Right-click `Calling.uproject` and Generate Visual Studio project files, or build from the engine:

```
Build.bat CallingEditor Win64 Development -Project=<path-to>/Calling.uproject -WaitMutex
```

Never run `Build.bat` while Unreal Editor is open — it locks `UnrealEditor-Calling.dll`. Prefer:

```
Scripts/dl-rebuild.ps1
```

That stops the editor, builds, and relaunches standalone `-game` (then Compose PvP by default). `-Activity none` skips director travel.

3. Open `Calling.uproject` in Unreal Editor, or launch:

```
UnrealEditor.exe Calling.uproject -game
```

Default boot is standalone. Agent HTTP (localhost only) is `127.0.0.1:18765`.

## Maps

| Level asset path | Intended GameMode |
|------------------|-------------------|
| `/Game/Maps/CL_Boot` | `ACLBootGameMode` |
| `/Game/Maps/CL_Social` | `ACLSocialGameMode` |
| `/Game/Maps/CL_PvpArena` | `ACLPvpGameMode` |
| `/Game/Maps/CL_Raid_01` … `_04` | `ACLRaidGameMode` |
| `/Game/Maps/CL_Practice` | `ACLPracticeGameMode` |

Composer reuses `/Game/Maps/CL_Social` with `?game=/Script/Calling.CLComposerGameMode`. Map names are in `Config/DefaultCalling.ini` under `CLSceneSettings`.

## Tick model

| Layer | Default | Notes |
|-------|---------|-------|
| Render | unbounded | Remote pawns interpolate |
| Game sim | 30 Hz fixed | `FCLTickClock` / `UCLTickSubsystem` |
| Net | 20 Hz | Listen-server peer state |

Input is sampled every render frame into accumulators; the fixed sim step consumes them.

## Identity / networking

- No online account. Local profile JSON under `Saved/Calling/Profiles/`.
- Vault is the only inventory; drops go there and raise earn badges.
- Sessions: listen server + LAN / NULL OSS. Host is authority.

## Config

- `Config/DefaultCalling.ini` — ticks, FOV, movement, combat feel, maps
- `Config/Loot/*.json` — weapon classes, modifier pool, drop tables
- `Config/Classes/AbilityCatalog.json` — ability types and slots
- `Config/Classes/Vanguard.json`, `Pathfinder.json`, `Warden.json` — one bind per slot

Loot rules are enforced locally only. Editing saves/config can grant anything; that is intentional for this demo.

## Agent circle run (local Windows + UE 5.8)

After a rebuild, with the game up:

```
Scripts/dl-verify-dual-composer.ps1 -Sequence ring
```

Expect `VERIFY_OK`, `diving=true`, and megalith sticks `8/8`. Radar-only: `-Sequence radar`.

Cursor **cloud** agents cannot run Unreal. This recipe is for a Windows machine with UE 5.8 installed. See [AGENTS.md](AGENTS.md) and `.cursor/skills/` (`dl-agent-control`, `dl-agent-nav`, `dl-circle-run`).

## Contributing

Fork, experiment, open issues or discussions with suggestions. Do not commit `Binaries/`, `Intermediate/`, `Saved/`, or editor-generated `.sln` files.
