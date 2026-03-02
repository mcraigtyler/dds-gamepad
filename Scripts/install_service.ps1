<#!
Install/uninstall/start/stop helper for the dds-gamepad Windows Service.

Usage examples (run PowerShell as Administrator):

    # From an extracted install tree (script lives in the install root):
    .\install_service.ps1 -Action Install -InstallDir "C:\dds-gamepad" -DomainId 0 -YokeId 1004 -ConfigFilePath "bin\config\driver.yaml" -StartType Automatic

  # Start / stop
    .\install_service.ps1 -Action Start
    .\install_service.ps1 -Action Stop

  # Uninstall
    .\install_service.ps1 -Action Uninstall

    # From this repository root (developer workflow):
    .\Scripts\install_service.ps1 -Action Install -InstallDir ".\install\dds-gamepad" -DomainId 0 -YokeId 1004 -ConfigFilePath "bin\config\driver.yaml" -StartType Automatic

Notes:
- The service binary is expected at: <InstallDir>\bin\dds-gamepad-service.exe
- The service reads a role config file from: <InstallDir>\bin\config\driver.yaml (default)
- Logs go to Windows Event Log (Application) under source: dds-gamepad-service
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Install','Uninstall','Start','Stop','Restart','Status')]
    [string]$Action,

    # Root of the extracted install tree that contains the 'bin' folder.
    [string]$InstallDir = (Get-Location).ProviderPath,

    [int]$DomainId = 0,

    [int]$YokeId = 1004,

    # Role config file path used by the service. Relative paths are resolved against InstallDir.
    [string]$ConfigFilePath = 'bin\config\driver.yaml',

    [ValidateSet('Automatic','Manual','Disabled')]
    [string]$StartType = 'Automatic'
)

$ErrorActionPreference = 'Stop'

$serviceName = 'dds-gamepad-service'
$displayName = 'dds-gamepad-service'

function Test-IsAdmin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Get-BinPath {
    $resolvedInstallDir = (Resolve-Path -Path $InstallDir).ProviderPath
    $binDir = Join-Path $resolvedInstallDir 'bin'
    $exePath = Join-Path $binDir 'dds-gamepad-service.exe'
    if (-not (Test-Path $exePath)) {
        throw "Service executable not found: $exePath"
    }

    $resolvedConfigFilePath = $ConfigFilePath
    if (-not [System.IO.Path]::IsPathRooted($resolvedConfigFilePath)) {
        $resolvedConfigFilePath = Join-Path $resolvedInstallDir $resolvedConfigFilePath
    }
    if (-not (Test-Path $resolvedConfigFilePath)) {
        throw "Config file not found: $resolvedConfigFilePath"
    }

    # sc.exe wants the full binPath string including args.
    $quotedExe = '"' + $exePath + '"'
    $quotedConfig = '"' + $resolvedConfigFilePath + '"'
    return "$quotedExe --domain-id $DomainId --yoke-id $YokeId --config-file $quotedConfig"
}

function New-EventLogSourceIfMissing {
    # Creating an event source requires admin and writes to HKLM.
    if (-not [System.Diagnostics.EventLog]::SourceExists($serviceName)) {
        New-EventLog -LogName Application -Source $serviceName | Out-Null
    }
}

function Test-ServiceExists {
    $svc = Get-Service -Name $serviceName -ErrorAction SilentlyContinue
    return $null -ne $svc
}

function Invoke-ServiceControl {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$ArgumentList
    )

    $serviceControlExe = Join-Path $env:SystemRoot 'System32\\sc.exe'
    $output = & $serviceControlExe @ArgumentList 2>&1 | Out-String
    if ($output) {
        $output | Write-Host
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Service control command failed (exit=$LASTEXITCODE): $($ArgumentList -join ' ')"
    }
}

if (-not (Test-IsAdmin)) {
    throw 'This script must be run from an elevated (Administrator) PowerShell.'
}

switch ($Action) {
    'Install' {
        New-EventLogSourceIfMissing

        if (Test-ServiceExists) {
            Write-Host "Service '$serviceName' already exists."
            return
        }

        $binPath = Get-BinPath

        $start = switch ($StartType) {
            'Automatic' { 'auto' }
            'Manual' { 'demand' }
            'Disabled' { 'disabled' }
        }

        # Note: spaces after '=' are required.
        Invoke-ServiceControl -ArgumentList @('create', $serviceName, 'binPath=', $binPath, 'DisplayName=', $displayName, 'start=', $start)
        Invoke-ServiceControl -ArgumentList @('description', $serviceName, 'dds-gamepad bridge from DDS to virtual Xbox 360 controller')

        # Recovery policy: restart 3 times over 1 minute.
        Invoke-ServiceControl -ArgumentList @('failure', $serviceName, 'reset=', '60', 'actions=', 'restart/20000/restart/20000/restart/20000')
        Invoke-ServiceControl -ArgumentList @('failureflag', $serviceName, '1')

        if (-not (Test-ServiceExists)) {
            throw "Service '$serviceName' was not created successfully."
        }

        Write-Host "Installed service '$serviceName' with binPath: $binPath"
    }

    'Uninstall' {
        if (-not (Test-ServiceExists)) {
            Write-Host "Service '$serviceName' does not exist."
            return
        }

        try {
            Stop-Service -Name $serviceName -Force -ErrorAction SilentlyContinue
        } catch {
            # ignore
        }

        Invoke-ServiceControl -ArgumentList @('delete', $serviceName)
        Write-Host "Uninstalled service '$serviceName'."
    }

    'Start' {
        Start-Service -Name $serviceName
        Write-Host "Started service '$serviceName'."
    }

    'Stop' {
        Stop-Service -Name $serviceName
        Write-Host "Stopped service '$serviceName'."
    }

    'Restart' {
        Restart-Service -Name $serviceName
        Write-Host "Restarted service '$serviceName'."
    }

    'Status' {
        Get-Service -Name $serviceName | Format-List | Out-String | Write-Host
    }
}
