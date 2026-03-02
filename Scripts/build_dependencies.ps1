<#
PowerShell helper to build project dependencies such as CycloneDDS and CycloneDDS-CXX on Windows from vendored sub-repositories.

Usage:
  From the workspace root run (Developer PowerShell recommended):

    Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope Process
    .\Scripts\build_dependencies.ps1 [-Force] [-Generator "Visual Studio 17 2022"]

Parameters:
  -Force      : Rebuild even if install/<repo> already exists (removes the install folder first).
  -Generator  : Optional CMake generator to pass to configure, e.g. "Ninja" or "Visual Studio 17 2022".

Prerequisites:
  - Git on PATH
  - CMake on PATH (https://cmake.org/download/)
  - A C/C++ build toolchain (Visual Studio or Ninja + toolchain)

This script expects CycloneDDS and CycloneDDS-CXX to be sub-repos under <workspace>/external
(e.g., added as git submodules), ensures they are initialized/updated, and installs into <workspace>/install.
#>

[CmdletBinding()]
param(
    [switch]$Force,
    [string]$Generator
)

$ErrorActionPreference = 'Stop'

function Info([string]$Message) { Write-Verbose $Message }
function Warn([string]$Message) { Write-Warning $Message }
function Err([string]$Message)  { Write-Error $Message }

# Determine workspace folder, fallback to current directory
$workspace = $env:WORKSPACE_FOLDER
if (-not $workspace -or $workspace -eq '') { $workspace = (Get-Location).ProviderPath }
Info "Workspace: $workspace"

# Check required tools
foreach ($t in @('git','cmake')) {
    if (-not (Get-Command $t -ErrorAction SilentlyContinue)) {
        Err -Message "Required tool '$t' not found on PATH. Please install it and re-run."
        exit 2
    }
}

$externalDir = Join-Path $workspace 'external'
$installDir  = Join-Path $workspace 'install'

# Mark script-level params as used (they're actually consumed by helper functions). This avoids PSScriptAnalyzer warnings about unused parameters.
if ($false) { $null = $Force; $null = $Generator }

if (-not (Test-Path $externalDir)) { New-Item -ItemType Directory -Path $externalDir | Out-Null }
if (-not (Test-Path $installDir))  { New-Item -ItemType Directory -Path $installDir  | Out-Null }

function Ensure-SubRepo {
    param(
        [Parameter(Mandatory=$true)][string]$Name
    )
    $path = Join-Path $externalDir $Name
    $relPath = "external/$Name"

    if (-not (Test-Path $path)) {
        Err -Message "Expected sub-repo '$Name' at '$path' not found. Add it as a submodule (e.g., git submodule add) and re-run."
        throw "Missing sub-repo: $Name"
    }

    $isGitWorkspace = Test-Path (Join-Path $workspace '.git')

    if ($isGitWorkspace) {
        Info "Ensuring submodule '$relPath' is initialized and up to date"
        # Initialize/update to the commit recorded in the superproject
        & git -C $workspace submodule update --init --recursive -- "$relPath" | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Warn -Message "git submodule update failed for $relPath (exit $LASTEXITCODE). Attempting 'git pull' fallback in $path."
            if (Test-Path (Join-Path $path '.git')) {
                & git -C $path pull --ff-only | Out-Null
            }
        }
    }
    else {
        # Not a git workspace (e.g., zipped source). Try a simple pull if it's an independent repo clone.
        if (Test-Path (Join-Path $path '.git')) {
            Info "Workspace is not a git repo; pulling latest in '$path'"
            & git -C $path pull --ff-only | Out-Null
        }
        else {
            Warn -Message "Workspace is not a git repo and '$path' is not a git checkout. Proceeding without update."
        }
    }

    return $path
}

