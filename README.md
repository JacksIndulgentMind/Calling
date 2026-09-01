# Calling

An Unreal Engine **5.8** C++ looter FPS. Movement is the gunfight. Social is home. There is no live service and no online account.

Inspired by Destiny-class shooters, then built as its own game: a handful of verbs, greybox spaces you can actually fight in, and a listen-server so two windows on one PC are already multiplayer. Apache 2.0 — see [LICENSE](LICENSE).

Please fork it and make some awesome games. Suggestions welcome. If this helps you, consider [hitting that coffee button](https://www.buymeacoffee.com/ahkilleux).

**Work in progress.** The verbs and routing are the part we intend to keep.

## Intent

Learn a small set of moves by doing them. No tutorial popups, no “new ability” briefings between activities. If a system needs a tooltip to be usable, the system is wrong.

The fight is **sprint → slide → jump**, and you shoot through all of it. Dodge, dash, and air-dive are close seconds: recover a bad hop, skip cover, chain into the next beat. Legal chains have **no dead frames** — land from a dive and slide the same tick; cancel a slide into a dash; ADS on or off mid-slide. That is the **digital dance**. Shooting is the solver. Class abilities set up or survive a gunfight; they do not replace it.

The HUD shows **state**, not instructions: your shield and health, ammo, ability cool-downs, a noisy motion radar. No world-space enemy health bars. A compact readout appears only while something living is on the barrel.

A **game mode** has to put a kill bar in conflict with a place-to-be. Chase the gunfight and you lose the ground; sit on the ground and you lose the race. Deathmatch with no objective, or a hold with no kill pressure, is not a Calling mode. PvP **shrine clash** is the first: ten final blows versus a rotating shrine. Raid encounters use the same idea — occupy a pad while waves and a boss team press you.

Raids and bosses are hard because of variety, accuracy, numbers, and terrain — not fatter HP. Difficulty is **intellect**: how they track you and how they answer a change. Loot should respect time (the vault is inventory). Pity rolls and player-count trophies are next, not in this build.

## A session

You boot a **local profile**, then land in **Social**. That square is home. Activities are entered from there, and leaving a match always returns you to *your* Social default: private (local, no listen), public/friends/party (listen host), or join by IP. Overlay **Lobby** reloads that instance. Overlay **Director** composes a PvP match, or sends you to raid / practice. When you leave Social for an activity, the listen session is torn first so a guest is not dragged into the raid.

Maps are greybox on purpose: a social square, a three-lane courtyard, an Obelisk raid chain (four chambers, in-map doors), and a practice pad. Same locomotion everywhere.

## What’s in the box

Three classes — **Vanguard**, **Pathfinder**, **Warden** — with JSON kits. Weapons, sights, modifiers, and drop tables are data. Overlay **Armory** lists every make and where it drops (raid, PvP, world, faction vendor) — it is not the vault. Sessions are a listen server on LAN (NULL online subsystem). Same-PC two-window join is first-class. Types and assets use the `CL*` / `Calling` prefix.

Agents are **seats**, not a second pawn API. A human, a Cursor session, or a bot occupies the same seat, possession, and loot rules. Drive a pawn with **BotBooks** (PlantUML on the hub) — same input path as the controller. Localhost HTTP/WebSocket only.

- BotBooks: [Docs/BotBooks.md](Docs/BotBooks.md)
- Two windows: [Docs/VirtualMp.md](Docs/VirtualMp.md)
- Agent recipes: [AGENTS.md](AGENTS.md)

## Where it’s headed

Near-term is the missing *front* on systems that already run: a **vault UI** (browse what you own; Armory already lists what you can chase), fire and melee **anim polish**, placeholder **audio**, and practice-range furniture. Then instance/party chat, party and friends lists, loot pity and trophies by player count, and more authored space on the same verbs. Steam / Deck later. Xbox is parked.

## Requirements

- Unreal Engine 5.8 (`EngineAssociation` in `Calling.uproject`)
- Windows, Visual Studio with the UE 5.8 C++ workload
- Engine plugins: Enhanced Input, Online Subsystem Null

## Build

Stop Unreal Editor first — it locks `UnrealEditor-Calling.dll`. Prefer:

```
Scripts/dl-rebuild.ps1
```

That stops the editor, builds, and relaunches standalone `-game` (Compose PvP by default). `-Activity social` lands in Social; `-Activity none` skips director travel.

Or generate project files from `Calling.uproject` and:

```
Build.bat CallingEditor Win64 Development -Project=<path-to>/Calling.uproject -WaitMutex
UnrealEditor.exe Calling.uproject -game
```

Saves live under `Saved/Calling/Profiles/`. Agent HTTP is localhost `127.0.0.1:18765`.

Sim is **30 Hz** fixed; net **20 Hz**; render unbounded. Input samples every render frame; the sim step consumes it.

## Maps

| Level | GameMode |
|-------|----------|
| `/Game/Maps/CL_Boot` | `ACLBootGameMode` |
| `/Game/Maps/CL_Social` | `ACLSocialGameMode` |
| `/Game/Maps/CL_PvpArena` | `ACLPvpGameMode` |
| `/Game/Maps/CL_Raid_01` … `_04` | `ACLRaidGameMode` |
| `/Game/Maps/CL_Practice` | `ACLPracticeGameMode` |

Composer reuses `/Game/Maps/CL_Social` with `?game=/Script/Calling.CLComposerGameMode`. Map names: `Config/DefaultCalling.ini` (`CLSceneSettings`).

## Config

- `Config/DefaultCalling.ini` — ticks, FOV, movement, combat feel, maps
- `Config/Loot/*.json` — weapon classes, modifier pool, drop tables
- `Config/Classes/AbilityCatalog.json` — ability types and slots
- `Config/Classes/Vanguard.json`, `Pathfinder.json`, `Warden.json` — one bind per slot
- `Config/GameModes/*.json` — shrine clash, obelisk raid, and the next rulesets

Loot rules are local only. Editing saves or config can grant anything; that is intentional for this demo.

## Verify (local Windows + UE 5.8)

Cursor **cloud** agents cannot run Unreal. On a machine with 5.8 installed, after a rebuild:

```
Scripts/dl-verify-dual-composer.ps1 -Sequence ring
Scripts/dl-verify-social-two-box.ps1
```

Circle-run pass: `VERIFY_OK`, `diving=true`, megalith sticks `8/8`. Social two-box: six join/default cases. Details: [AGENTS.md](AGENTS.md) and `.cursor/skills/`.

## Contributing

Fork, experiment, open issues or discussions. Do not commit `Binaries/`, `Intermediate/`, `Saved/`, or editor-generated `.sln` files.
