# Stop UnrealEditor, build CallingEditor, relaunch. The editor locks
# UnrealEditor-Calling.dll; never Build.bat while it is running.
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
		$st = (Invoke-WebRequest -Uri "http://127.0.0.1:18765/state" -UseBasicParsing -TimeoutSec 3).Content | ConvertFrom-Json
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
	Invoke-WebRequest -Uri "http://127.0.0.1:18765/director" -Method POST -Body '{"action":"enter"}' -ContentType "application/json; charset=utf-8" -UseBasicParsing -TimeoutSec 20 | Out-Null
	$leaveBoot = (Get-Date).AddSeconds(45)
	while ((Get-Date) -lt $leaveBoot) {
		Start-Sleep -Seconds 1
		try {
			$st = (Invoke-WebRequest -Uri "http://127.0.0.1:18765/state" -UseBasicParsing -TimeoutSec 3).Content | ConvertFrom-Json
			$scene = [string]$st.scene
			if ($scene -and $scene -ne "boot") { break }
		} catch { }
	}
	if ($scene -eq "boot") {
		Write-Output "boot_timeout; director skipped"
		return
	}
}

$body = '{"action":"' + $Activity + '"}'
$r = Invoke-WebRequest -Uri "http://127.0.0.1:18765/director" -Method POST -Body $body -ContentType "application/json; charset=utf-8" -UseBasicParsing -TimeoutSec 20
Write-Output $r.Content
