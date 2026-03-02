<#
.SYNOPSIS
    Download the ViGEmBus installer into the repository's external folder.

.DESCRIPTION
    Downloads https://github.com/nefarius/ViGEmBus/releases/download/v1.22.0/ViGEmBus_1.22.0_x64_x86_arm64.exe
    and places it in the repository's `external` directory (creates the dir if needed).

.PARAMETER Url
    Optional download URL. Defaults to the v1.22.0 release binary.

.PARAMETER Force
    If set, overwrite any existing file.

.EXAMPLE
    .\fetch_vigembus.ps1

.EXAMPLE
    .\fetch_vigembus.ps1 -Force
#>

[CmdletBinding()]
param(
    [string]
    $Url = 'https://github.com/nefarius/ViGEmBus/releases/download/v1.22.0/ViGEmBus_1.22.0_x64_x86_arm64.exe',

    [switch]
    $Force
)

try {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
    $repoRoot = Resolve-Path (Join-Path $scriptDir '..')
    $externalDir = Join-Path $repoRoot 'external'
    if (-not (Test-Path $externalDir)) {
        Write-Host "Creating external directory: $externalDir"
        New-Item -Path $externalDir -ItemType Directory -Force | Out-Null
    }

    $fileName = [System.IO.Path]::GetFileName([uri]$Url).Trim()
    if ([string]::IsNullOrEmpty($fileName)) {
        throw "Unable to determine filename from URL: $Url"
    }

    $outPath = Join-Path $externalDir $fileName

    if ((Test-Path $outPath) -and (-not $Force)) {
        Write-Host "File already exists at $outPath. Use -Force to re-download."
        exit 0
    }

    Write-Host "Downloading ViGEmBus installer to $outPath ..."
    # Use Invoke-WebRequest which works on modern PowerShell; fall back to Start-BitsTransfer if available
    if (Get-Command Invoke-WebRequest -ErrorAction SilentlyContinue) {
        Invoke-WebRequest -Uri $Url -OutFile $outPath -UseBasicParsing -Verbose
    }
    else {
        Start-BitsTransfer -Source $Url -Destination $outPath
    }

    if (Test-Path $outPath) {
        Write-Host "Download complete: $outPath"
        exit 0
    }
    else {
        throw "Download finished but file not found at $outPath"
    }
}
catch {
    Write-Error "Failed to download ViGEmBus installer: $_"
    exit 1
}
