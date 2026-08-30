param(
  [ValidateSet("ring", "radar", "pillar", "nav")]
  [string]$Sequence = "ring"
)
$ErrorActionPreference = "Stop"
function J($method, $path, $body) {
  $p = @{
    Uri = "http://127.0.0.1:18765$path"
    Method = $method
    UseBasicParsing = $true
    TimeoutSec = 20
  }
  if ($null -ne $body) {
    $p.Body = $body
    $p.ContentType = "application/json; charset=utf-8"
  }
  return ((Invoke-WebRequest @p).Content | ConvertFrom-Json)
}
function Hub($obj) { J "POST" "/hub" ($obj | ConvertTo-Json -Compress -Depth 8) }
function Director($action) { J "POST" "/director" (@{ action = $action } | ConvertTo-Json -Compress) }

Write-Host "wait for HTTP then open overlay + compose PvP"
$httpWait = (Get-Date).AddSeconds(90)
$up = $false
while ((Get-Date) -lt $httpWait) {
  try { J "GET" "/state" $null | Out-Null; $up = $true; break } catch { Start-Sleep -Seconds 2 }
}
if (-not $up) { throw "http down" }
$boot = J "GET" "/state" $null
if ($boot.scene -eq "boot") {
  Write-Host "boot: director enter (default profile)"
  Director "enter" | Out-Null
  $leave = (Get-Date).AddSeconds(45)
  while ((Get-Date) -lt $leave) {
    try { $boot = J "GET" "/state" $null } catch { Start-Sleep -Seconds 1; continue }
    Write-Host ("scene={0}" -f $boot.scene)
    if ($boot.scene -ne "boot") { break }
    Start-Sleep -Seconds 1
  }
  if ($boot.scene -eq "boot") { throw "stuck on boot after enter" }
}
if ($Sequence -eq "pillar") {
  Write-Host "sequence=pillar (goto pillar_pad)"
  Director "practice" | Out-Null
  $pw = (Get-Date).AddSeconds(45)
  $ps = $null
  while ((Get-Date) -lt $pw) {
    try { $ps = J "GET" "/state" $null } catch { Start-Sleep -Seconds 1; continue }
    Write-Host ("scene={0}" -f $ps.scene)
    if ($ps.scene -eq "practice") { break }
    Start-Sleep -Seconds 1
  }
  if ($ps.scene -ne "practice") { throw "expected practice, got $($ps.scene)" }
  $joinP = Hub @{ type = "join"; displayName = "pillarA"; headless = $true; kind = "cursor" }
  $seatP = $joinP.seatId
  if (-not $joinP.ok) { throw "pillar join failed" }
  $hostP = $ps.lobby.seatList | Where-Object { $_.host -eq $true } | Select-Object -First 1
  if (-not $hostP) { throw "no host seat on practice" }
  Hub @{ type = "mindControl"; seatId = $seatP; targetSeatId = $hostP.id } | Out-Null
  Hub @{ type = "appendBotBook"; seatId = $seatP; botBook = "pillar_dive" } | Out-Null
  $ok = $false
  $untilP = (Get-Date).AddSeconds(25)
  while ((Get-Date) -lt $untilP) {
    Start-Sleep -Milliseconds 200
    $st = J "GET" "/state?seat=$seatP" $null
    $z = [double]$st.z
    $r = [math]::Sqrt(([double]$st.x * [double]$st.x) + ([double]$st.y * [double]$st.y))
    $air = $st.air -eq $true
    Write-Host ("pillar z={0:N0} r={1:N0} diving={2} air={3}" -f $z, $r, $st.diving, $st.air)
    if ($z -lt -1800 -and $r -gt 800 -and $st.diving -ne $true -and -not $air) { $ok = $true; break }
  }
  if (-not $ok) { throw "pillar pad not stuck" }
  Write-Host "VERIFY_OK"
  exit 0
}

