# POSEIDON C5 node flash + serial monitor in one shot.
# Usage:
#   scripts\c5_flash_and_monitor.ps1 -Port COM3
#
# Activates the ESP-IDF PowerShell environment, builds/flashes the c5_node
# app, and opens the idf.py monitor in the same step.
#
# Ctrl+] inside the monitor to exit cleanly.

param(
    [Parameter(Mandatory=$true)]
    [string]$Port
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

# Activate ESP-IDF environment
. C:\Espressif\tools\Microsoft.v5.5.5.PowerShell_profile.ps1

Write-Host "=== C5 BUILD + FLASH ($Port) ==="
idf.py -C c5_node -p $Port flash monitor
