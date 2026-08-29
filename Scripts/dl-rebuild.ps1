# Stop UnrealEditor, build CallingEditor, relaunch. The editor locks
# UnrealEditor-Calling.dll; never Build.bat while it is running.
#
# Default -Activity pvp: Compose PvP, then auto host/ready/guest/go into the
# match (do not leave agents sitting in composer). Use -Activity composer to
# stop in the lobby; -Activity arena for solo skip; -Activity none for no director.
param(
	[ValidateSet("game", "editor")]
	[string]$Mode = "game",
	[string]$Activity = "pvp",
	[int]$WaitSeconds = 90
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$UProject = Join-Path $RepoRoot "Calling.uproject"
$BuildBat = "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat"
$PlayPy = Join-Path $PSScriptRoot "dl-editor-play.py"

function Get-State {
	param([string]$Seat = "")
	$uri = if ($Seat) { "http://127.0.0.1:18765/state?seat=$Seat" } else { "http://127.0.0.1:18765/state" }
	return (Invoke-WebRequest -Uri $uri -UseBasicParsing -TimeoutSec 3).Content | ConvertFrom-Json
}

function Invoke-Director([string]$Action) {
	$body = '{"action":"' + $Action + '"}'
	return (Invoke-WebRequest -Uri "http://127.0.0.1:18765/director" -Method POST -Body $body -ContentType "application/json; charset=utf-8" -UseBasicParsing -TimeoutSec 20).Content | ConvertFrom-Json
}

function Invoke-Hub($Obj) {
	$body = $Obj | ConvertTo-Json -Compress -Depth 8
	return (Invoke-WebRequest -Uri "http://127.0.0.1:18765/hub" -Method POST -Body $body -ContentType "application/json; charset=utf-8" -UseBasicParsing -TimeoutSec 20).Content | ConvertFrom-Json
}

function Wait-Scene([string]$Want, [int]$Seconds) {
	$deadline = (Get-Date).AddSeconds($Seconds)
	$st = $null
	while ((Get-Date) -lt $deadline) {
		try { $st = Get-State } catch { Start-Sleep -Milliseconds 400; continue }
		if ([string]$st.scene -eq $Want) { return $st }
		Start-Sleep -Milliseconds 400
	}
	return $st
}

function Wait-NavTiles([int]$Seconds) {
	$deadline = (Get-Date).AddSeconds($Seconds)
	$st = $null
	while ((Get-Date) -lt $deadline) {
		try { $st = Get-State } catch { Start-Sleep -Milliseconds 400; continue }
		if ([int]$st.navTiles -gt 0) { return $st }
		Start-Sleep -Milliseconds 400
	}
	return $st
}

# Race Compose PvP lobby into the match (minPlayers=2).
function Complete-ComposerIntoMatch {
	Write-Output "composer: host + ready + guest + go"
	Invoke-Director "host" | Out-Null
	$st = Get-State
	if ($st.lobby.localHost -ne $true) {
		throw "composer localHost false after host"
	}
	Invoke-Director "ready" | Out-Null
	Start-Sleep -Milliseconds 200
	$st = Get-State
	$hostSeat = $st.lobby.seatList | Where-Object { $_.host -eq $true } | Select-Object -First 1
	if (-not $hostSeat -or $hostSeat.ready -ne $true) {
		throw "composer host ready did not stick"
	}

	$joinB = Invoke-Hub @{ type = "join"; displayName = "rebuildB"; headless = $true; kind = "cursor" }
	$seatB = $joinB.seatId
	if (-not $seatB) { throw "composer guest join failed" }
	Invoke-Hub @{ type = "setTeam"; seatId = $seatB; team = "blue" } | Out-Null
	Invoke-Hub @{ type = "ready"; seatId = $seatB; ready = $true } | Out-Null
	Start-Sleep -Milliseconds 200
	$st = Get-State
	if ([int]$st.lobby.ready -lt 2) {
		throw "composer expected 2 ready, got $($st.lobby.ready)"
	}

	Invoke-Director "go" | Out-Null
	$st = Wait-Scene "pvp" 30
	if ([string]$st.scene -ne "pvp") {
		throw "composer go did not reach pvp scene=$($st.scene)"
	}
	$st = Wait-NavTiles 45
	Write-Output ("pvp scene=$($st.scene) navTiles=$($st.navTiles) seats=$($st.lobby.seats)")
	return $st
}

Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue | ForEach-Object {
	Write-Output "stopping UnrealEditor pid=$($_.Id)"
	Stop-Process -Id $_.Id -Force
}
$deadline = (Get-Date).AddSeconds(20)
while ((Get-Date) -lt $deadline -and (Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue)) {
	Start-Sleep -Milliseconds 400
}
if (Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue) {
	throw "UnrealEditor still running; DLL would stay locked"
}

& $BuildBat CallingEditor Win64 Development "-Project=$UProject" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) {
	throw "build failed exit=$LASTEXITCODE"
}

