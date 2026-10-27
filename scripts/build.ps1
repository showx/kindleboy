[CmdletBinding()]
param([string]$BuilderImage)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    python scripts/fetch-deps.py --with-toolchain
    if ($LASTEXITCODE -ne 0) { throw 'Dependency download failed.' }
    python scripts/make-demo-rom.py
    if ($LASTEXITCODE -ne 0) { throw 'Demo ROM generation failed.' }
    if (-not $BuilderImage) {
        docker build -f Dockerfile.build -t kindleboy-builder:local .
        if ($LASTEXITCODE -ne 0) { throw 'Builder image failed. Check registry connectivity or pass an existing Debian 12 build image using -BuilderImage.' }
        $BuilderImage = 'kindleboy-builder:local'
    }
    docker run --rm --mount "type=bind,source=$projectRoot,target=/work" --workdir /work $BuilderImage sh scripts/build.sh
    if ($LASTEXITCODE -ne 0) { throw 'Build or verification failed.' }
    python scripts/package.py
    if ($LASTEXITCODE -ne 0) { throw 'Packaging failed.' }
} finally {
    Pop-Location
}