function InstallFromSource {
    param(
        [string]$SourcePath,
        [string]$InstallPath,
        [string[]]$ExtraCMake = @()
    )

    if ((Test-Path $InstallPath) -and (-not $Force)) {
        Warn -Message "Install path $InstallPath exists - skipping (use -Force to rebuild)."
        return
    }

    if ((Test-Path $InstallPath) -and $Force) {
        Info "-Force set: removing existing install path $InstallPath"
        Remove-Item -Recurse -Force $InstallPath
    }

    $buildDir = Join-Path $SourcePath 'build'
    if (Test-Path $buildDir) { Remove-Item -Recurse -Force $buildDir }
    New-Item -ItemType Directory -Path $buildDir | Out-Null
    Push-Location $buildDir
    try {
        $cmakeArgs = @()
        if ($Generator) {
            $cmakeArgs += '-G'; $cmakeArgs += $Generator
            if ($Generator -like 'Visual Studio*') {
                # Ensure VS generator has architecture/toolset set
                $cmakeArgs += '-A'; $cmakeArgs += 'x64'
                $cmakeArgs += '-T'; $cmakeArgs += 'host=x64'
            }
        }
        $cmakeArgs += '..'
        $cmakeArgs += "-DCMAKE_INSTALL_PREFIX=$InstallPath"
        if ($ExtraCMake) { $cmakeArgs += $ExtraCMake }

        Info "Configuring: cmake $($cmakeArgs -join ' ')"
        & cmake @cmakeArgs
        # Fail fast if configure failed or cache is missing
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path (Join-Path $buildDir 'CMakeCache.txt'))) {
            Err "CMake configure failed (exit $LASTEXITCODE) in $buildDir."
            return
        }

        Info "Building and installing (Release)"
        & cmake --build . --config Release --target install -- /m
        if ($LASTEXITCODE -ne 0) {
            Err "CMake build/install failed (exit $LASTEXITCODE) in $buildDir."
            return
        }
    }
    finally {
        Pop-Location
    }
}

# Repositories and installs (vendored under external/)
$cycloneRepo = 'cyclonedds'
$cyclonePath = Ensure-SubRepo -Name $cycloneRepo
$cycloneInstall = Join-Path $installDir $cycloneRepo

# Disable building tests, examples, and ddsperf tool to avoid idlc-driven custom step issues
$cycloneExtraCMake = @('-DBUILD_TESTING=OFF','-DBUILD_EXAMPLES=OFF','-DBUILD_DDSPERF=OFF')
InstallFromSource -SourcePath $cyclonePath -InstallPath $cycloneInstall -ExtraCMake $cycloneExtraCMake

$cycloneCxxRepo = 'cyclonedds-cxx'
$cycloneCxxPath = Ensure-SubRepo -Name $cycloneCxxRepo
$cycloneCxxInstall = Join-Path $installDir $cycloneCxxRepo

# Try to locate the CycloneDDS CMake config dir in the install tree and pass it on
$possible1 = Join-Path $cycloneInstall 'lib\cmake\CycloneDDS'
$possible2 = Join-Path $cycloneInstall 'share\cyclonedds\cmake'
$cmakeOpts = @()
if (Test-Path $possible1) { $cmakeOpts += "-DCycloneDDS_DIR=$possible1"; Info "Using CycloneDDS_DIR=$possible1" }
elseif (Test-Path $possible2) { $cmakeOpts += "-DCycloneDDS_DIR=$possible2"; Info "Using CycloneDDS_DIR=$possible2" }
else { Warn "CycloneDDS CMake config not found in install tree; cyclonedds-cxx configure may need CycloneDDS_DIR set manually." }

InstallFromSource -SourcePath $cycloneCxxPath -InstallPath $cycloneCxxInstall -ExtraCMake $cmakeOpts

Info "Done. Installed into:"
Write-Output "  $cycloneInstall"
Write-Output "  $cycloneCxxInstall"

Info "Example CMake options to consume the install from other builds:"
Write-Output "  -DCMAKE_PREFIX_PATH=\"$installDir\""
if ($cmakeOpts) { Write-Output "  $($cmakeOpts -join ' ')" }

# End of script.
