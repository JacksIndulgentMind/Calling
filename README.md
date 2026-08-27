# Calling

Unreal Engine **5.8** C++ project. Combat, scenes, and greybox live in the in-tree **DestinyLike** plugin.

This repository is the game. Design notes and rebuild/verify scripts live in the private parent repo and are not vendored here.

## Requirements

- Unreal Engine 5.8 (`EngineAssociation` in `Calling.uproject`)
- Windows, Visual Studio with the UE 5.8 C++ workload

## Build

1. Clone this repo.
2. Right-click `Calling.uproject` → Generate Visual Studio project files, or build from the engine:

```
Build.bat CallingEditor Win64 Development -Project=<path-to>/Calling.uproject -WaitMutex
```

3. Open `Calling.uproject` in Unreal Editor, or launch:

```
UnrealEditor.exe Calling.uproject -game
```

Default boot is standalone. Agent HTTP (localhost only) is `127.0.0.1:18765`.

Do not commit `Binaries/`, `Intermediate/`, `Saved/`, or editor-generated `.sln` files.
