param(
	[int]$HostPort = 18765,
	[int]$GuestPort = 18767,
	[int]$WaitSeconds = 90
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$UProject = Join-Path $RepoRoot "Calling.uproject"
$GuestUserDir = Join-Path $RepoRoot "Saved\CallingClient2"

function Get-StatePort {
	param([int]$Port, [string]$Seat = "")
	$uri = if ($Seat) { "http://127.0.0.1:$Port/state?seat=$Seat" } else { "http://127.0.0.1:$Port/state" }
	return (Invoke-WebRequest -Uri $uri -UseBasicParsing -TimeoutSec 3).Content | ConvertFrom-Json
}

function Invoke-DirectorPort {
	param([int]$Port, $Obj)
	$body = $Obj | ConvertTo-Json -Compress -Depth 6
	return (Invoke-WebRequest -Uri "http://127.0.0.1:$Port/director" -Method POST -Body $body -ContentType "application/json; charset=utf-8" -UseBasicParsing -TimeoutSec 20).Content | ConvertFrom-Json
}

function Wait-ScenePort {
	param([int]$Port, [string]$Want, [int]$Seconds)
	$deadline = (Get-Date).AddSeconds($Seconds)
	$st = $null
	while ((Get-Date) -lt $deadline) {
		try { $st = Get-StatePort -Port $Port } catch { Start-Sleep -Milliseconds 400; continue }
		if ([string]$st.scene -eq $Want) { return $st }
		Start-Sleep -Milliseconds 400
	}
	return $st
}

function Wait-Pred {
	param([int]$Port, [scriptblock]$Pred, [int]$Seconds, [string]$Label)
	$deadline = (Get-Date).AddSeconds($Seconds)
	$st = $null
	while ((Get-Date) -lt $deadline) {
		try { $st = Get-StatePort -Port $Port } catch { Start-Sleep -Milliseconds 400; continue }
		if (& $Pred $st) { return $st }
		Start-Sleep -Milliseconds 400
	}
	throw "$Label timeout scene=$($st.scene) access=$($st.lobby.access) listening=$($st.lobby.listening) net=$($st.lobby.netMode) seats=$($st.lobby.seats)"
}

function Assert-Ok {
	param($Resp, [string]$Label)
	if ($null -eq $Resp) { throw "$Label null director response" }
	if ($Resp.ok -eq $false) { throw "$Label director ok=false error=$($Resp.error)" }
}

function Ensure-Guest {
	try {
		Get-StatePort -Port $GuestPort | Out-Null
		Write-Output "guest already up on $GuestPort"
		return
	} catch {
		Write-Output "launching guest UserDir=$GuestUserDir"
	}
	New-Item -ItemType Directory -Force -Path $GuestUserDir | Out-Null
	$guestProfiles = Join-Path $GuestUserDir "Saved\Calling\Profiles"
	if (Test-Path $guestProfiles) {
		Get-ChildItem $guestProfiles -Filter "*.json" | ForEach-Object {
			$raw = [System.IO.File]::ReadAllText($_.FullName)
			$updated = [regex]::Replace($raw, '("kind"\s*:\s*")join(")', '${1}private${2}')
			if ($updated -ne $raw) {
				[System.IO.File]::WriteAllText($_.FullName, $updated)
			}
		}
	}
	$args = @(
		$UProject, "-game", "-WINDOWED", "-ResX=1280", "-ResY=720", "-NOSPLASH",
		"-UserDir=$GuestUserDir",
		"-CallingAgentHttpPort=$GuestPort",
		"-CallingSessionHubPort=18768"
	)
	Start-Process -FilePath $Editor -ArgumentList $args | Out-Null
	$deadline = (Get-Date).AddSeconds([Math]::Max(30, $WaitSeconds))
	while ((Get-Date) -lt $deadline) {
		try {
			Get-StatePort -Port $GuestPort | Out-Null
			return
		} catch {
			Start-Sleep -Seconds 2
		}
	}
	throw "guest http_timeout on $GuestPort"
}

function Enter-IfBoot([int]$Port) {
	$st = $null
	try { $st = Get-StatePort -Port $Port } catch { return }
	if ([string]$st.scene -eq "boot") {
		Write-Output "port $Port boot: director enter"
		$r = Invoke-DirectorPort -Port $Port -Obj @{ action = "open" }
		Assert-Ok $r "enter-open"
		# Boot handler treats any director as enter.
		$r = Invoke-DirectorPort -Port $Port -Obj @{ action = "enter" }
		Assert-Ok $r "enter"
		$st = Wait-Pred -Port $Port -Seconds 45 -Label "enter social" -Pred {
			param($s) [string]$s.scene -eq "social" -and $s.alive -eq $true
		}
		if ([string]$st.scene -ne "social") { throw "port $Port boot did not reach social scene=$($st.scene)" }
	}
}

Write-Output "social two-box verify host=$HostPort guest=$GuestPort"

$st = $null
$httpDeadline = (Get-Date).AddSeconds([Math]::Max(20, $WaitSeconds))
while ((Get-Date) -lt $httpDeadline) {
	try { $st = Get-StatePort -Port $HostPort; break } catch { Start-Sleep -Seconds 2 }
}
if (-not $st) { throw "host http_timeout; run Scripts/dl-rebuild.ps1 -Activity social first" }

Enter-IfBoot -Port $HostPort
$st = Wait-ScenePort -Port $HostPort -Want "social" -Seconds 45
if ([string]$st.scene -ne "social") { throw "case1 expected social got $($st.scene)" }

$r = Invoke-DirectorPort -Port $HostPort -Obj @{ action = "savedefaultsocial"; kind = "private" }
Assert-Ok $r "reset host default private"
$r = Invoke-DirectorPort -Port $HostPort -Obj @{ action = "socialaudience"; kind = "private" }
Assert-Ok $r "case1 socialaudience private"

# 1. Host → private, not listening
$st = Wait-Pred -Port $HostPort -Seconds 20 -Label "case1 private" -Pred {
	param($s)
	[string]$s.scene -eq "social" -and [string]$s.lobby.access -eq "private" -and $s.lobby.listening -ne $true -and $s.alive -eq $true
}
Write-Output "case1 ok access=$($st.lobby.access) net=$($st.lobby.netMode) alive=$($st.alive)"

# 2. Host Lobby → Public
$r = Invoke-DirectorPort -Port $HostPort -Obj @{ action = "socialaudience"; kind = "public" }
Assert-Ok $r "case2 socialaudience"
$st = Wait-Pred -Port $HostPort -Seconds 45 -Label "case2 public listen" -Pred {
	param($s)
	[string]$s.scene -eq "social" -and [string]$s.lobby.access -eq "open" -and $s.lobby.listening -eq $true
}
Write-Output "case2 ok access=$($st.lobby.access) listening=$($st.lobby.listening)"

# 3. Guest join
Ensure-Guest
Enter-IfBoot -Port $GuestPort
$gst = Wait-Pred -Port $GuestPort -Seconds 45 -Label "guest social ready" -Pred {
	param($s) [string]$s.scene -eq "social" -and $s.alive -eq $true
}
if ([string]$gst.scene -ne "social") { throw "guest did not reach social scene=$($gst.scene)" }
$r = Invoke-DirectorPort -Port $GuestPort -Obj @{ action = "socialjoin"; host = "127.0.0.1"; port = 7777 }
Assert-Ok $r "case3 socialjoin"
Write-Output "case3 director=$($r | ConvertTo-Json -Compress)"
try {
	$gstNow = Get-StatePort -Port $GuestPort
	Write-Output "case3 guest immediately scene=$($gstNow.scene) net=$($gstNow.lobby.netMode) access=$($gstNow.lobby.access)"
} catch {
	Write-Output "case3 guest state unreadable after join"
}
$st = Wait-Pred -Port $HostPort -Seconds 45 -Label "case3 host seats" -Pred {
	param($s)
	[string]$s.scene -eq "social" -and [int]$s.lobby.seats -ge 2
}
$gst = Wait-Pred -Port $GuestPort -Seconds 20 -Label "case3 guest social" -Pred {
	param($s) [string]$s.scene -eq "social"
}
Write-Output "case3 ok hostSeats=$($st.lobby.seats) guestScene=$($gst.scene) guestNet=$($gst.lobby.netMode)"

# Save defaults while still connected
$r = Invoke-DirectorPort -Port $GuestPort -Obj @{ action = "savedefaultsocial"; kind = "join"; host = "127.0.0.1"; port = 7777; fallback = "private" }
Assert-Ok $r "guest savedefaultsocial"
Write-Output "guest save echo $($r | ConvertTo-Json -Compress)"
if ([string]$r.action -eq "enter") { throw "guest savedefaultsocial was swallowed as boot enter" }
if ([string]$r.savedKind -ne "join") { throw "guest save echo kind=$($r.savedKind) expected join" }
$gst = Get-StatePort -Port $GuestPort
Write-Output "guest default after save=$($gst.socialDefault | ConvertTo-Json -Compress)"
if ([string]$gst.socialDefault.kind -ne "join") { throw "guest socialDefault.kind=$($gst.socialDefault.kind) expected join" }
$r = Invoke-DirectorPort -Port $HostPort -Obj @{ action = "savedefaultsocial"; kind = "public" }
Assert-Ok $r "host savedefaultsocial"

# 4. Host raid → social public listen
$r = Invoke-DirectorPort -Port $HostPort -Obj @{ action = "raid" }
Assert-Ok $r "case4 raid"
$st = Wait-ScenePort -Port $HostPort -Want "raid" -Seconds 45
if ([string]$st.scene -ne "raid") { throw "case4 expected raid got $($st.scene)" }
$r = Invoke-DirectorPort -Port $HostPort -Obj @{ action = "social" }
Assert-Ok $r "case4 social"
$st = Wait-Pred -Port $HostPort -Seconds 45 -Label "case4 back public" -Pred {
	param($s)
	[string]$s.scene -eq "social" -and [string]$s.lobby.access -eq "open" -and $s.lobby.listening -eq $true
}
Write-Output "case4 ok access=$($st.lobby.access) listening=$($st.lobby.listening) default=$($st.socialDefault.kind)"

# 5. Guest raid → social auto-join
Enter-IfBoot -Port $GuestPort
$r = Invoke-DirectorPort -Port $GuestPort -Obj @{ action = "raid" }
Assert-Ok $r "case5 raid"
$gst = Wait-ScenePort -Port $GuestPort -Want "raid" -Seconds 45
if ([string]$gst.scene -ne "raid") { throw "case5 expected raid got $($gst.scene)" }
$r = Invoke-DirectorPort -Port $GuestPort -Obj @{ action = "social" }
Assert-Ok $r "case5 social"
$gst = Wait-Pred -Port $GuestPort -Seconds 45 -Label "case5 autojoin" -Pred {
	param($s) [string]$s.scene -eq "social"
}
$st = Wait-Pred -Port $HostPort -Seconds 30 -Label "case5 host seats again" -Pred {
	param($s)
	[string]$s.scene -eq "social" -and [int]$s.lobby.seats -ge 2
}
Write-Output "case5 ok guestScene=$($gst.scene) guestNet=$($gst.lobby.netMode) hostSeats=$($st.lobby.seats)"

# 6. Guest join dead port → fallback private
$r = Invoke-DirectorPort -Port $GuestPort -Obj @{ action = "savedefaultsocial"; kind = "join"; host = "127.0.0.1"; port = 7999; fallback = "private" }
Assert-Ok $r "case6 save fallback"
$r = Invoke-DirectorPort -Port $GuestPort -Obj @{ action = "socialjoin"; host = "127.0.0.1"; port = 7999 }
Assert-Ok $r "case6 socialjoin dead"
$gst = Wait-Pred -Port $GuestPort -Seconds 25 -Label "case6 join unavailable" -Pred {
	param($s)
	$codes = @()
	if ($s.events) { $codes = @($s.events | ForEach-Object { $_.code }) }
	[string]$s.scene -eq "social" -and (
		$codes -contains "join_unavailable" -or
		-not [string]::IsNullOrEmpty($s.lobby.joinUnavailable)
	)
}
$codes = @()
if ($gst.events) { $codes = @($gst.events | ForEach-Object { $_.code }) }
if ($codes -notcontains "join_unavailable" -and [string]::IsNullOrEmpty($gst.lobby.joinUnavailable)) {
	throw "case6 missing join_unavailable event codes=$($codes -join ',') access=$($gst.lobby.access)"
}
if ([string]$gst.scene -ne "social") { throw "case6 expected social got $($gst.scene)" }
Write-Output "case6 ok access=$($gst.lobby.access) event=join_unavailable"

Write-Output "VERIFY_OK social two-box (6 cases)"
