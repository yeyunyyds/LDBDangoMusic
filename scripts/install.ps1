param(
    [Parameter(Mandatory=$true)][string]$GameRoot,
    [ValidateRange(-10000,10000)][int]$OffsetMs = 500
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$sourceDll = Join-Path $repo 'build/LDBDangoMusic.dll'
if (!(Test-Path $sourceDll)) { throw 'Build the DLL first or use the source directory inside the ZIP.' }
$hostRoot = Join-Path $GameRoot 'bin/.l4d2bridge'
if (!(Test-Path (Join-Path $hostRoot 'L4D2Bridge64.exe'))) { throw 'x64 LDB Host not found under GameRoot/bin/.l4d2bridge' }
$target = Join-Path $hostRoot 'plugins'
$settingsDir = Join-Path $target 'LDBDangoMusic'
New-Item -ItemType Directory -Force $settingsDir | Out-Null
$path = Join-Path $settingsDir 'config.json'
$defaults = Get-Content -Raw (Join-Path $repo 'config.example.json') | ConvertFrom-Json
# Parse before replacing any existing files. Preserve existing mode, port and visual preferences.
if (Test-Path $path) {
    $settings = Get-Content -Raw $path | ConvertFrom-Json
    if ($null -eq $settings -or $settings -isnot [pscustomobject]) { throw 'Existing config must be a JSON object' }
} else { $settings = $defaults }
foreach ($property in $defaults.PSObject.Properties) {
    if ($null -eq $settings.PSObject.Properties[$property.Name]) {
        $settings | Add-Member -NotePropertyName $property.Name -NotePropertyValue $property.Value
    }
}
$settings | Add-Member -NotePropertyName offsetMs -NotePropertyValue $OffsetMs -Force
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-ffff'
if (Test-Path $path) { Copy-Item $path ($path + '.' + $stamp + '.bak') }
$targetDll = Join-Path $target 'LDBDangoMusic.dll'
if (Test-Path $targetDll) { Copy-Item $targetDll ($targetDll + '.' + $stamp + '.bak') }
# A locked DLL causes a terminating error; exit the game and Host before installation.
Copy-Item $sourceDll $targetDll -Force
[IO.File]::WriteAllText($path, ($settings | ConvertTo-Json -Depth 16), [System.Text.UTF8Encoding]::new($false))
Write-Host "Installed 0.3.1, preserved existing config; offsetMs=$OffsetMs. Restart game and Host."
