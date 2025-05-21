$ErrorActionPreference = 'Stop'

# Set working directory and define paths
$PSScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -Path $PSScriptRoot
$rootDir = Resolve-Path "$PSScriptRoot\.."
$buildDir = Join-Path $rootDir "build"

# Check if build directory exists, create if not
if (-not (Test-Path $buildDir)) {
    Write-Host "Creating build directory..."
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

# Move to build directory and run CMake if CMakeCache.txt doesn't exist
Push-Location $buildDir
if (-not (Test-Path "CMakeCache.txt")) {
    Write-Host "Running CMake for initial configuration..."
    cmake $rootDir `
        -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
        -DVCPKG_TARGET_TRIPLET="x64-windows-static" `
        -DCMAKE_BUILD_TYPE=Release
}

# Build the project
Write-Host "Building project..."
cmake --build . --config Release
Pop-Location

# Find and run the executable
$exeName = "LocalTether.exe"
$exeRelease = Join-Path $buildDir "Release\$exeName"
$exeDebug = Join-Path $buildDir "Debug\$exeName"
$exeDefault = Join-Path $buildDir $exeName

Write-Host "Looking for executable..."
if (Test-Path $exeRelease) {
    $exe = $exeRelease
    Write-Host "Found Release executable at: $exe"
} elseif (Test-Path $exeDebug) {
    $exe = $exeDebug
    Write-Host "Found Debug executable at: $exe"
} elseif (Test-Path $exeDefault) {
    $exe = $exeDefault
    Write-Host "Found executable at: $exe"
} else {
    Write-Error "Executable not found in Release, Debug, or build root folders."
    exit 1
}

# Run the executable
Write-Host "Launching LocalTether..."
& $exe

Write-Host "`n--- Done ---"