function Write-NavProbe($st) {
  Write-Host ("nav probe tiles={0} lipOk={1} padOk={2} findPathMeshOk={3} partial={4} offMesh={5} validEndsMax={6} jumpLen={7:N0} jumpH={8:N0} jumpMaxDepth={9:N0} bakeMs={10:N0}" -f `
    $st.navTiles, $st.edgePadLipOk, $st.edgePadPadOk, $st.findPathMeshOk, $st.edgePadPartial, `
    $st.edgePadOffMesh, $st.edgePadValidEndsMax, `
    [double]$st.airDiveJumpLength, [double]$st.airDiveJumpHeight, [double]$st.airDiveJumpMaxDepth, [double]$st.edgePadBakeMs)
  if ([int]$st.navTiles -le 0) { throw "navTiles 0" }
  if ($st.edgePadPadOk -ne $true) { throw "edgePadPadOk=false: island not in nav" }
  Write-Host "VERIFY_OK"
}

if ($Sequence -eq "nav") {
  Write-Host "sequence=nav (bake FindPath mesh probe)"
  $cur = J "GET" "/state" $null
  if ($cur.scene -ne "pvp") {
    Write-Host "director arena (solo PvP skip)"
    Director "arena" | Out-Null
    $wArena = (Get-Date).AddSeconds(60)
    while ((Get-Date) -lt $wArena) {
      try { $cur = J "GET" "/state" $null } catch { Start-Sleep -Seconds 1; continue }
      Write-Host ("scene={0}" -f $cur.scene)
      if ($cur.scene -eq "pvp") { break }
      Start-Sleep -Seconds 1
    }
    if ($cur.scene -ne "pvp") { throw "arena did not reach pvp scene=$($cur.scene)" }
  }
  $w = (Get-Date).AddSeconds(45)
  $st = $cur
  while ((Get-Date) -lt $w) {
    try { $st = J "GET" "/state" $null } catch { Start-Sleep -Seconds 1; continue }
    if ([int]$st.navTiles -gt 0) { break }
    Start-Sleep -Seconds 1
  }
  Write-NavProbe $st
  exit 0
}

$s = J "GET" "/state" $null
if ($s.scene -eq "composer") {
  Write-Host "already composer"
} else {
  Write-Host "compose PvP"
  Director "open" | Out-Null
  Director "pvp" | Out-Null
  $s = $null
  $wait = (Get-Date).AddSeconds(45)
  while ((Get-Date) -lt $wait) {
    try { $s = J "GET" "/state" $null } catch { Start-Sleep -Seconds 1; continue }
    Write-Host ("scene={0}" -f $s.scene)
    if ($s.scene -eq "composer") { break }
    Start-Sleep -Seconds 1
  }
}
if ($s.scene -ne "composer") { throw "expected composer, got $($s.scene)" }

Write-Host "select Host (UI / director)"
Director "host" | Out-Null
$s = J "GET" "/state" $null
if ($s.lobby.localHost -ne $true) { throw "localHost false after director host" }

$hostSeat = $s.lobby.seatList | Where-Object { $_.host -eq $true } | Select-Object -First 1
Write-Host ("composer host={0} team={1} ready={2} gate={3}" -f $hostSeat.id, $hostSeat.team, $hostSeat.ready, $s.lobby.gate)

Write-Host "host Ready (UI / director)"
$hr = Director "ready"
Write-Host ("director ready ok={0}" -f $hr.ok)
Start-Sleep -Milliseconds 400
$s = J "GET" "/state" $null
$hostSeat = $s.lobby.seatList | Where-Object { $_.host -eq $true } | Select-Object -First 1
if ($hostSeat.ready -ne $true) { throw "host ready did not stick" }
if ($s.lobby.countdown -eq $true) { throw "countdown started on host-only ready" }

$joinA = Hub @{ type = "join"; displayName = "agentA"; headless = $true; kind = "cursor" }
$seatA = $joinA.seatId
Hub @{ type = "mindControl"; seatId = $seatA; targetSeatId = $hostSeat.id } | Out-Null
Hub @{ type = "setTeam"; seatId = $hostSeat.id; team = "red" } | Out-Null
Write-Host ("A join={0} mc host" -f $joinA.ok)

$joinB = Hub @{ type = "join"; displayName = "agentB"; headless = $true; kind = "cursor" }
$seatB = $joinB.seatId
Hub @{ type = "setTeam"; seatId = $seatB; team = "blue" } | Out-Null
$rb = Hub @{ type = "ready"; seatId = $seatB; ready = $true }
Write-Host ("B join={0} readyNet={1}" -f $joinB.ok, $rb.ok)
Start-Sleep -Milliseconds 400
$s = J "GET" "/state" $null
if ($s.lobby.ready -lt 2) { throw "expected 2 ready, got $($s.lobby.ready)" }
if ($s.lobby.countdown -eq $true) { throw "countdown started before host Start" }

Write-Host "host Start (UI / director go)"
$go = Director "go"
Write-Host ("director go ok={0}" -f $go.ok)

$pvp = $null
$deadline = (Get-Date).AddSeconds(25)
while ((Get-Date) -lt $deadline) {
  Start-Sleep -Seconds 1
  try { $pvp = J "GET" "/state" $null } catch { continue }
  if ($pvp.scene -eq "pvp") { break }
}
if ($pvp.scene -ne "pvp") { throw "did not reach pvp scene=$($pvp.scene)" }
Write-Host ("pvp seats={0} gate={1} unlocked={2}" -f $pvp.lobby.seats, $pvp.lobby.gate, $pvp.lobby.unlocked)

$navWait = (Get-Date).AddSeconds(20)
$a0 = $null; $b0 = $null
while ((Get-Date) -lt $navWait) {
  $a0 = J "GET" "/state?seat=$seatA" $null
  $b0 = J "GET" "/state?seat=$seatB" $null
  if ($a0.ok -and $b0.ok -and $a0.navTiles -gt 0 -and $b0.navTiles -gt 0) { break }
  Start-Sleep -Seconds 1
}
Write-Host ("A x={0:N0} z={1:N0} nav={2}" -f $a0.x, $a0.z, $a0.navTiles)
Write-Host ("B x={0:N0} z={1:N0} nav={2}" -f $b0.x, $b0.z, $b0.navTiles)
if ([math]::Abs($a0.x + 6380) -gt 800) { throw "A not red spawn" }
if ([math]::Abs($b0.x - 6380) -gt 800) { throw "B not blue spawn" }
if ($a0.navTiles -le 0 -or $b0.navTiles -le 0) { throw "navTiles still 0 after wait" }

function Seat($id) { J "GET" "/state?seat=$id" $null }
function DistXY($st, $x, $y) {
  $dx = [double]$st.x - [double]$x
  $dy = [double]$st.y - [double]$y
  return [math]::Sqrt($dx * $dx + $dy * $dy)
}
function Test-OnEdgePad($st) {
  if ($null -eq $st.edgePadX -or $null -eq $st.edgePadY -or $null -eq $st.edgePadZ) { return $false }
  $d = DistXY $st $st.edgePadX $st.edgePadY
  $z = [double]$st.z
  $pz = [double]$st.edgePadZ
  return ($d -lt 420 -and $z -gt ($pz - 50) -and $z -lt ($pz + 260) `
    -and $st.diving -ne $true -and $st.air -ne $true)
}
$script:lastCollapseRecover = @{}
# Recover, not a Recast cheat: polar XYZ from the south lip re-takes the island
# AirDive chord. Re-take court via edge_lip; HoldRing uses menhir when already on island.
function RecoverCourt($id) {
  Write-Host ("recover court seat={0}" -f $id)
  $puml = @"
@startuml recover_court
start
:goto marker=edge_lip;
note right
  success: distXY 400
  goodEnough: distXY 700
  fail.timeout: 40
end note
stop
@enduml
"@
  Hub @{ type = "branchBotBook"; seatId = $id; puml = $puml } | Out-Null
  $until = (Get-Date).AddSeconds(45)
  while ((Get-Date) -lt $until) {
    Start-Sleep -Milliseconds 300
    $st = Seat $id
    if (-not $st.ok) { continue }
    if (Test-OnEdgePad $st) { continue }
    if ($st.diving -eq $true -or $st.air -eq $true) { continue }
    if ([double]$st.z -gt -2300 -and -not (BookBusy $st) -and -not $st.goto) { return $st }
  }
  return (Seat $id)
}
function AssertNoCollapse($st, $id) {
  if (-not $st.ok) { return }
  if (Test-OnEdgePad $st) { return }
  # Mid Launch / dive to the island is expected; only void fall is collapse.
  if ($st.diving -eq $true -or $st.air -eq $true) { return }
  if ([double]$st.z -ge -2800) { return }
  $now = Get-Date
  $last = $script:lastCollapseRecover[$id]
  if ($last -and ($now - $last).TotalSeconds -lt 10) { return }
  $script:lastCollapseRecover[$id] = $now
  Write-Host ("Z-collapse recover seat={0} z={1:N0}" -f $id, [double]$st.z)
  $back = RecoverCourt $id
  if ([double]$back.z -lt -2800 -and -not (Test-OnEdgePad $back) `
    -and $back.diving -ne $true -and $back.air -ne $true) {
    throw "Z-collapse seat=$id z=$($back.z) after recover"
  }
}
function WaitIdle($id, $sec = 20) {
  $until = (Get-Date).AddSeconds($sec)
  $last = $null
  while ((Get-Date) -lt $until) {
    Start-Sleep -Milliseconds 300
    $last = Seat $id
    AssertNoCollapse $last $id
    $seq = 0.0
    if ($null -ne $last.seqRemaining) { $seq = [double]$last.seqRemaining }
    if ($last.ok -and -not $last.goto -and $seq -le 0.08 -and -not (BookBusy $last)) { return $last }
  }
  return $last
}
function BookBusy($st) {
  if ($null -eq $st.botBook) { return $false }
  $n = [string]$st.botBook.name
  return -not [string]::IsNullOrWhiteSpace($n)
}
function AppendBook($id, $name) {
  return Hub @{ type = "appendBotBook"; seatId = $id; botBook = $name }
}
function BranchBook($id, $name) {
  return Hub @{ type = "branchBotBook"; seatId = $id; botBook = $name }
}
function WaitBot($id, $sec = 90) {
  $until = (Get-Date).AddSeconds($sec)
  $last = $null
  while ((Get-Date) -lt $until) {
    Start-Sleep -Milliseconds 300
    $last = Seat $id
    AssertNoCollapse $last $id
    if ($last.ok -and -not (BookBusy $last) -and -not $last.goto) { return $last }
  }
  return $last
}
function StartGoto($id, $x, $y, $z) {
  $puml = @"
@startuml jit_goto
start
:goto x=$x y=$y z=$z;
note right
  success: distXY 150
  goodEnough: distXY 280
  fail.timeout: 55
end note
stop
@enduml
"@
  return Hub @{ type = "appendBotBook"; seatId = $id; puml = $puml }
}
function TrackArrive($id, $x, $y, $z, $arrive, $tracker) {
  $st = Seat $id
  if (-not $st.ok) { return $false }
  AssertNoCollapse $st $id
  $d = DistXY $st $x $y
  $dz = [math]::Abs([double]$st.z - [double]$z)
  if ($d -lt $arrive -and $dz -lt 500) { $tracker.last = $st; return $true }
  if ($null -eq $tracker.anchor) { $tracker.anchor = $st; $tracker.t = Get-Date }
  else {
    $moved = DistXY $st $tracker.anchor.x $tracker.anchor.y
    if ($moved -gt 90) { $tracker.anchor = $st; $tracker.t = Get-Date }
    elseif (((Get-Date) - $tracker.t).TotalSeconds -gt 8) { $tracker.stalled = $true }
  }
  $tracker.last = $st
  return $false
}
function DropOffLintel($id) {
  $st = Seat $id
  if (-not $st.ok) { return }
  if ([double]$st.z -lt -1750) { return }
  Write-Host ("drop off lintel seat z={0:N0}" -f [double]$st.z)
  $yaw = [math]::Round((OriginDeg $st) + 180.0, 1)
  Plan $id @(
    @{ seconds = 0.2; look = @{ yawAbs = $yaw; pitchAbs = 0 } },
    @{ seconds = 0.12; jump = $true; move = @{ x = 0; y = 1 }; look = @{ yawAbs = $yaw; pitchAbs = 0 } },
    @{ seconds = 0.45; airDive = $true; move = @{ x = 0; y = 1 }; look = @{ yawAbs = $yaw; pitchAbs = 0 } },
    @{ seconds = 0.7; move = @{ x = 0; y = 0 } }
  ) | Out-Null
  WaitFlagClear $id "diving" 5 | Out-Null
}
function GotoOne($id, $x, $y, $z, $arrive = 800) {
  DropOffLintel $id
  for ($try = 1; $try -le 4; $try++) {
    $st = Seat $id
    if ((DistXY $st $x $y) -lt $arrive -and [math]::Abs([double]$st.z - [double]$z) -lt 500) {
      return $st
    }
    StartGoto $id $x $y $z | Out-Null
    $tr = @{ stalled = $false }
    $until = (Get-Date).AddSeconds(55)
    while ((Get-Date) -lt $until) {
      Start-Sleep -Milliseconds 400
      if (TrackArrive $id $x $y $z $arrive $tr) { return $tr.last }
      if ($tr.stalled) {
        Write-Host ("goto one stalled seat try={0}" -f $try)
        DropOffLintel $id
        break
      }
    }
  }
  return (Seat $id)
}
function GotoPair($idA, $ax, $ay, $az, $idB, $bx, $by, $bz, $arrive = 700) {
  for ($try = 1; $try -le 4; $try++) {
    $stA = Seat $idA
    $stB = Seat $idB
    $needA = (DistXY $stA $ax $ay) -ge $arrive -or [math]::Abs([double]$stA.z - [double]$az) -ge 500
    $needB = (DistXY $stB $bx $by) -ge $arrive -or [math]::Abs([double]$stB.z - [double]$bz) -ge 500
    if ($needA) { $ga = StartGoto $idA $ax $ay $az } else { $ga = @{ ok = $true; partial = $false } }
    if ($needB) { $gb = StartGoto $idB $bx $by $bz } else { $gb = @{ ok = $true; partial = $false } }
    Write-Host ("gotoPair try={0} A ok={1} partial={2} B ok={3} partial={4}" -f $try, $ga.ok, $ga.partial, $gb.ok, $gb.partial)
    if ($ga.ok -ne $true -or $gb.ok -ne $true -or $ga.partial -eq $true -or $gb.partial -eq $true) {
      Start-Sleep -Seconds 1
      continue
    }
    $ta = @{ stalled = $false }
    $tb = @{ stalled = $false }
    $doneA = $false
    $doneB = $false
    $until = (Get-Date).AddSeconds(70)
    while ((Get-Date) -lt $until) {
      Start-Sleep -Seconds 1
      if (-not $doneA) { $doneA = TrackArrive $idA $ax $ay $az $arrive $ta }
      if (-not $doneB) { $doneB = TrackArrive $idB $bx $by $bz $arrive $tb }
      if ($doneA -and $doneB) { return @{ A = $ta.last; B = $tb.last } }
      if ($ta.stalled -or $tb.stalled) {
        Write-Host "goto stalled, rewrite from live state"
        break
      }
    }
  }
  throw "goto pair failed after retries"
}
function FaceYaw($from, $to) {
  $dx = [double]$to.x - [double]$from.x
  $dy = [double]$to.y - [double]$from.y
  return [math]::Atan2($dy, $dx) * 180.0 / [math]::PI
}
function Plan($id, $steps) {
  return Hub @{ type = "plan"; seatId = $id; steps = $steps }
}
function Pool($st) {
  return [double]$st.shield + [double]$st.health
}
function PoolHurt($st) {
  if (-not $st.ok) { return $true }
  if ($st.alive -eq $false) { return $true }
  $h = 100.0
  $s = 100.0
  if ($null -ne $st.health) { $h = [double]$st.health }
  if ($null -ne $st.shield) { $s = [double]$st.shield }
  return ($s -le 0.5 -or $h -lt 99.0)
}
function FireUntilHit($atk, $tgt, $tries = 6) {
  $baseline = Pool (Seat $tgt)
  if ($baseline -le 0.5) {
    Write-Host "fire skip, target already down"
    return
  }
  function Tap($yaw) {
    Plan $atk @(
      @{ seconds = 0.5; look = @{ yawAbs = $yaw; pitchAbs = 0 } },
      @{ seconds = 0.16; ads = $true; fire = $true; look = @{ yawAbs = $yaw; pitchAbs = 0 } }
    ) | Out-Null
    WaitIdle $atk 4 | Out-Null
    return (Pool (Seat $tgt))
  }
  for ($i = 1; $i -le $tries; $i++) {
    $t0 = Seat $tgt
    $a0 = Seat $atk
    $yaw = [math]::Round((FaceYaw $a0 $t0), 1)
    if ($i -gt 1) {
      Plan $atk @(
        @{ seconds = 0.4; sprint = $true; move = @{ x = (($i % 2) * 2 - 1); y = 1 }; look = @{ yawAbs = $yaw; pitchAbs = 0 } }
      ) | Out-Null
      WaitIdle $atk 3 | Out-Null
      $a0 = Seat $atk
      $t0 = Seat $tgt
      $yaw = [math]::Round((FaceYaw $a0 $t0), 1)
    }
    $after = Tap $yaw
    Write-Host ("fire try={0} yaw={1} pool {2:N0}->{3:N0} base={4:N0}" -f $i, $yaw, (Pool $t0), $after, $baseline)
    if ($after -lt ($baseline - 0.5)) { return }
  }
  $a0 = Seat $atk
  $t0 = Seat $tgt
  $dx = [double]$t0.x - [double]$a0.x
  $dy = [double]$t0.y - [double]$a0.y
  $len = [math]::Sqrt($dx * $dx + $dy * $dy)
  if ($len -gt 500) {
    $nx = [double]$t0.x - $dx / $len * 450.0
    $ny = [double]$t0.y - $dy / $len * 450.0
    Write-Host "fire close-goto"
    StartGoto $atk $nx $ny ([double]$t0.z) | Out-Null
    $until = (Get-Date).AddSeconds(20)
    while ((Get-Date) -lt $until) {
      Start-Sleep -Milliseconds 400
      $st = Seat $atk
      AssertNoCollapse $st $atk
      if (-not $st.goto -and (DistXY $st $nx $ny) -lt 500) { break }
    }
    $t0 = Seat $tgt
    $a0 = Seat $atk
    $yaw = [math]::Round((FaceYaw $a0 $t0), 1)
    $after = Tap $yaw
    Write-Host ("fire close yaw={0} pool {1:N0} base={2:N0}" -f $yaw, $after, $baseline)
    if ($after -lt ($baseline - 0.5)) { return }
  }
  throw "no damage registered after retries"
}

function HideB {
  return @{ x = 0; y = 400; z = -600 }
}
function DistBHide($st) {
  $h = HideB
  return (DistXY $st $h.x $h.y)
}
function GotoBHide {
  $h = HideB
  $b = Seat $seatB
  if ((DistBHide $b) -lt 200) { return }
  StartGoto $seatB $h.x $h.y $h.z | Out-Null
  $until = (Get-Date).AddSeconds(45)
  while ((Get-Date) -lt $until) {
    Start-Sleep -Milliseconds 400
    $b = Seat $seatB
    AssertNoCollapse $b $seatB
    if (-not $b.goto -and (DistBHide $b) -lt 280) { return }
  }
}
function CoverHolds {
  Write-Host "B hide first, then A (do not shove B off goto)"
  GotoBHide
  $p = Polar 0 1250
  $a = Seat $seatA
  if ((DistXY $a $p.x $p.y) -ge 400) {
    StartGoto $seatA $p.x $p.y $p.z | Out-Null
    $until = (Get-Date).AddSeconds(50)
    while ((Get-Date) -lt $until) {
      Start-Sleep -Milliseconds 400
      $a = Seat $seatA
      AssertNoCollapse $a $seatA
      if (-not $a.goto -and (DistXY $a $p.x $p.y) -lt 450) { return }
    }
  }
}
function Polar($deg, $r) {
  $rad = [double]$deg * [math]::PI / 180.0
  return @{ x = [math]::Cos($rad) * $r; y = [math]::Sin($rad) * $r; z = -600 }
}
function OriginDeg($st) { return [math]::Atan2([double]$st.y, [double]$st.x) * 180.0 / [math]::PI }
function OnLintel($st, $deg) {
  if (-not $st.ok) { return $false }
  if ($st.air -eq $true) { return $false }
  if ($st.diving -eq $true) { return $false }
  $z = [double]$st.z
  if ($z -lt -1750 -or $z -gt -1450) { return $false }
  $p = Polar $deg 950
  return ((DistXY $st $p.x $p.y) -lt 280)
}
function SetView($id) {
  if ($id) {
    Hub @{ type = "view"; seatId = $id } | Out-Null
  } else {
    Hub @{ type = "view"; seatId = "" } | Out-Null
  }
  Start-Sleep -Milliseconds 500
}
function RingRadius($st) {
  return DistXY $st 0 0
}
function WaitAlive($id, $sec) {
  $until = (Get-Date).AddSeconds($sec)
  while ((Get-Date) -lt $until) {
    $st = Seat $id
    AssertNoCollapse $st $id
    if ($st.alive -ne $false) { return $st }
    Start-Sleep -Milliseconds 200
  }
  return (Seat $id)
}
# Recover onto the court ring after EdgeHop. Polar XYZ from the south lip re-takes
# the island AirDive; menhir / edge_lip keep the pawn on court. Do not shrink JumpLength.
function HoldRing {
  $st = Seat $seatA
  $r = RingRadius $st
  $d = 950.0
  if ($null -ne $st.slideDistanceCm) { $d = [double]$st.slideDistanceCm }
  if ($d -gt 1200) { $d = 950.0 }
  $maxR = [math]::Sqrt([math]::Max(40000.0, 1900.0 * 1900.0 - $d * $d))
  if ($r -ge 1100 -and $r -le $maxR) { return }
  # South lip after edge_pad: polar XYZ toward the ring often re-takes the island
  # AirDive chord. Pull onto the court ring via menhir approach instead.
  if ([double]$st.z -gt -2300 -and $r -gt $maxR) {
    Write-Host ("A hold via menhir_0_approach r={0:N0} (avoid island chord)" -f $r)
    $puml = @"
@startuml hold_ring
start
:goto marker=menhir_0_approach;
note right
  success: distXY 450
  goodEnough: distXY 800
  fail.timeout: 50
end note
stop
@enduml
"@
    Hub @{ type = "appendBotBook"; seatId = $seatA; puml = $puml } | Out-Null
    WaitBot $seatA 55 | Out-Null
    return
  }
  $deg = OriginDeg $st
  $target = [math]::Min(1250.0, $maxR - 30.0)
  Write-Host ("A slide-commit r={0:N0} d={1:N0} -> {2:N0}" -f $r, $d, $target)
  $until = (Get-Date).AddSeconds(45)
  while ((Get-Date) -lt $until) {
    $st = Seat $seatA
    AssertNoCollapse $st $seatA
    $rNow = RingRadius $st
    if ($rNow -ge 1100 -and $rNow -le $maxR) { return }
    if (-not $st.goto -and -not (BookBusy $st)) {
      $deg = OriginDeg $st
      $p = Polar $deg $target
      StartGoto $seatA $p.x $p.y $p.z | Out-Null
    }
    Start-Sleep -Milliseconds 400
  }
  Write-Host ("A ring hold timeout r={0:N0}" -f (RingRadius (Seat $seatA)))
}
function WaitFlagClear($id, $flag, $sec) {
  $until = (Get-Date).AddSeconds($sec)
  while ((Get-Date) -lt $until) {
    $st = Seat $id
    AssertNoCollapse $st $id
    $seq = 0.0
    if ($null -ne $st.seqRemaining) { $seq = [double]$st.seqRemaining }
    $busy = $false
    if ($flag -eq "sliding") { $busy = ($st.sliding -eq $true) }
    elseif ($flag -eq "dodging") { $busy = ($st.dodging -eq $true) }
    elseif ($flag -eq "dashing") { $busy = ($st.dashing -eq $true) }
    elseif ($flag -eq "diving") { $busy = ($st.diving -eq $true -or $st.air -eq $true) }
    if ($st.ok -and -not $st.goto -and $seq -le 0.08 -and -not $busy) { return $st }
    Start-Sleep -Milliseconds 80
  }
  return (Seat $id)
}
function SightedLos {
  Write-Host "sighted readout LOS"
  $aPos = Polar 22.5 1250
  $bPos = Polar 45 1250
  GotoPair $seatA $aPos.x $aPos.y $aPos.z $seatB $bPos.x $bPos.y $bPos.z 400 | Out-Null
  WaitAlive $seatA 5 | Out-Null
  WaitAlive $seatB 5 | Out-Null
  $a = Seat $seatA
  $b = Seat $seatB
  $yaw = [math]::Round((FaceYaw $a $b), 1)
  Plan $seatA @(@{ seconds = 0.5; look = @{ yawAbs = $yaw; pitchAbs = 0 } }) | Out-Null
  WaitIdle $seatA 3 | Out-Null
  Start-Sleep -Milliseconds 400
  $st = Seat $seatA
  Write-Host ("sightedHealth={0} sightedShield={1} A=({2:N0},{3:N0}) B=({4:N0},{5:N0}) yaw={6}" -f $st.sightedHealth, $st.sightedShield, [double]$st.x, [double]$st.y, [double]$b.x, [double]$b.y, $yaw)
  if ($null -eq $st.sightedHealth) { throw "expected sightedHealth with B in pip" }
  $hide = Polar 0 1250
  CoverHolds
  WaitAlive $seatA 5 | Out-Null
  WaitAlive $seatB 5 | Out-Null
}
function RadarBlips($st) {
  if ($null -eq $st.radarBlips) { return 0 }
  return [int]$st.radarBlips
}
function RadarLos {
  Write-Host "radar LOS motion then still then cover"
  $aPos = Polar 22.5 1250
  $bPos = Polar 45 1250
  GotoPair $seatA $aPos.x $aPos.y $aPos.z $seatB $bPos.x $bPos.y $bPos.z 400 | Out-Null
  WaitAlive $seatA 5 | Out-Null
  WaitAlive $seatB 5 | Out-Null
  $a = Seat $seatA
  $b = Seat $seatB
  $yawA = [math]::Round((FaceYaw $a $b), 1)
  $yawB = [math]::Round((FaceYaw $b $a), 1)
  Plan $seatA @(@{ seconds = 0.4; look = @{ yawAbs = $yawA; pitchAbs = 0 } }) | Out-Null
  Plan $seatB @(@{ seconds = 0.9; move = @{ x = 0; y = 1 }; look = @{ yawAbs = $yawB; pitchAbs = 0 } }) | Out-Null
  $seen = $false
  $until = (Get-Date).AddSeconds(1.1)
  $st = $null
  while ((Get-Date) -lt $until) {
    Start-Sleep -Milliseconds 80
    $st = Seat $seatA
    if ((RadarBlips $st) -ge 1) { $seen = $true; break }
  }
  Write-Host ("radar walk blips={0}" -f (RadarBlips $st))
  if (-not $seen) { throw "expected radarBlips while B walks in LOS" }
  WaitIdle $seatB 3 | Out-Null
  Start-Sleep -Milliseconds 150
  $still = Seat $seatA
  Write-Host ("radar still blips={0}" -f (RadarBlips $still))
  if ((RadarBlips $still) -ne 0) { throw "expected radarBlips 0 when B stops" }

  Write-Host "radar cover: 60-deg spoke wall"
  GotoPair $seatA 127 620 -600 $seatB 473 420 -600 150 | Out-Null
  WaitAlive $seatA 5 | Out-Null
  WaitAlive $seatB 5 | Out-Null
  $a = Seat $seatA
  $b = Seat $seatB
  $yawA = [math]::Round((FaceYaw $a $b), 1)
  Plan $seatA @(@{ seconds = 0.35; look = @{ yawAbs = $yawA; pitchAbs = 0 } }) | Out-Null
  Plan $seatB @(@{ seconds = 0.45; move = @{ x = 0; y = 1 }; look = @{ yawAbs = 60; pitchAbs = 0 } }) | Out-Null
  $blocked = $true
  $heard = $false
  $until = (Get-Date).AddSeconds(1.0)
  $coverSt = $null
  while ((Get-Date) -lt $until) {
    Start-Sleep -Milliseconds 80
    $coverSt = Seat $seatA
    if ((RadarBlips $coverSt) -ge 1) { $blocked = $false; break }
    $ripple = 0
    if ($null -ne $coverSt.radarRipple) { $ripple = [int]$coverSt.radarRipple }
    if ($ripple -ne 0) { $heard = $true }
  }
  Write-Host ("radar cover blips={0} rippleLast={1} heard={2} A=({3:N0},{4:N0}) B=({5:N0},{6:N0})" -f (RadarBlips $coverSt), $coverSt.radarRipple, $heard, [double]$a.x, [double]$a.y, [double]$b.x, [double]$b.y)
  if (-not $blocked) { throw "expected no radarBlips with B moving behind cover" }
  if (-not $heard) { throw "expected radarRipple while B moves behind cover" }
  WaitIdle $seatB 3 | Out-Null
}
function RadarViewB {
  Write-Host "radar + HUD follow camera B"
  $aPos = Polar 22.5 1250
  $bPos = Polar 45 1250
  GotoPair $seatA $aPos.x $aPos.y $aPos.z $seatB $bPos.x $bPos.y $bPos.z 400 | Out-Null
  WaitAlive $seatA 5 | Out-Null
  WaitAlive $seatB 5 | Out-Null
  $a = Seat $seatA
  $b = Seat $seatB
  $yawA = [math]::Round((FaceYaw $a $b), 1)
  $yawB = [math]::Round((FaceYaw $b $a), 1)
  Plan $seatB @(@{ seconds = 0.4; look = @{ yawAbs = $yawB; pitchAbs = 0 } }) | Out-Null
  Plan $seatA @(@{ seconds = 0.9; move = @{ x = 0; y = 1 }; look = @{ yawAbs = $yawA; pitchAbs = 0 } }) | Out-Null
  $seen = $false
  $viewBlips = 0
  $until = (Get-Date).AddSeconds(1.1)
  $st = $null
  $hostSt = $null
  while ((Get-Date) -lt $until) {
    Start-Sleep -Milliseconds 80
    $st = Seat $seatB
    $hostSt = J "GET" "/state" $null
    if ((RadarBlips $st) -ge 1) { $seen = $true }
    if ($null -ne $hostSt.viewRadarBlips) { $viewBlips = [int]$hostSt.viewRadarBlips }
    if ($seen -and $viewBlips -ge 1) { break }
  }
  $bVitals = Seat $seatB
  Write-Host ("radar B blips={0} viewRadarBlips={1} viewHealth={2} B health={3}" -f (RadarBlips $st), $viewBlips, $hostSt.viewHealth, $bVitals.health)
  if (-not $seen) { throw "expected B radarBlips while A walks in LOS" }
  if ($viewBlips -lt 1) { throw "expected viewRadarBlips from camera on B" }
  if ($null -eq $hostSt.viewHealth) { throw "expected viewHealth from camera" }
  if ([math]::Abs([double]$hostSt.viewHealth - [double]$bVitals.health) -gt 1.0) { throw "viewHealth should match B" }
  WaitIdle $seatA 3 | Out-Null
}
function EdgeHop {
  Write-Host "edge hop: lip then pad"
  WaitAlive $seatA 5 | Out-Null
  $lip = @"
@startuml jit_edge_lip
start
:goto marker=edge_lip;
note right
  success: distXY 180
  goodEnough: distXY 280
  fail.timeout: 50
end note
stop
@enduml
"@
  Hub @{ type = "appendBotBook"; seatId = $seatA; puml = $lip } | Out-Null
  $st = WaitBot $seatA 55
  Write-Host ("edge_lip x={0:N0} y={1:N0} z={2:N0} nav={3}" -f [double]$st.x, [double]$st.y, [double]$st.z, $st.navTiles)
  if ([double]$st.z -lt -2300) { throw "edge_lip left court floor" }
  $divedPad = $false
  $padOk = $false
  for ($try = 1; $try -le 3 -and -not $padOk; $try++) {
    Write-Host ("edge_pad try={0}" -f $try)
    AppendBook $seatA "edge_pad" | Out-Null
    $until = (Get-Date).AddSeconds(55)
    while ((Get-Date) -lt $until) {
      Start-Sleep -Milliseconds 200
      $st = Seat $seatA
      if ($st.diving -eq $true) { $divedPad = $true; Write-Host "edge_pad diving=true" }
      if (Test-OnEdgePad $st) { $padOk = $true; break }
      if ($st.ok -and -not (BookBusy $st) -and -not $st.goto -and $st.air -ne $true -and $st.diving -ne $true) { break }
    }
    Write-Host ("edge_pad x={0:N0} y={1:N0} z={2:N0} findPathMeshOk={3} diving={4} padOk={5}" -f `
      [double]$st.x, [double]$st.y, [double]$st.z, $st.findPathMeshOk, $divedPad, $padOk)
  }
  if (-not $padOk) { throw "edge_pad not stuck (Recast goto only - no JIT airDive)" }
  if ($divedPad) { $script:edgeDived = $true }
  Write-Host "wait island recall to lip"
  $re = (Get-Date).AddSeconds(8)
  while ((Get-Date) -lt $re) {
    Start-Sleep -Milliseconds 250
    $st = Seat $seatA
    if ($st.ok -and [double]$st.z -gt -2300) { break }
  }
  Write-Host ("recall x={0:N0} y={1:N0} z={2:N0}" -f [double]$st.x, [double]$st.y, [double]$st.z)
  if ([double]$st.z -lt -2300) { throw "island recall did not return to court" }
}
function RecoilDemo {
  Write-Host "recoil hip then ads"
  WaitAlive $seatA 5 | Out-Null
  WaitAlive $seatB 5 | Out-Null
  $a = Seat $seatA
  $b = Seat $seatB
  $yaw = [math]::Round((FaceYaw $a $b), 1)
  Plan $seatA @(
    @{ seconds = 0.45; look = @{ yawAbs = $yaw; pitchAbs = 0 } }
  ) | Out-Null
  WaitIdle $seatA 3 | Out-Null
  $a = Seat $seatA
  $pitch0 = [double]$a.pitch
  Plan $seatA @(
    @{ seconds = 0.45; look = @{ yawAbs = $yaw } },
    @{ seconds = 0.55; fire = $true; look = @{ yawAbs = $yaw } }
  ) | Out-Null
  WaitIdle $seatA 3 | Out-Null
  $hip = Seat $seatA
  $hipDelta = [double]$hip.pitch - $pitch0
  Write-Host ("hip recoil pitch {0:N1} -> {1:N1} d={2:N1}" -f $pitch0, [double]$hip.pitch, $hipDelta)
  if ($hipDelta -lt 0.4) { Write-Host "hip recoil weak (pitch did not climb much)" }

  $yaw = [math]::Round((FaceYaw (Seat $seatA) (Seat $seatB)), 1)
  Plan $seatA @(
    @{ seconds = 0.25; ads = $true; look = @{ yawAbs = $yaw; pitchAbs = 0 } },
    @{ seconds = 0.45; ads = $true; fire = $true; look = @{ yawAbs = $yaw; pitchAbs = 0 } }
  ) | Out-Null
  Start-Sleep -Milliseconds 550
  $adsMid = Seat $seatA
  WaitIdle $seatA 3 | Out-Null
  $adsEnd = Seat $seatA
  Write-Host ("ads recoil punch={0:N2} pitch={1:N1}" -f [double]$adsMid.recoilPitch, [double]$adsEnd.pitch)
}

