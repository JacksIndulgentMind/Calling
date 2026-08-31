# After appendBotBook: fail as soon as the engine says the book is not followed.
# A followAlert / botbook_* modeFail also fails the PvP match. Dump /state.events. Do not keep polling.
# Engine owns loc_still / no_stick. Do not add a second DistXY detector here.
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
		throw ("command_not_followed {0} seat={1} modeResult=fail modeFailReason={2} - match ended; do not resume" -f `
			$When, $SeatId, $reason)
	}
	$alert = [string]$S.botBook.followAlert
	$err = $S.botBook.executionError -eq $true
	$followed = $S.botBook.followed
	$cause = [string]$S.botBook.lastBranchCause
	if ($alert) {
		throw ("command_not_followed {0} seat={1} followAlert={2} book={3} loc=({4:N0},{5:N0}) modeResult={6} - dump events above; do not resume" -f `
			$When, $SeatId, $alert, $S.botBook.name, $S.x, $S.y, $S.modeResult)
	}
	if ($err -or $cause -eq "execution") {
		throw ("command_not_followed {0} seat={1} executionError book={2} lastBranchCause={3} loc=({4:N0},{5:N0})" -f `
			$When, $SeatId, $S.botBook.name, $cause, $S.x, $S.y)
	}
	if ($null -ne $followed -and $followed -eq $false) {
		throw ("command_not_followed {0} seat={1} followed=false book={2}" -f $When, $SeatId, $S.botBook.name)
	}
}
$a = Get-SeatState
Assert-Followed $a "t0"
Start-Sleep -Seconds $WaitSeconds
$b = Get-SeatState
Assert-Followed $b "t1"
$dx = [math]::Abs([double]$b.x - [double]$a.x)
$dy = [math]::Abs([double]$b.y - [double]$a.y)
$book = $b.botBook.name
$goto = $b.goto -eq $true
Write-MatchEvents $b "pass"
Write-Output ("followed seat={0} dXY=({1:N0},{2:N0}) book={3} goto={4} spawnX={5} stillCm={6}" -f `
	$SeatId, $dx, $dy, $book, $goto, $SpawnX, $StillCm)
