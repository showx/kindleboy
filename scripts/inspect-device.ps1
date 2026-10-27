<#
.SYNOPSIS
Read Kindle USB metadata without changing the device or reading books.
.EXAMPLE
pwsh -File scripts/inspect-device.ps1 -DriveRoot H:\
#>
[CmdletBinding()]
param(
    [Parameter()]
    [string]$DriveRoot
)

$ErrorActionPreference = 'Stop'

function Test-KindleRoot {
    param([string]$Root)
    $versionPath = Join-Path $Root 'system\version.txt'
    if (-not (Test-Path -LiteralPath $versionPath -PathType Leaf)) {
        return $false
    }
    $versionText = Get-Content -LiteralPath $versionPath -Raw
    return $versionText -match '^Kindle\s+\d+(?:\.\d+)+'
}

try {
    if (-not $DriveRoot) {
        $candidates = @(Get-CimInstance Win32_LogicalDisk |
            Where-Object { $_.DriveType -eq 2 -or $_.VolumeName -eq 'Kindle' } |
            ForEach-Object { $_.DeviceID + '\' } |
            Where-Object { Test-KindleRoot $_ })
        if ($candidates.Count -ne 1) {
            throw "Found $($candidates.Count) Kindle volumes. Connect one device or specify -DriveRoot."
        }
        $DriveRoot = $candidates[0]
    }

    $rootPath = (Resolve-Path -LiteralPath $DriveRoot).ProviderPath
    if (-not (Test-KindleRoot $rootPath)) {
        throw 'The selected path does not contain Kindle system/version.txt metadata.'
    }
    $versionText = (Get-Content -LiteralPath (Join-Path $rootPath 'system\version.txt') -Raw).Trim()
    if ($versionText -notmatch '^Kindle\s+(?<firmware>\d+(?:\.\d+)+)(?:\s+\((?<build>[\d ]+)\))?\s*$') {
        throw 'Unexpected version metadata format; check the firmware on the Kindle screen.'
    }
    $firmware = $Matches.firmware
    $firmwareBuild = $Matches.build
    $extensionPath = Join-Path $rootPath 'extensions'
    $kualPaths = @(
        'documents\KUAL-KDK-1.0.azw2',
        'documents\KUAL-KDK-2.0.azw2',
        'documents\KUAL.azw2',
        'documents\KUAL.jar',
        'documents\KUAL.sh'
    )
    $launcherFiles = @($kualPaths | Where-Object {
        Test-Path -LiteralPath (Join-Path $rootPath $_) -PathType Leaf
    })

    [ordered]@{
        schemaVersion = 1
        driveRoot = $rootPath
        firmware = $firmware
        firmwareBuild = $firmwareBuild
        model = $null
        modelStatus = 'Needs confirmation from Settings > Device Info; USB volume does not identify the model.'
        jailbreakStatus = 'unknown'
        hasExtensionsDirectory = (Test-Path -LiteralPath $extensionPath -PathType Container)
        hasMrPackagesDirectory = (Test-Path -LiteralPath (Join-Path $rootPath 'mrpackages') -PathType Container)
        knownLauncherFiles = $launcherFiles
        note = 'File presence alone cannot prove whether the Kindle is jailbroken. No device files were modified.'
    } | ConvertTo-Json -Depth 3
} catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
}

