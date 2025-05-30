$ErrorActionPreference = 'Stop'


try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
} catch {
    Write-Warning "Could not set TLS 1.2. Depending on your PowerShell version and .NET Framework, this might be an issue for some downloads."
    Write-Warning "Error: $($_.Exception.Message)"
}
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
$assetsDir = Join-Path $rootDir "assets"      
$fontsDir = Join-Path $assetsDir "fonts"

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

vcpkg install asio:x64-windows-static sdl2:x64-windows-static openssl:x64-windows-static glad:x64-windows-static cereal:x64-windows-static

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
Push-Location $buildDir

$imguiBackendsDir = Join-Path $externalBaseDir "imgui_backends"

if (-not (Test-Path $externalBaseDir)) {
    Write-Host "Creating directory: $externalBaseDir"
    New-Item -ItemType Directory -Path $externalBaseDir | Out-Null
}
if (-not (Test-Path $assetsDir)) {
    Write-Host "Creating directory: $assetsDir"
    New-Item -ItemType Directory -Path $assetsDir | Out-Null
}
if (-not (Test-Path $fontsDir)) {
    Write-Host "Creating directory: $fontsDir"
    New-Item -ItemType Directory -Path $fontsDir | Out-Null
}

$creamSodeTtfUrl = "https://github.com/google/fonts/raw/refs/heads/main/ofl/ibmplexmono/IBMPlexMono-Regular.ttf"
$creamSodeTtfPath = Join-Path $fontsDir "IBMPlexMono-Regular.ttf"

$fontAwesomeSolidTtfUrl = "https://github.com/FortAwesome/Font-Awesome/raw/6.x/webfonts/fa-solid-900.ttf" # Using FA6 solid
$fontAwesomeSolidTtfPath = Join-Path $fontsDir "fa-solid-900.ttf"

# Download Open Sans Regular if it doesn't exist
if (-not (Test-Path $creamSodeTtfPath)) {
    Write-Host "Downloading IBMPlexMono-Regular.ttf..."
    try {
        Invoke-WebRequest -Uri $creamSodeTtfUrl -OutFile $creamSodeTtfPath
        Write-Host "IBMPlexMono-Regular.ttf downloaded successfully."
    } catch {
        Write-Error "Failed to download IBMPlexMono-Regular.ttf: $($_.Exception.Message)"
    }
} else {
    Write-Host "IBMPlexMono-Regular.ttf already exists."
}

# Download Font Awesome Solid if it doesn't exist
if (-not (Test-Path $fontAwesomeSolidTtfPath)) {
    Write-Host "Downloading Font Awesome Solid ttf..."
    try {
        Invoke-WebRequest -Uri $fontAwesomeSolidTtfUrl -OutFile $fontAwesomeSolidTtfPath
        Write-Host "Font Awesome Solid ttf downloaded successfully."
    } catch {
        Write-Error "Failed to download Font Awesome Solid ttf: $($_.Exception.Message)"
    }
} else {
    Write-Host "Font Awesome Solid ttf already exists."
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

