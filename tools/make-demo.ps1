# Packs a demo of openciv4 for one mod or the unmodded game: the engine, the game core rebuilt as
# 64-bit, Python 2.7 and the runtimes they need, launchers and a readme. No game or mod assets: the
# player has Beyond the Sword and the mod installed.
#   tools\make-demo.ps1 [-Mod vanilla|and2|com] [-Out build\dist] [-Build build\Release]
param([string]$Mod = "and2", [string]$Out = "build\dist", [string]$Version = "", [string]$Build = "build\Release")
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$bin = if ($Mod -eq "vanilla") { Join-Path $root "$Build\bin" } else { Join-Path $root "$Build\mods\$Mod" }
$python = Join-Path $root "external\python27"
if (-not (Test-Path "$bin\oc4core.exe")) { throw "build it first: build.cmd release ($bin\oc4core.exe)" }
if (-not $Version) { $Version = (git -C $root rev-parse --short HEAD).Trim() }
$name = "openciv4-demo-$Mod-$Version"
$stage = Join-Path $root "$Out\$name"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $stage | Out-Null

# The engine and the game core
Copy-Item "$bin\oc4core.exe", "$bin\CvGameCoreDLL-x64.dll", "$bin\python27.dll" $stage

# FFmpeg's libraries for the movies (LGPL), if the build has them
$ffmpeg = @("avutil-60.dll", "avcodec-62.dll", "avformat-62.dll", "swresample-6.dll") | ForEach-Object { "$bin\$_" }
if (Test-Path $ffmpeg[0]) {
	Copy-Item $ffmpeg $stage
	Copy-Item "$root\external\ffmpeg-8.1\LICENSE.txt" "$stage\FFmpeg-LICENSE.txt"
}

# The Visual C++ runtime they are built with (the newest redistributable the compiler has)
$redist = Get-ChildItem "C:\Program Files\Microsoft Visual Studio\*\*\VC\Redist\MSVC\*\x64\Microsoft.VC14*.CRT" -Directory |
	Sort-Object FullName -Descending | Select-Object -First 1
if (-not $redist) { throw "no Visual C++ redistributable found" }
Copy-Item "$($redist.FullName)\msvcp140.dll", "$($redist.FullName)\vcruntime140.dll", "$($redist.FullName)\vcruntime140_1.dll" $stage

# Python 2.7's own runtime (Visual C++ 2008), privately beside python27.dll: the version its manifest asks for
$vc90 = Get-ChildItem "C:\Windows\WinSxS" -Directory -Filter "amd64_microsoft.vc90.crt_1fc8b3b9a1e18e3b_*" | Sort-Object Name -Descending | Select-Object -First 1
if (-not $vc90) { throw "no Visual C++ 2008 x64 runtime found in WinSxS" }
$crt = New-Item -ItemType Directory -Force "$stage\Microsoft.VC90.CRT"
Copy-Item "$($vc90.FullName)\msvcr90.dll", "$($vc90.FullName)\msvcp90.dll", "$($vc90.FullName)\msvcm90.dll" $crt
@'
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <noInheritable/>
  <assemblyIdentity type="win32" name="Microsoft.VC90.CRT" version="9.0.21022.8" processorArchitecture="amd64" publicKeyToken="1fc8b3b9a1e18e3b"/>
  <file name="msvcr90.dll"/>
  <file name="msvcp90.dll"/>
  <file name="msvcm90.dll"/>
</assembly>
'@ | Set-Content -Encoding UTF8 "$crt\Microsoft.VC90.CRT.manifest"

# Python's standard library (the engine finds it in "python" beside itself), without tests and Tk
$py = New-Item -ItemType Directory -Force "$stage\python"
robocopy "$python\Lib" "$py\Lib" /E /NFL /NDL /NJH /NJS /NP /XD test tests lib-tk idlelib lib2to3 ensurepip site-packages msilib bsddb /XF *.pyc *.pyo | Out-Null
New-Item -ItemType Directory -Force "$py\DLLs" | Out-Null
Get-ChildItem "$python\DLLs" -Filter *.pyd | Where-Object { $_.Name -notmatch "tkinter|_test|_msi|_bsddb" } | Copy-Item -Destination "$py\DLLs"
Copy-Item "$python\DLLs\sqlite3.dll" "$py\DLLs"
Copy-Item "$python\LICENSE.txt" "$py\LICENSE.txt"

# The launchers and the readme; for testing ported game cores, the export check
Copy-Item "$root\tools\demo\$Mod\*" $stage
if ($Mod -eq "vanilla") {
	New-Item -ItemType Directory -Force "$stage\tools" | Out-Null
	Copy-Item "$root\gamecore\tools\check-exports.ps1" "$stage\tools"
}

$zip = Join-Path $root "$Out\$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
# (Windows' tar: Compress-Archive of Windows PowerShell writes backslashes into the zip's paths)
& "$env:SystemRoot\System32\tar.exe" -a -c -f $zip -C (Split-Path $stage) $name
if ($LASTEXITCODE -ne 0) { throw "tar failed" }
"{0}: {1:N1} MB" -f $zip, ((Get-Item $zip).Length / 1MB)
