# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2018 Barry Peterson
#
# Build IOTRobot on Windows with Podman Desktop or Docker.
# Default: podman run ... idf.py set-target esp32 build
# Menuconfig: .\build.ps1 idf.py menuconfig

$ErrorActionPreference = "Stop"

$Root = (Resolve-Path -LiteralPath $PSScriptRoot).Path
$Version = (Get-Content -LiteralPath (Join-Path $Root "idf-version.txt") -Raw).Trim()
if ($env:IDF_IMAGE) {
    $Image = $env:IDF_IMAGE
} else {
    $Image = "docker.io/espressif/idf:$Version"
}

function Find-Runner {
    foreach ($name in @("podman", "docker")) {
        if (Get-Command $name -ErrorAction SilentlyContinue) {
            return $name
        }
    }
    throw "Install Podman Desktop (https://podman-desktop.io/) or Docker, then reopen the terminal."
}

$Runner = Find-Runner
$Defaults = "sdkconfig.defaults"
$Private = Join-Path $Root "sdkconfig.defaults.private"
if (Test-Path -LiteralPath $Private) {
    $Defaults = "sdkconfig.defaults;sdkconfig.defaults.private"
    $Sdk = Join-Path $Root "sdkconfig"
    if ((Test-Path -LiteralPath $Sdk) -and ((Get-Item -LiteralPath $Private).LastWriteTime -gt (Get-Item -LiteralPath $Sdk).LastWriteTime)) {
        Remove-Item -LiteralPath $Sdk -Force
        Write-Host "Removed sdkconfig so the newer sdkconfig.defaults.private is applied"
    }
}

$IdfArgs = $args
if ($IdfArgs.Count -eq 0) {
    $IdfArgs = @("idf.py", "set-target", "esp32", "build")
}

$RunArgs = @("run", "--rm", "-e", "SDKCONFIG_DEFAULTS=$Defaults", "-e", "CCACHE_DIR=/tmp/ccache", "-v", "${Root}:/project", "-w", "/project")
$joined = " $($IdfArgs -join ' ') "
if ($joined -match " menuconfig " -or $joined -match " monitor ") {
    $RunArgs += @("-it")
}
$RunArgs += $Image
$RunArgs += $IdfArgs

Write-Host "Using $Runner image $Image"
& $Runner @RunArgs
exit $LASTEXITCODE
