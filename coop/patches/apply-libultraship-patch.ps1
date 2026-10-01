# Applies the co-op headless-host patch to the libultraship submodule (idempotent).
# Run once after `git submodule update --init`.
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$patch = Join-Path $root 'coop\patches\libultraship-headless.patch'
Push-Location (Join-Path $root 'libultraship')
try {
    git apply --check -R $patch 2>$null
    if ($LASTEXITCODE -eq 0) { Write-Host 'libultraship: co-op patch already applied.'; return }
    git apply --check $patch
    if ($LASTEXITCODE -ne 0) { throw 'The patch does not apply; the submodule is not at the expected commit (7f9b86a5).' }
    git apply $patch
    Write-Host 'libultraship: co-op patch applied.'
} finally { Pop-Location }