$launchArgs = if ($Mode -eq "editor") {
	@($UProject, "-ExecutePythonScript=$PlayPy")
} else {
	@($UProject, "-game", "-WINDOWED", "-ResX=1600", "-ResY=900", "-NOSPLASH")
}
Start-Process -FilePath $Editor -ArgumentList $launchArgs
Write-Output "launched mode=$Mode"

if ($Activity -eq "none") {
	return
}

$httpDeadline = (Get-Date).AddSeconds([Math]::Max(15, $WaitSeconds))
$up = $false
$scene = "down"
while ((Get-Date) -lt $httpDeadline) {
	try {
		$st = Get-State
		$scene = [string]$st.scene
		if ($scene -and $scene -ne "boot") {
			$up = $true
			break
		}
		$up = $true
	} catch {
		Start-Sleep -Seconds 2
		continue
	}
	Start-Sleep -Seconds 1
}
if (-not $up) {
	Write-Output "http_timeout; director skipped"
	return
}

if ($scene -eq "boot") {
	Write-Output "boot: creating default profile via /director"
	Invoke-Director "enter" | Out-Null
	$leaveBoot = (Get-Date).AddSeconds(45)
	$stBoot = $null
	while ((Get-Date) -lt $leaveBoot) {
		try { $stBoot = Get-State } catch { Start-Sleep -Milliseconds 400; continue }
		$scene = [string]$stBoot.scene
		if ($scene -and $scene -ne "boot") { break }
		Start-Sleep -Milliseconds 400
	}
	if (-not $scene -or $scene -eq "boot") {
		Write-Output "boot_timeout; director skipped"
		return
	}
}

$act = $Activity.ToLowerInvariant()
if ($act -eq "composer") {
	$r = Invoke-Director "pvp"
	Write-Output ($r | ConvertTo-Json -Compress)
	$st = Wait-Scene "composer" 45
	Write-Output ("stopped at composer scene=$($st.scene)")
	return
}

if ($act -eq "arena") {
	$r = Invoke-Director "arena"
	Write-Output ($r | ConvertTo-Json -Compress)
	$st = Wait-Scene "pvp" 60
	if ([string]$st.scene -ne "pvp") { throw "arena did not reach pvp scene=$($st.scene)" }
	$st = Wait-NavTiles 45
	Write-Output ("pvp scene=$($st.scene) navTiles=$($st.navTiles)")
	return
}

if ($act -eq "pvp") {
	$r = Invoke-Director "pvp"
	Write-Output ($r | ConvertTo-Json -Compress)
	$st = Wait-Scene "composer" 45
	if ([string]$st.scene -ne "composer") {
		if ([string]$st.scene -eq "pvp") {
			$st = Wait-NavTiles 45
			Write-Output ("already pvp navTiles=$($st.navTiles)")
			return
		}
		throw "expected composer after pvp, got $($st.scene)"
	}
	Complete-ComposerIntoMatch
	return
}

# raid / practice / social / other: fire director and wait for matching scene when possible
$r = Invoke-Director $act
Write-Output ($r | ConvertTo-Json -Compress)
$st = Wait-Scene $act 45
Write-Output ("scene=$($st.scene)")
