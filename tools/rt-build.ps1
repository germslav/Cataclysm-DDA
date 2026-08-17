<#
    rt-build.ps1 — configure and build the Continuous Cataclysm fork.

    MSVC (Visual Studio Build Tools) + vcpkg (static triplet) + Ninja Multi-Config.
    Locates Visual Studio via vswhere, imports its developer environment, then
    drives the bundled CMake. No Developer Command Prompt required.

    Usage:
        pwsh tools/rt-build.ps1                     # configure + build RelWithDebInfo
        pwsh tools/rt-build.ps1 -ConfigureOnly      # configure only
        pwsh tools/rt-build.ps1 -Target cata_test   # build a single target
        pwsh tools/rt-build.ps1 -Config Debug
        pwsh tools/rt-build.ps1 -Fresh              # wipe the build dir first

    VCPKG_ROOT is honoured if already set; otherwise it defaults to D:/vcpkg.
#>
param(
    [string]$Config = "RelWithDebInfo",
    [string]$Target = "",
    [switch]$ConfigureOnly,
    [switch]$Fresh
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $RepoRoot "out\build\rt-msvc"

# ---------------------------------------------------------------- vcpkg ----
if (-not $env:VCPKG_ROOT) { $env:VCPKG_ROOT = "D:/vcpkg" }
$vcpkgToolchain = Join-Path $env:VCPKG_ROOT "scripts/buildsystems/vcpkg.cmake"
if (-not (Test-Path $vcpkgToolchain)) {
    throw "vcpkg not found at '$env:VCPKG_ROOT' (looked for $vcpkgToolchain). Set VCPKG_ROOT."
}
Write-Host "vcpkg:        $env:VCPKG_ROOT"

# Keep the binary cache next to vcpkg itself. The default location under
# %LOCALAPPDATA% failed to accept uploads on this machine, which means every
# fresh configure would rebuild all ports from source.
if (-not $env:VCPKG_DEFAULT_BINARY_CACHE) {
    $env:VCPKG_DEFAULT_BINARY_CACHE = Join-Path $env:VCPKG_ROOT "archives"
}
if (-not (Test-Path $env:VCPKG_DEFAULT_BINARY_CACHE)) {
    New-Item -ItemType Directory -Force -Path $env:VCPKG_DEFAULT_BINARY_CACHE | Out-Null
}
Write-Host "vcpkg cache:  $env:VCPKG_DEFAULT_BINARY_CACHE"

# --------------------------------------------------- Visual Studio setup ----
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found at $vswhere" }

$vsPath = & $vswhere -latest -products '*' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $vsPath) { throw "No Visual Studio installation with the C++ toolset was found." }
Write-Host "visual studio: $vsPath"

$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found at $vcvars" }

# Import the developer environment (cl.exe, link.exe, Windows SDK) into this session.
# `cmd /c "<vcvars> && set"` is the only reliable way to capture what the batch file exports.
& cmd.exe /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') {
        Set-Item -Path "Env:\$($matches[1])" -Value $matches[2] -ErrorAction SilentlyContinue
    }
}
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    throw "cl.exe is still not on PATH after importing the developer environment."
}

# CMake and Ninja ship inside the VS installation; vcvars64 does not add them.
$cmakeExe  = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$ctestExe  = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
$ninjaDir  = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
if (-not (Test-Path $cmakeExe)) { throw "cmake.exe not found at $cmakeExe" }
if (-not (Test-Path $ninjaDir)) { throw "Ninja directory not found at $ninjaDir" }
$env:PATH = "$ninjaDir;$(Split-Path $cmakeExe);$env:PATH"

Write-Host "cmake:        $cmakeExe"
Write-Host "build dir:    $BuildDir"
Write-Host ""

# ------------------------------------------------------------- configure ----
if ($Fresh -and (Test-Path $BuildDir)) {
    Write-Host "Removing $BuildDir"
    Remove-Item -Recurse -Force $BuildDir
}

$configureArgs = @(
    "-S", $RepoRoot,
    "-B", $BuildDir,
    "-G", "Ninja Multi-Config",
    "-DCMAKE_TOOLCHAIN_FILE=$RepoRoot/build-scripts/MSVC.cmake",
    "-DCMAKE_PROJECT_INCLUDE_BEFORE=$RepoRoot/build-scripts/windows-tiles-sounds-x64-msvc.cmake",
    "-DVCPKG_TARGET_TRIPLET=x64-windows-static",
    "-DDYNAMIC_LINKING=False",
    "-DTILES=True",
    "-DCURSES=False",
    "-DSOUND=False",
    "-DTESTS=True",
    "-DLOCALIZE=True",
    "-DCMAKE_INSTALL_MESSAGE=NEVER"
)

Write-Host "--- configure ---"
& $cmakeExe @configureArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE" }

if ($ConfigureOnly) {
    Write-Host ""
    Write-Host "Configure complete. ctest lives at: $ctestExe"
    exit 0
}

# ----------------------------------------------------------------- build ----
$buildArgs = @("--build", $BuildDir, "--config", $Config)
if ($Target) { $buildArgs += @("--target", $Target) }

Write-Host ""
Write-Host "--- build ($Config) ---"
& $cmakeExe @buildArgs
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }

Write-Host ""
Write-Host "Build complete: $BuildDir\$Config"
Write-Host "ctest:          $ctestExe"
