<#
PowerShell helper to clone and build CycloneDDS and CycloneDDS-CXX on Windows.

Usage:
  From the workspace root run (Developer PowerShell recommended):

    Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope Process
    .\Scripts\install_cyclonedds.ps1 [-Force] [-Generator "Ninja"]

Parameters:
  -Force      : Rebuild even if install/<repo> already exists (removes the install folder first).
  -Generator  : Optional CMake generator to pass to configure, e.g. "Ninja" or "Visual Studio 17 2022".

Prerequisites:
  - Git on PATH
  - CMake on PATH
  - A C/C++ build toolchain (Visual Studio or Ninja + toolchain)

This script clones/updates repositories into <workspace>/external and installs into <workspace>/install.
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

function Get-Repository {
    param($Name, $Url)
    $path = Join-Path $externalDir $Name
    if (-not (Test-Path $path)) {
        Info "Cloning $Name -> $path"
        & git clone $Url $path | Out-Null
    }
    else {
        Info "Updating $Name at $path"
        Push-Location $path
        try { & git pull | Out-Null } finally { Pop-Location }
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
        if ($Generator) { $cmakeArgs += '-G'; $cmakeArgs += $Generator }
        $cmakeArgs += '..'
        $cmakeArgs += "-DCMAKE_INSTALL_PREFIX=$InstallPath"
        if ($ExtraCMake) { $cmakeArgs += $ExtraCMake }

        Info "Configuring: cmake $($cmakeArgs -join ' ')"
        & cmake @cmakeArgs

        Info "Building and installing (Release)"
        & cmake --build . --config Release --target install -- /m
    }
    finally {
        Pop-Location
    }
}

# Repositories and installs
$cycloneRepo = 'cyclonedds'
$cycloneUrl  = 'https://github.com/eclipse-cyclonedds/cyclonedds.git'
$cyclonePath = Get-Repository -Name $cycloneRepo -Url $cycloneUrl
$cycloneInstall = Join-Path $installDir $cycloneRepo

InstallFromSource -SourcePath $cyclonePath -InstallPath $cycloneInstall

$cycloneCxxRepo = 'cyclonedds-cxx'
$cycloneCxxUrl  = 'https://github.com/eclipse-cyclonedds/cyclonedds-cxx.git'
$cycloneCxxPath = Get-Repository -Name $cycloneCxxRepo -Url $cycloneCxxUrl
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

# Duplicate trailing content removed - consolidated above.
# End of script.
