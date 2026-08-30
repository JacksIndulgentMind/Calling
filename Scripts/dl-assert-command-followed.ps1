# After appendBotBook: fail as soon as the engine says the book is not followed.
# A followAlert / botbook_* modeFail also fails the PvP match. Dump /state.events. Do not keep polling.
param(
	[Parameter(Mandatory = $true)][string]$StateUri,
	[Parameter(Mandatory = $true)][string]$SeatId,
	[double]$SpawnX = 0,
	[double]$StillCm = 80,
	[int]$WaitSeconds = 5
)
$ErrorActionPreference = "Stop"
function Get-SeatState {
	$sep = if ($StateUri -match '\?') { '&' } else { '?' }
	$uri = "$StateUri$sep" + "seat=$SeatId"
	return (Invoke-WebRequest -Uri $uri -UseBasicParsing -TimeoutSec 8).Content | ConvertFrom-Json
}
function Write-MatchEvents {
	param($S, [string]$When)
	Write-Output ("events {0} modeResult={1} modeFailReason={2}" -f $When, $S.modeResult, $S.modeFailReason)
	if ($null -eq $S.events) {
		Write-Output "events (none)"
		return
	}
	foreach ($e in $S.events) {
		Write-Output ("  t={0:N1} code={1} seat={2} book={3} loc=({4:N0},{5:N0}) {6}" -f `
			$e.t, $e.code, $e.seat, $e.book, $e.x, $e.y, $e.detail)
	}
}
function Assert-Followed {
	param($S, [string]$When)
	Write-MatchEvents $S $When
	$fail = [string]$S.modeResult
	$reason = [string]$S.modeFailReason
	if ($fail -eq "fail" -and $reason -like "botbook_*") {
		throw ("command_not_followed {0} seat={1} modeResult=fail modeFailReason={2} — match ended; do not resume" -f `
			$When, $SeatId, $reason)
	}
	$alert = [string]$S.botBook.followAlert
	$err = $S.botBook.executionError -eq $true
	$followed = $S.botBook.followed
	$cause = [string]$S.botBook.lastBranchCause
	if ($alert) {
		throw ("command_not_followed {0} seat={1} followAlert={2} book={3} loc=({4:N0},{5:N0}) modeResult={6} — dump events above; do not resume" -f `
			$When, $SeatId, $alert, $S.botBook.name, $S.x, $S.y, $S.modeResult)
	}
	if ($err -or $cause -eq "execution") {
		throw ("command_not_followed {0} seat={1} executionError book={2} lastBranchCause={3} loc=({4:N0},{5:N0})" -f `
			$When, $SeatId, $S.botBook.name, $cause, $S.x, $S.y)
	}
	if ($null -ne $followed -and $followed -eq $false) {
		throw ("command_not_followed {0} seat={1} followed=false book={2}" -f $When, $SeatId, $S.botBook.name)
	}
	$goto = $S.goto -eq $true
	$distWp = 0
	if ($null -ne $S.gotoDistXY) { $distWp = [double]$S.gotoDistXY }
	$mx = 0; $my = 0
	if ($null -ne $S.gotoMoveX) { $mx = [math]::Abs([double]$S.gotoMoveX) }
	if ($null -ne $S.gotoMoveY) { $my = [math]::Abs([double]$S.gotoMoveY) }
	$ax = 0; $ay = 0
	if ($null -ne $S.agentMoveX) { $ax = [math]::Abs([double]$S.agentMoveX) }
	if ($null -ne $S.agentMoveY) { $ay = [math]::Abs([double]$S.agentMoveY) }
	if ($goto -and $distWp -gt 300 -and ($mx + $my) -lt 0.08 -and ($ax + $ay) -lt 0.08) {
		throw ("command_not_followed {0} seat={1} goto_no_stick DistXY={2:N0} book={3}" -f `
			$When, $SeatId, $distWp, $S.botBook.name)
	}
}
$a = Get-SeatState
Assert-Followed $a "t0"
Start-Sleep -Seconds $WaitSeconds
$b = Get-SeatState
Assert-Followed $b "t1"
$dx = [math]::Abs([double]$b.x - [double]$a.x)
$dy = [math]::Abs([double]$b.y - [double]$a.y)
$fromSpawn = [math]::Abs([double]$b.x - $SpawnX)
$book = $b.botBook.name
$goto = $b.goto -eq $true
$commanded = [bool]$book -or $goto
$distWp = 0
if ($null -ne $b.gotoDistXY) { $distWp = [double]$b.gotoDistXY }
$stuck = 0
if ($null -ne $b.gotoStuck) { $stuck = [double]$b.gotoStuck }
if ($commanded -and $dx -lt $StillCm -and $dy -lt $StillCm -and $fromSpawn -lt $StillCm) {
	Write-MatchEvents $b "spawn_still"
	throw ("command_not_followed seat={0} wait={1}s loc=({2:N0},{3:N0}) spawnX={4} book={5} goto={6} dXY=({7:N0},{8:N0})" -f `
		$SeatId, $WaitSeconds, $b.x, $b.y, $SpawnX, $book, $goto, $dx, $dy)
}
if ($fromSpawn -lt 400 -and $dx -lt $StillCm -and $dy -lt $StillCm -and -not $goto) {
	Write-MatchEvents $b "spawn_idle"
	throw ("command_not_followed spawn_idle seat={0} loc=({1:N0},{2:N0}) spawnX={3} book={4} fire={5}" -f `
		$SeatId, $b.x, $b.y, $SpawnX, $book, $b.agentFire)
}
if ($goto -and $dx -lt $StillCm -and $dy -lt $StillCm) {
	$goalX = if ($null -ne $b.gotoX) { [double]$b.gotoX } else { 0 }
	$goalY = if ($null -ne $b.gotoY) { [double]$b.gotoY } else { 0 }
	$goalDist = [math]::Sqrt(([double]$b.x - $goalX)*([double]$b.x - $goalX) + ([double]$b.y - $goalY)*([double]$b.y - $goalY))
	if ($goalDist -gt 250) {
		Write-MatchEvents $b "loc_still"
		throw ("command_not_followed loc_still seat={0} wait={1}s loc=({2:N0},{3:N0}) goalDist={4:N0} wpDist={5:N0} stuck={6:N1} book={7}" -f `
			$SeatId, $WaitSeconds, $b.x, $b.y, $goalDist, $distWp, $stuck, $book)
	}
}
if ($stuck -ge 0.5 -and $goto -and $distWp -gt 300) {
	Write-MatchEvents $b "gotoStuck"
	throw ("command_not_followed gotoStuck seat={0} stuck={1:N1} DistXY={2:N0} book={3}" -f $SeatId, $stuck, $distWp, $book)
}
Write-MatchEvents $b "pass"
Write-Output ("followed seat={0} dXY=({1:N0},{2:N0}) book={3} goto={4}" -f $SeatId, $dx, $dy, $book, $goto)
