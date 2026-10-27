[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$DriveRoot,
    [switch]$StageJailbreak,
    [switch]$DryRun
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$deviceInfo = & (Join-Path $PSScriptRoot 'inspect-device.ps1') -DriveRoot $DriveRoot | ConvertFrom-Json
if (-not $deviceInfo -or $deviceInfo.firmware -ne '5.15.1.1') {
    throw 'This prepared package targets the confirmed KT3 on firmware 5.15.1.1.'
}
$deviceRoot = [IO.Path]::GetFullPath($deviceInfo.driveRoot).TrimEnd('\') + '\'
$staging = Join-Path $projectRoot 'dist\staging'
$manifest = Get-Content -LiteralPath (Join-Path $staging 'manifest.json') -Raw | ConvertFrom-Json
$operations = @()
foreach ($entry in $manifest.files.PSObject.Properties) {
    $relative = $entry.Name
    if ($relative.Contains('..') -or $relative.Contains('\') -or $relative.StartsWith('/')) {
        throw 'Unsafe package manifest path.'
    }
    $source = Join-Path $staging $relative
    if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Value) {
        throw "Package checksum failed: $relative"
    }
    if ($relative -notmatch '^(extensions/kindleboy/|documents/KindleBoy(?:-Dedicated|-Setup)?\.sh$|roms/gb/KindleBoy-Demo\.gb$)') {
        continue
    }
    $operations += [pscustomobject]@{ Relative = $relative; Source = $source }
}
if ($StageJailbreak) {
    python (Join-Path $PSScriptRoot 'prepare-jailbreak.py')
    if ($LASTEXITCODE -ne 0) { throw 'WinterBreak2 package verification failed.' }
    $operations += [pscustomobject]@{
        Relative = 'winterbreak2/dialoger.html'
        Source = Join-Path $projectRoot 'dist\jailbreak\winterbreak2\dialoger.html'
    }
}
if ($DryRun) {
    Write-Output "Validated $($operations.Count) files for $deviceRoot; dry run made no device changes."
    return
}
$backupRoot = Join-Path $projectRoot ('.local\device-backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$copied = @()
foreach ($operation in $operations) {
    $target = [IO.Path]::GetFullPath((Join-Path $deviceRoot $operation.Relative))
    if (-not $target.StartsWith($deviceRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Target escaped the Kindle volume.'
    }
    if (Test-Path -LiteralPath $target -PathType Leaf) {
        if ($operation.Relative -eq 'extensions/kindleboy/kindleboy.cfg') { continue }
        if ((Get-FileHash -LiteralPath $target).Hash -eq (Get-FileHash -LiteralPath $operation.Source).Hash) { continue }
        $backup = Join-Path $backupRoot $operation.Relative
        New-Item -ItemType Directory -Path (Split-Path $backup -Parent) -Force | Out-Null
        Copy-Item -LiteralPath $target -Destination $backup
    }
    New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $operation.Source -Destination $target -Force
    if ((Get-FileHash -LiteralPath $target).Hash -ne (Get-FileHash -LiteralPath $operation.Source).Hash) {
        throw "Device copy verification failed: $($operation.Relative)"
    }
    $copied += $operation.Relative
}
$jailbreakStatus = 'Pending on-device execution; copying files does not jailbreak the Kindle.'
$successMarker = Join-Path $deviceRoot 'documents\JAILBROKEN.txt'
if ((Test-Path -LiteralPath $successMarker -PathType Leaf) -and
    ((Get-Content -LiteralPath $successMarker -TotalCount 1) -eq 'You are jailbroken!')) {
    $jailbreakStatus = 'Success marker found; application launch support requires separate verification.'
}
$report = [ordered]@{
    driveRoot = $deviceRoot
    model = 'KT3 (confirmed by serial prefix G000K9)'
    firmware = $deviceInfo.firmware
    copiedFiles = $copied
    backupRoot = if (Test-Path -LiteralPath $backupRoot) { $backupRoot } else { $null }
    jailbreakStatus = $jailbreakStatus
    hardwareValidation = $manifest.hardwareValidation
}
New-Item -ItemType Directory -Path (Join-Path $projectRoot '.local') -Force | Out-Null
$report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $projectRoot '.local\install-report.json') -Encoding UTF8
$report | ConvertTo-Json -Depth 4
