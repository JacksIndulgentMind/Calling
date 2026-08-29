# Virtual multiplayer (same-PC listen)

Two Unreal processes on one machine: **listen server** + **client** over loopback UDP. Not a second net stack. Design spec: [design/agents-and-lobbies.md](../../../design/agents-and-lobbies.md) (parent repo). Agent bring-up: [`.cursor/skills/dl-virtual-mp/SKILL.md`](../.cursor/skills/dl-virtual-mp/SKILL.md).

## What shipped

| Piece | Role |
|-------|------|
| `IpNetDriver` listen on **7777** | Host `?listen`. Guest `ClientTravel` to `127.0.0.1:7777`. |
| `CLLoopbackJoin` | Config `bShowLoopbackJoin` / `LoopbackConnect`. Beacon JSON + `loopback-ipc.log` under **project** `Saved/Calling/`. |
| `UCLSessionSubsystem::StartComposerLoopbackHost` | Composer map + `?game=/Script/Calling.CLComposerGameMode` + `?listen`. Director `virtualhost`. |
| `UCLSessionSubsystem::JoinLoopback` | Resolve combo Loopback vs Beacon, then `ClientTravel`. Refuses if already listen. Director `virtualjoin` (host window only). |
| `UCLSeatRegistry::EnsureNetHuman` | One human seat per `PlayerController`. Host default Red; net guest Blue. `FindForController` matches `BoundController` first. |
| `ACLGameStateBase` lobby snaps | Guest composer menu has no GameInstance seats; it reads replicated `LobbySeats` / ready / min / queued. |
| `ACLPlayerController` Server RPCs | Guest Ready / team. `EnsureComposerMenu` on the **local** client (server cannot create the guest viewport widget). |
| `UCLSceneRouter::SoftTravel` | Appends `?listen` when `NM_ListenServer` so PvP travel keeps both connections. |
| `UCLTravelCoordinator::RestoreBodiesAfterTravel` | Skips seats with `BoundController` (net humans keep their pawns). First human in roster is host; other humans are guests. |
| `ACLGreyboxFloors::Layout` | `ReplicatedUsing = OnRep_Layout`. Missing `Replicated*` fatals the guest on join. |

Parked: named pipe / AF_UNIX NetDriver; shared file as the replication channel; custom NetDriver over WS; dedicated/headless host.

## Why composer listen (not two Social hosts)

Same-process composer Host/Guest is still the ring-verify path. A second **window** is a real Unreal net client: `PostLogin` / `HandleStartingNewPlayer` bind a human seat to that `PlayerController`. `ChoosePlayerStart` uses that seat’s team (Red west, Blue east).

`Map?listen` without `game=` would load Social GameMode via map prefixes. Composer listen URL must include `game=/Script/Calling.CLComposerGameMode`.

## Guest MCP (ports + via)

`UCLAgentBridgeSubsystem` / `UCLSessionHub` read `AgentHttpPort` / `SessionHubPort`, then cmdline `-CallingAgentHttpPort=` / `-CallingSessionHubPort=`. Default two-box guest is **18767 / 18768**. Guest hub `Dispatch` binds a local cursor to the possessed pawn. Host `POST /hub` with `via` (net-human seat id) Client-RPCs the same JSON to that `ACLPlayerController`. Director session actions on the guest return `host_only`. Replication probe stays host 18765. See the skill.
