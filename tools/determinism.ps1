# Per-turn sync checksums of new games, headless and in the map window, for vanilla and each mod
# game core: the game must play the same whatever shows it, and a change that should not touch
# the rules must not change them. New games from fixed seeds, so nothing but the install is needed.
#   tools\determinism.ps1 [-Build build\Release] [-Turns 8] [-Out build\tests]
#   (-Build another build folder, e.g. of an older commit, to compare with)
param(
	[string]$Build = "",
	[int]$Turns = 8,
	[string]$Out = "",
	[string]$And2 = "",
	[string]$Com = ""
)
$root = Split-Path -Parent $PSScriptRoot
if (-not $Build) { $Build = Join-Path $root "build\Release" }
if (-not $Out) { $Out = Join-Path $root "build\tests" }
if (-not $And2) { $And2 = Join-Path $root "external\mods\and2-assets-r1091\Rise of Mankind - A New Dawn" }
New-Item -ItemType Directory -Force $Out | Out-Null
$tag = (Split-Path -Leaf (Split-Path -Parent $Build)) + "-" + (Split-Path -Leaf $Build)
$game = @("--map", "Continents", "--size", "SMALL", "--seed", "3", "--turns", "$Turns")
$runs = @(@{ Name = "vanilla"; Exe = "$Build\bin\oc4core.exe"; Mod = "" })
if (Test-Path "$Build\mods\and2\oc4core.exe") { $runs += @{ Name = "and2"; Exe = "$Build\mods\and2\oc4core.exe"; Mod = $And2 } }
if ($Com -and (Test-Path "$Build\mods\com\oc4core.exe")) { $runs += @{ Name = "com"; Exe = "$Build\mods\com\oc4core.exe"; Mod = $Com } }
$procs = @()
foreach ($r in $runs) {
	foreach ($mode in "headless", "window") {
		$a = @()
		if ($r.Mod) { $a += @("--mod", "`"$($r.Mod)`"") }
		$a += $game
		if ($mode -eq "window") { $a += @("--view", "--view-seconds", "$(20 + 10 * $Turns)") }
		$log = "$Out\det-$tag-$($r.Name)-$mode.log"
		$p = Start-Process -FilePath $r.Exe -ArgumentList $a -RedirectStandardOutput $log -RedirectStandardError "$log.err" -PassThru -NoNewWindow
		$null = $p.Handle
		$procs += $p
	}
}
foreach ($p in $procs) { if (-not $p.WaitForExit(900000)) { $p.Kill() } }
foreach ($r in $runs) {
	foreach ($mode in "headless", "window") {
		$log = "$Out\det-$tag-$($r.Name)-$mode.log"
		$sums = Select-String -Path $log -Pattern "^\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+([0-9A-F]{8})" | ForEach-Object { $_.Matches[0].Groups[1].Value }
		"{0,-8} {1,-9} {2}" -f $r.Name, $mode, ($sums -join " ")
	}
}
