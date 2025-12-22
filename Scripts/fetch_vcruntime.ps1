<#
.SYNOPSIS
    Download the Visual C++ Redistributable (x64) into the repository's external folder.

.DESCRIPTION
    Downloads the official Visual C++ 2015-2022 redistributable x64 installer (via Microsoft aka.ms link)
    and places it in the repository's `external` directory (creates the dir if needed).

.PARAMETER Url
    Optional download URL. Defaults to the Microsoft `vc_redist.x64.exe` redirect link.

.PARAMETER Force
    If set, overwrite any existing file.

.EXAMPLE
    .\fetch_vcruntime.ps1

.EXAMPLE
    .\fetch_vcruntime.ps1 -Force
#>

[CmdletBinding()]
param(
    [string]
    $Url = 'https://aka.ms/vs/17/release/vc_redist.x64.exe',

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
        # Fallback name for aka.ms redirect
        $fileName = 'vc_redist.x64.exe'
    }

    $outPath = Join-Path $externalDir $fileName

    if ((Test-Path $outPath) -and (-not $Force)) {
        Write-Host "File already exists at $outPath. Use -Force to re-download."
        exit 0
    }

    Write-Host "Downloading Visual C++ Redistributable (x64) to $outPath ..."
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
    Write-Error "Failed to download Visual C++ Redistributable: $_"
    exit 1
}
