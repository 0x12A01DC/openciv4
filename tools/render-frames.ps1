# Renders animation frames of one model into a contact sheet: one row per animation.
# Usage: tools\render-frames.ps1 -Civ4 <root> -Model art/units/axeman/axeman.nif -Out sheet.png `
#            -Anims run,strikea,diea -Times 0,0.2,0.4,0.6 [-Size 300] [-Extra '--yaw','90']
param(
	[Parameter(Mandatory)] [string] $Civ4,
	[Parameter(Mandatory)] [string] $Model,
	[Parameter(Mandatory)] [string] $Out,
	[Parameter(Mandatory)] [string[]] $Anims,
	[Parameter(Mandatory)] [double[]] $Times,
	[int] $Size = 300,
	[string[]] $Extra = @(),
	[string] $Viewer = "$PSScriptRoot\..\build\release\bin\nifview.exe"
)
$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("oc4frames_" + [guid]::NewGuid())
New-Item -ItemType Directory $tmp | Out-Null
try {
	$images = @(); $labels = @()
	foreach ($a in $Anims) {
		foreach ($t in $Times) {
			$png = Join-Path $tmp ("{0}_{1}.png" -f $images.Count, $a)
			& $Viewer --civ4 $Civ4 --screenshot $png --size "${Size}x${Size}" --anim $a --time $t @Extra $Model | Out-Null
			$images += $png
			$labels += ("{0} @ {1:0.00}s" -f $a, $t)
		}
	}
	& "$PSScriptRoot\tile-images.ps1" -Out $Out -Images $images -Labels $labels -Columns $Times.Count
} finally {
	Remove-Item -Recurse -Force $tmp
}
