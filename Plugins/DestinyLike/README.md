# DestinyLike

In-tree runtime plugin for **Calling** (Unreal Engine 5.8). Local profiles/vault, listen-server lobbies, scene routers, combat, and raid AI.

This plugin ships inside `Calling.uproject`. Enable **DestinyLike**, **Online Subsystem Null**, and **Enhanced Input**. Game instance is `UDLGameInstance`.

## Maps

| Level asset path | Intended GameMode |
|------------------|-------------------|
| `/Game/Maps/DL_Boot` | `ADLBootGameMode` |
| `/Game/Maps/DL_Social` | `ADLSocialGameMode` |
| `/Game/Maps/DL_PvpArena` | `ADLPvpGameMode` |
| `/Game/Maps/DL_Raid_01` … `_04` | `ADLRaidGameMode` |
| `/Game/Maps/DL_Practice` | `ADLPracticeGameMode` |

Composer reuses `/Game/Maps/DL_Social` with `?game=/Script/DestinyLike.DLComposerGameMode`. Map names are in `Config/DefaultDestinyLike.ini` under `DLSceneSettings`.

## Tick model

| Layer | Default | Notes |
|-------|---------|-------|
| Render | unbounded | Remote pawns interpolate |
| Game sim | 30 Hz fixed | `FDLTickClock` / `UDLTickSubsystem` |
| Net | 20 Hz | Listen-server peer state |

Input is sampled every render frame into accumulators; the fixed sim step consumes them.

## Identity / networking

- No online account. Local profile JSON under `Saved/DestinyLike/Profiles/`.
- Vault is the only inventory; drops go there and raise earn badges.
- Sessions: listen server + LAN / NULL OSS. Host is authority.

## Config

- `Config/DefaultDestinyLike.ini` — ticks, FOV, movement, combat feel, maps
- `Config/Loot/*.json` — weapon classes, modifier pool, drop tables
- `Config/Classes/AbilityCatalog.json` — ability types and slots
- `Config/Classes/Vanguard.json`, `Pathfinder.json`, `Warden.json` — one bind per slot

Loot rules are enforced locally only. Editing saves/config can grant anything; that is intentional for this demo.

Agent HTTP (localhost): `127.0.0.1:18765`.
