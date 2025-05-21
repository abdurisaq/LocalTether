$ErrorActionPreference = 'Stop'

Set-Location -Path $PSScriptRoot

$rootDir = Resolve-Path "$PSScriptRoot\.."

$buildDir = Join-Path $rootDir "build"
$vcpkgDir = Join-Path $rootDir "vcpkg"

Start-Transcript -Path "$PSScriptRoot\build_log.txt" -Append

Write-Host "Running build script as admin..."

$chocoPath = "C:\ProgramData\chocolatey\bin"
if (Test-Path $chocoPath) {
    $env:PATH = "$chocoPath;$env:PATH"
} else {
    Write-Warning "Chocolatey folder not found. Attempting installation..."
    Set-ExecutionPolicy Bypass -Scope Process -Force
    Invoke-Expression ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))
    if (!(Test-Path $chocoPath)) {
        Write-Error "Chocolatey installation failed or folder still missing: $chocoPath"
        Stop-Transcript
        exit 1
    }
    $env:PATH = "$chocoPath;$env:PATH"
}

if (-not (Get-Command choco -ErrorAction SilentlyContinue)) {
    Write-Error "Chocolatey still not found in PATH. Aborting."
    Stop-Transcript
    exit 1
}

choco install -y cmake git curl


if (!(Test-Path $vcpkgDir)) {
    git clone https://github.com/microsoft/vcpkg.git $vcpkgDir
    Push-Location $vcpkgDir
    .\bootstrap-vcpkg.bat
    Pop-Location
}

#env for vcpkg
$env:VCPKG_ROOT = $vcpkgDir
$env:PATH = "$env:VCPKG_ROOT;$env:PATH"

$externalBaseDir = "$PSScriptRoot\..\external"
$imguiDir = Join-Path $externalBaseDir "imgui"

if (!(Test-Path $imguiDir)) {
    Write-Host "Cloning Dear ImGui (docking branch)..."
    git clone --branch docking https://github.com/ocornut/imgui.git $imguiDir
} else {
    Write-Host "Dear ImGui already cloned, updating to latest docking branch..."
    Push-Location $imguiDir
    git fetch
    git checkout docking
    git pull
    Pop-Location
}

vcpkg install boost-asio:x64-windows-static sdl2:x64-windows-static openssl:x64-windows-static glad:x64-windows-static

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
Push-Location $buildDir

$imguiBackendsDir = Join-Path $externalBaseDir "imgui_backends"

$files = @{
    "imgui_impl_sdl2.h"        = "https://raw.githubusercontent.com/ocornut/imgui/docking/backends/imgui_impl_sdl2.h"
    "imgui_impl_sdl2.cpp"      = "https://raw.githubusercontent.com/ocornut/imgui/docking/backends/imgui_impl_sdl2.cpp"
    "imgui_impl_opengl3.h"     = "https://raw.githubusercontent.com/ocornut/imgui/docking/backends/imgui_impl_opengl3.h"
    "imgui_impl_opengl3.cpp"   = "https://raw.githubusercontent.com/ocornut/imgui/docking/backends/imgui_impl_opengl3.cpp"
}

if (-not (Test-Path $externalBaseDir)) {
    Write-Host "Creating directory: $externalBaseDir"
    New-Item -ItemType Directory -Path $externalBaseDir | Out-Null
}

if (-not (Test-Path $imguiBackendsDir)) {
    Write-Host "Creating directory: $imguiBackendsDir"
    New-Item -ItemType Directory -Path $imguiBackendsDir | Out-Null
}

foreach ($fileName in $files.Keys) {
    $filePath = Join-Path $imguiBackendsDir $fileName
    if (-not (Test-Path $filePath)) {
        Write-Host "Downloading $fileName ..."
        try {
            Invoke-WebRequest -Uri $files[$fileName] -OutFile $filePath -UseBasicParsing
            Write-Host "$fileName downloaded successfully."
        }
        catch {
            Write-Warning "Failed to download $fileName from $($files[$fileName])"
        }
    } else {
        Write-Host "$fileName already exists, skipping download."
    }
}



cmake $rootDir `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET="x64-windows-static" `
  -DCMAKE_BUILD_TYPE=Release


cmake --build . --config Release

Pop-Location

$buildDir = "C:\Users\abdur\Documents\LocalTether\build"
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

if (Test-Path $exe) {
    Write-Host "Launching LocalTether..."
    & $exe
} else {
    Write-Error "Executable not found: $exe"
}

Stop-Transcript
Write-Host "`n--- Done ---"
Read-Host "Press Enter to close"

