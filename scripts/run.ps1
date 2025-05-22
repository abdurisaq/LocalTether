$ErrorActionPreference = 'Stop'

Set-Location -Path $PSScriptRoot

$rootDir = Resolve-Path "$PSScriptRoot\.."
$buildDir = Join-Path $rootDir "build"
$vcpkgDir = Join-Path $rootDir "vcpkg"

$env:VCPKG_ROOT = $vcpkgDir
$env:PATH = "$env:VCPKG_ROOT;$env:PATH"


New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
Push-Location $buildDir

cmake $rootDir `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET="x64-windows-static" `
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON `
  -DCMAKE_BUILD_TYPE=Release

cmake --build . --config Release

Pop-Location

$exeName = "LocalTether.exe"
$exeRelease = Join-Path $buildDir "Release\$exeName"
$exeDebug = Join-Path $buildDir "Debug\$exeName"
$exeDefault = Join-Path $buildDir $exeName

if (Test-Path $exeRelease) {
    $exe = $exeRelease
} elseif (Test-Path $exeDebug) {
    $exe = $exeDebug
} elseif (Test-Path $exeDefault) {
    $exe = $exeDefault
} else {
    Write-Error "Executable not found in Release, Debug, or build root folders."
    exit 1
}

Write-Host "Found executable at $exe"
Write-Host "Launching LocalTether..."
& $exe

Write-Host "`n--- Done ---"
