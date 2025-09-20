# build-all.ps1
# Compila HypnoS in 4 varianti e salva gli eseguibili in .\bin\

$arches = @{
    "avx2"    = "x86-64-avx2"
    "bmi2"    = "x86-64-bmi2"
    "sse41"   = "x86-64-sse41-popcnt"
    "generic" = "x86-64"
}

$binDir = Join-Path $PSScriptRoot "bin"
if (-not (Test-Path $binDir)) {
    New-Item -ItemType Directory -Path $binDir | Out-Null
}

foreach ($name in $arches.Keys) {
    $arch = $arches[$name]

    Write-Host "▶ Compilo variante $name ($arch)..." -ForegroundColor Cyan
    mingw32-make clean 2>$null | Out-Null
    mingw32-make profile-build ARCH=$arch COMP=mingw -j 12

    $exe = Join-Path $PSScriptRoot "hypnos.exe"
    if (Test-Path $exe) {
        $target = Join-Path $binDir ("hypnos-" + $name + ".exe")
        Move-Item -Force $exe $target
        Write-Host "✔ Creato $target" -ForegroundColor Green
    } else {
        Write-Host "✘ Nessun exe trovato per $name" -ForegroundColor Red
    }
}

Write-Host "Tutte le build completate. Gli eseguibili sono in $binDir" -ForegroundColor Yellow
