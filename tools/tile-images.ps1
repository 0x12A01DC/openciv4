# Tiles PNG files into one labelled contact sheet.
# Usage: tools\tile-images.ps1 -Out sheet.png -Images a.png,b.png [-Labels 'a','b'] [-Columns 4]
param(
	[Parameter(Mandatory)] [string] $Out,
	[Parameter(Mandatory)] [string[]] $Images,
	[string[]] $Labels = @(),
	[int] $Columns = 4
)
Add-Type -AssemblyName System.Drawing
$first = [System.Drawing.Image]::FromFile($Images[0])
$w = $first.Width; $h = $first.Height
$first.Dispose()
$rows = [math]::Ceiling($Images.Count / $Columns)
$bmp = New-Object System.Drawing.Bitmap ([math]::Min($Columns, $Images.Count) * $w), ($rows * ($h + 18))
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.Clear([System.Drawing.Color]::Black)
$font = New-Object System.Drawing.Font 'Arial', 9
for ($i = 0; $i -lt $Images.Count; $i++) {
	$x = ($i % $Columns) * $w
	$y = [math]::Floor($i / $Columns) * ($h + 18)
	if (Test-Path $Images[$i]) {
		$img = [System.Drawing.Image]::FromFile($Images[$i])
		$g.DrawImage($img, $x, $y + 18, $w, $h)
		$img.Dispose()
	}
	$label = if ($i -lt $Labels.Count) { $Labels[$i] } else { Split-Path $Images[$i] -Leaf }
	$g.DrawString($label, $font, [System.Drawing.Brushes]::Yellow, $x + 3, $y + 2)
}
$bmp.Save($Out)
$g.Dispose(); $bmp.Dispose()
