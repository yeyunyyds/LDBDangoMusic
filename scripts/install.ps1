param([Parameter(Mandatory=$true)][string]$GameRoot,[ValidateRange(-10000,10000)][int]$OffsetMs=500)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$sourceDll = Join-Path $repo 'build/LDBDangoMusic.dll'
if (!(Test-Path $sourceDll)) { throw 'Build the x86 DLL or use the ZIP source directory.' }
$neko = Join-Path $GameRoot 'bin/neko'
if (!(Test-Path $neko)) { throw 'L4N bin/neko not found; install L4N SDK v2 compatible loader first.' }
$target = Join-Path $neko 'plugins'
$settingsDir = Join-Path $target 'LDBDangoMusic'
$path = Join-Path $settingsDir 'config.json'
$oldRoot = Join-Path $GameRoot 'bin/.l4d2bridge/plugins'
$oldConfig = Join-Path $oldRoot 'LDBDangoMusic/config.json'
$defaults = Get-Content -Raw (Join-Path $repo 'config.example.json') | ConvertFrom-Json
$sourceConfig = if (Test-Path $path) { $path } elseif (Test-Path $oldConfig) { $oldConfig } else { $null }
$settings = if ($sourceConfig) { Get-Content -Raw $sourceConfig | ConvertFrom-Json } else { $defaults }
if ($null -eq $settings -or $settings -isnot [pscustomobject]) { throw 'Config must be a JSON object.' }
foreach ($p in $defaults.PSObject.Properties) { if ($null -eq $settings.PSObject.Properties[$p.Name]) { $settings | Add-Member -NotePropertyName $p.Name -NotePropertyValue $p.Value } }
$settings | Add-Member -NotePropertyName offsetMs -NotePropertyValue $OffsetMs -Force
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-ffff'
New-Item -ItemType Directory -Force $settingsDir | Out-Null
if (Test-Path $path) { Copy-Item $path ($path + '.' + $stamp + '.bak') }
$dll = Join-Path $target 'LDBDangoMusic.dll'
if (Test-Path $dll) { Copy-Item $dll ($dll + '.' + $stamp + '.bak') }
# Exit game and Host first. Disable only our legacy Host plugin, preserving other plugins.
$oldDll = Join-Path $oldRoot 'LDBDangoMusic.dll'
if (Test-Path $oldDll) { Move-Item $oldDll ($oldDll + '.' + $stamp + '.disabled') }
Copy-Item $sourceDll $dll -Force
[IO.File]::WriteAllText($path, ($settings | ConvertTo-Json -Depth 16), [System.Text.UTF8Encoding]::new($false))
Write-Host 'Installed pure x86 L4N 0.4.3, migrated settings; legacy Host DLL disabled. Fully restart game.'
