# Two-box lessons

Append a line when a two-window join, travel, or two-agent drive fails in a new way. Method, not a one-hop coordinate.

- Loopback UDP is the virtual socket. Named pipes, TAP adapters, and shared-file “net drivers” are parked. World sim stays `IpNetDriver`.
- Host first. Guest without `-CallingAgentHttpPort=18767` fails 18765/18766 bind (expected). With the flags, guest MCP is 18767/18768. Director `virtualhost` / `ready` / `go` only on the host.
- Drive the guest pawn from guest HTTP or host hub `via` (net-human seat). Do not `mindControl` the guest human from the host lobby — that puppets the server copy and skips the `ServerMove` the two windows exist to test. Do not skip guest `ServerMove` or `PerformMovement` the remote pawn on the listen server.
- Guest connect is `ClientTravel` to `127.0.0.1:7777` (command line map URL, or **Virtual join**). Composer Host/Guest on one window is still same-process.
- Beacon is `FPaths::ProjectDir()/Saved/Calling/loopback-host.json`, not UserDir. Give the guest `-UserDir=` so logs and profiles do not fight; both still read the same beacon.
- `ACLGreyboxFloors` replicates. Any `DOREPLIFETIME` property needs `Replicated` or `ReplicatedUsing` or the **guest fatals** on join. `Layout` uses `OnRep_Layout` so the client stamps pads locally (runtime cubes are not the net channel). Guest Recast must bake too (`RebuildNavigation` on the client after OnRep); catalog `goto` books skip when `navTiles` is 0 on that process.
- Lobby seats live on host GameInstance. Net guests need `EnsureNetHuman` + `BoundController`, GameState `LobbySeats` snaps for the guest composer menu, and Ready/team Server RPCs. Do not map every `PlayerController` to the first human.
- `RestoreBodiesAfterTravel` must not wipe seats that have `BoundController`. After PvP `ServerTravel` (`?listen` kept), `/state.lobby.seats` can dip to 1 until the guest re-joins the new map — wait, then confirm two humans.
- Guest auto-ready on composer `HandleStartingNewPlayer`. Host still Ready+Go. Min players 2. Do not auto-Go.
- Default `/state` is the host pawn. Two agents: host `/state?seat=` per host cursor; guest MCP samples 18767 for local prediction. Replication proof is always host 18765. Never one probe for two bodies.
- Ring verify (`dl-circle-run`) is **one** process + hub guest. Two-box is a different bring-up; do not replace ring with listen+ClientTravel unless the user asked for two windows.