Write-Host "cover holds: A behind menhir, B lee of center hide"
AppendBook $seatB "cover_then_peek" | Out-Null
$coverA = @"
@startuml jit_cover_a
start
:goto marker=menhir_0_approach;
note right
  success: distXY 450
  goodEnough: distXY 800
  fail.timeout: 50
end note
stop
@enduml
"@
Hub @{ type = "appendBotBook"; seatId = $seatA; puml = $coverA } | Out-Null
WaitBot $seatB 50 | Out-Null
WaitBot $seatA 55 | Out-Null
WaitAlive $seatA 5 | Out-Null
WaitAlive $seatB 5 | Out-Null
AppendBook $seatB "hold_lee" | Out-Null
Write-Host "B hold_lee until megalith 8/8"
if ($Sequence -ne "radar") {
  EdgeHop
}
HoldRing
$script:lintelSticks = 0

Write-Host "view A"
SetView $seatA
if ($Sequence -eq "radar") {
  Write-Host "sequence=radar (sighted + LOS/still/cover + view B HUD)"
  SightedLos
  RadarLos
  Write-Host "view B radar"
  SetView $seatB
  RadarViewB
} else {
  Write-Host "sequence=ring (BotBooks: cover, hold_lee, edge_pad, ring_lap, megalith_hop, court_gunfight)"
  AppendBook $seatA "ring_lap" | Out-Null
  $dived = $false
  if ($script:edgeDived) { $dived = $true; Write-Host "diving=true (from EdgeHop)" }
  $untilLap = (Get-Date).AddSeconds(140)
  while ((Get-Date) -lt $untilLap) {
    Start-Sleep -Milliseconds 200
    $st = Seat $seatA
    AssertNoCollapse $st $seatA
    if ($st.diving -eq $true) { $dived = $true; Write-Host "diving=true" }
    if (-not (BookBusy $st) -and -not $st.goto) { break }
  }
  if (-not $dived) {
    $st = Seat $seatA
    if ($st.diving -eq $true) { $dived = $true; Write-Host "diving=true" }
  }
  if (-not $dived) { throw "diving never true (EdgeHop Recast Launch / catalog airDive verb - no JIT fallback)" }
  WaitBot $seatA 20 | Out-Null
  Write-Host "megalith hop (B still hold_lee)"
  AppendBook $seatA "megalith_hop" | Out-Null
  $stuck = @{}
  $degs = @(0, 45, 90, 135, 180, 225, 270, 315)
  $hopUntil = (Get-Date).AddSeconds(180)
  $hopStart = Get-Date
  while ((Get-Date) -lt $hopUntil) {
    Start-Sleep -Milliseconds 150
    $st = Seat $seatA
    AssertNoCollapse $st $seatA
    foreach ($deg in $degs) {
      if (OnLintel $st $deg) { $stuck["$deg"] = 1 }
    }
    if (-not (BookBusy $st) -and -not $st.goto) { break }
  }
  $script:lintelSticks = $stuck.Count
  Write-Host ("megalith sticks={0}/8 elapsed={1:N1}" -f $script:lintelSticks, ((Get-Date) - $hopStart).TotalSeconds)
  if ($script:lintelSticks -lt 8) {
    Write-Host "megalith retry missing stations"
    foreach ($deg in $degs) {
      if ($stuck.ContainsKey("$deg")) { continue }
      $idx = [int]([math]::Round($deg / 45.0))
      Write-Host ("retry menhir_{0} deg={1}" -f $idx, $deg)
      $puml = @"
@startuml megalith_retry
start
:airDive marker=menhir_$idx;
note right
  success: distXY 180
  goodEnough: distXY 280
  fail.timeout: 10
end note
stop
@enduml
"@
      Hub @{ type = "appendBotBook"; seatId = $seatA; puml = $puml } | Out-Null
      $retryUntil = (Get-Date).AddSeconds(14)
      while ((Get-Date) -lt $retryUntil) {
        Start-Sleep -Milliseconds 120
        $st = Seat $seatA
        AssertNoCollapse $st $seatA
        if (OnLintel $st $deg) { $stuck["$deg"] = 1 }
        if (-not (BookBusy $st) -and -not $st.goto -and $st.air -ne $true -and $st.diving -ne $true) { break }
      }
    }
    $script:lintelSticks = $stuck.Count
    Write-Host ("megalith sticks after retry={0}/8" -f $script:lintelSticks)
  }
  if ($script:lintelSticks -lt 8) { throw "megalith sticks $($script:lintelSticks)/8" }
  Write-Host "court gunfight: B court_center slide/dash, A track_fire"
  DropOffLintel $seatA
  HoldRing
  BranchBook $seatB "court_gunfight" | Out-Null
  AppendBook $seatA "track_fire" | Out-Null
  WaitAlive $seatA 8 | Out-Null
  WaitAlive $seatB 8 | Out-Null
  WaitBot $seatA 40 | Out-Null
  WaitBot $seatB 40 | Out-Null
  RecoilDemo
}
Write-Host "restore host view"
SetView $hostSeat.id

Write-Host "home"
$homeA = GotoOne $seatA -6380 0 98 800
$homeB = GotoOne $seatB 6380 0 98 800
Write-Host ("A home x={0:N0} B home x={1:N0}" -f $homeA.x, $homeB.x)
if ([math]::Abs($homeA.x + 6380) -gt 800) { throw "A missed red home" }
if ([math]::Abs($homeB.x - 6380) -gt 800) { throw "B missed blue home" }
Write-Host "VERIFY_OK"
