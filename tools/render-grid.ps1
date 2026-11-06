# Renders several models with nifview --screenshot and tiles them into one PNG.
# Usage: tools\render-grid.ps1 -Civ4 <root> -Out grid.png -Models key1,key2,... [-Size 360] [-Columns 4] [-Extra '--no-skin']
param(
	[Parameter(Mandatory)] [string] $Civ4,
	[Parameter(Mandatory)] [string] $Out,
	[Parameter(Mandatory)] [string[]] $Models,
	[int] $Size = 360,
	[int] $Columns = 4,
	[string[]] $Extra = @(),
	[string] $Viewer = "$PSScriptRoot\..\build\release\bin\nifview.exe"
)
Add-Type -AssemblyName System.Drawing
$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("oc4grid_" + [guid]::NewGuid())
New-Item -ItemType Directory $tmp | Out-Null
try {
	$rows = [math]::Ceiling($Models.Count / $Columns)
	$bmp = New-Object System.Drawing.Bitmap ($Columns * $Size), ($rows * ($Size + 18))
	$g = [System.Drawing.Graphics]::FromImage($bmp)
	$g.Clear([System.Drawing.Color]::Black)
	$font = New-Object System.Drawing.Font 'Arial', 9
	for ($i = 0; $i -lt $Models.Count; $i++) {
		$png = Join-Path $tmp "$i.png"
		$log = & $Viewer --civ4 $Civ4 --screenshot $png --size "${Size}x${Size}" @Extra $Models[$i] 2>&1
		$log | Select-Object -First 1 | Write-Host
		$x = ($i % $Columns) * $Size
		$y = [math]::Floor($i / $Columns) * ($Size + 18)
		$label = Split-Path $Models[$i] -Leaf
		if (Test-Path $png) {
			$img = [System.Drawing.Image]::FromFile($png)
			$g.DrawImage($img, $x, $y + 18, $Size, $Size)
			$img.Dispose()
		} else { $label += ' (failed)' }
		$g.DrawString($label, $font, [System.Drawing.Brushes]::Yellow, $x + 3, $y + 2)
	}
	$bmp.Save($Out)
	$g.Dispose(); $bmp.Dispose()
} finally {
	Remove-Item -Recurse -Force $tmp
}
