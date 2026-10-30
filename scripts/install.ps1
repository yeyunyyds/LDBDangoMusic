param([Parameter(Mandatory=$true)][string]$GameRoot,[ValidateRange(-10000,10000)][int]$OffsetMs)
$ErrorActionPreference = 'Stop'
if (Get-Process -Name left4dead2 -ErrorAction SilentlyContinue) { throw 'Exit Left 4 Dead 2 before installing; also close any legacy lyrics Host if used.' }
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
# Migrate legacy custom distance before filling the new per-component defaults.
if ($null -eq $settings.PSObject.Properties['layout']) {
    $layout = $defaults.layout | ConvertTo-Json -Depth 16 | ConvertFrom-Json
    if ($null -ne $settings.PSObject.Properties['worldDistance']) {
        foreach ($position in $layout.lyrics) { $position.distance = $settings.worldDistance }
    }
    $settings | Add-Member -NotePropertyName layout -NotePropertyValue $layout
}
# Leave legacy configs unversioned until the runtime performs its one-time
# defaults migration; saving in-game then writes defaultsVersion=2.
foreach ($p in $defaults.PSObject.Properties) {
    if ($p.Name -eq 'defaultsVersion' -and $sourceConfig -and $null -eq $settings.PSObject.Properties['defaultsVersion']) { continue }
    if ($null -eq $settings.PSObject.Properties[$p.Name]) { $settings | Add-Member -NotePropertyName $p.Name -NotePropertyValue $p.Value }
}
if ($PSBoundParameters.ContainsKey('OffsetMs')) { $settings | Add-Member -NotePropertyName offsetMs -NotePropertyValue $OffsetMs -Force }
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-ffff'
New-Item -ItemType Directory -Force $settingsDir | Out-Null
if (Test-Path $path) { Copy-Item $path ($path + '.' + $stamp + '.bak') }
$dll = Join-Path $target 'L4D2DangoMusic.dll'
# Backups belong outside plugin search paths, including files renamed .dll.bak.
$backupRoot = Join-Path $neko ("plugins-disabled/L4D2DangoMusic-" + $stamp)
if (Test-Path $dll) {
    New-Item -ItemType Directory -Force (Join-Path $backupRoot 'client') | Out-Null
    Copy-Item $dll (Join-Path $backupRoot 'client/L4D2DangoMusic.dll')
}
# Identify this project's PE binaries by embedded product marker, rather than
# filename alone. This catches renamed copies without touching other plugins.
$locations = @(
    @{ Path = $target; Scope = 'client' },
    @{ Path = (Join-Path $GameRoot 'bin/.l4d2bridge/plugins'); Scope = 'host' }
)
foreach ($location in $locations) {
    if (!(Test-Path $location.Path)) { continue }
    foreach ($file in Get-ChildItem -LiteralPath $location.Path -File) {
        if ($file.FullName -eq [IO.Path]::GetFullPath($dll)) { continue }
        if ($file.Extension -notin @('.dll', '.bak', '.disabled') -and $file.Name -notmatch 'DangoMusic') { continue }
        $bytes = [IO.File]::ReadAllBytes($file.FullName)
        if ($bytes.Length -lt 64 -or $bytes[0] -ne 77 -or $bytes[1] -ne 90) { continue }
        $marker = [Text.Encoding]::ASCII.GetString($bytes)
        if ($marker -notmatch '(L4D2DangoMusic|LDBDangoMusic)' -or $marker -notmatch '(GetL4NPluginInstance|LDBHostPluginQuery)') { continue }
        $backup = Join-Path $backupRoot $location.Scope
        New-Item -ItemType Directory -Force $backup | Out-Null
        Move-Item -LiteralPath $file.FullName -Destination (Join-Path $backup $file.Name)
        Write-Host ('Disabled extra lyrics binary: ' + $file.FullName)
    }
}
Copy-Item $sourceDll $dll -Force
[IO.File]::WriteAllText($path, ($settings | ConvertTo-Json -Depth 16), [System.Text.UTF8Encoding]::new($false))
Write-Host 'Installed L4D2DangoMusic 0.9.3; settings preserved. Restart game; use the L4D2DangoMusic 0.9.3 menu.'

Write-Host ('Active DLL: ' + [IO.Path]::GetFullPath($dll))
Write-Host ('Backups outside plugin search paths: ' + $backupRoot)
