param([Parameter(Mandatory=$true)][string]$GameRoot,[ValidateRange(-10000,10000)][int]$OffsetMs)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$sourceDll = Join-Path $repo 'build/L4D2DangoMusic.dll'
if (!(Test-Path $sourceDll)) { throw 'Build the x86 DLL or run from the ZIP source directory.' }
$neko = Join-Path $GameRoot 'bin/neko'
if (!(Test-Path $neko)) { throw 'L4N bin/neko not found.' }
$target = Join-Path $neko 'plugins'
$settingsDir = Join-Path $target 'L4D2DangoMusic'
$path = Join-Path $settingsDir 'config.json'
$defaults = Get-Content -Raw -Encoding UTF8 (Join-Path $repo 'config.example.json') | ConvertFrom-Json
# Explicit legacy names are retained only for migration, not as active product names.
$legacy = 'LDBDangoMusic'
$candidates = @($path, (Join-Path $target "$legacy/config.json"), (Join-Path $GameRoot "bin/.l4d2bridge/plugins/$legacy/config.json"))
$sourceConfig = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
$settings = if ($sourceConfig) { Get-Content -Raw -Encoding UTF8 $sourceConfig | ConvertFrom-Json } else { $defaults }
if ($null -eq $settings -or $settings -isnot [pscustomobject]) { throw 'Config must be a JSON object.' }
foreach ($p in $defaults.PSObject.Properties) { if ($null -eq $settings.PSObject.Properties[$p.Name]) { $settings | Add-Member -NotePropertyName $p.Name -NotePropertyValue $p.Value } }
if ($PSBoundParameters.ContainsKey('OffsetMs')) { $settings | Add-Member -NotePropertyName offsetMs -NotePropertyValue $OffsetMs -Force }
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-ffff'
New-Item -ItemType Directory -Force $settingsDir | Out-Null
if (Test-Path $path) { Copy-Item $path ($path + '.' + $stamp + '.bak') }
$dll = Join-Path $target 'L4D2DangoMusic.dll'
if (Test-Path $dll) { Copy-Item $dll ($dll + '.' + $stamp + '.bak') }
# Exit game and Host first. Keep old configurations, and disable only old lyrics DLLs.
foreach ($old in @((Join-Path $target "$legacy.dll"), (Join-Path $GameRoot "bin/.l4d2bridge/plugins/$legacy.dll"), (Join-Path $GameRoot 'bin/.l4d2bridge/plugins/L4D2DangoMusic.dll'))) {
    if (Test-Path $old) { Move-Item $old ($old + '.' + $stamp + '.disabled') }
}
Copy-Item $sourceDll $dll -Force
[IO.File]::WriteAllText($path, ($settings | ConvertTo-Json -Depth 16), [System.Text.UTF8Encoding]::new($false))
Write-Host 'Installed L4D2DangoMusic 0.6.0; existing settings migrated. Restart game, open the L4N menu.'
