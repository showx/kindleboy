[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$DriveRoot)
$ErrorActionPreference = 'Stop'
$metadata = Join-Path $DriveRoot 'system\version.txt'
if (-not (Test-Path -LiteralPath $metadata)) { throw 'Not a Kindle USB volume.' }
[IO.File]::WriteAllBytes((Join-Path (Resolve-Path -LiteralPath $DriveRoot).ProviderPath 'DISABLE_KINDLEBOY'), [byte[]]@())
Write-Output 'Autostart disabled. Safely eject the Kindle and restart to use the reading UI.'
