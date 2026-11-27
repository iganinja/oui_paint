#!/usr/bin/env bash
# Writes install.cmd, the Windows installer that is opened with a double click: install.ps1 inside a batch file.
# Windows runs a script of PowerShell downloaded from the internet only when told to in a way of its own, and a batch
# file as any other program, so the batch file gives PowerShell the script that follows it, as irm | iex does. What is
# given to install.cmd goes to install.ps1, as -Install. Opened from Explorer it waits for a key before its window
# closes, so that what it said can be read.
#
#   tools/make_install_cmd.sh install.ps1 dist/release/install.cmd
set -euo pipefail

if [[ $# -ne 2 ]]; then
    printf 'usage: %s install.ps1 install.cmd\n' "$0" >&2
    exit 2
fi

{
    cat <<'EOF'
@echo off
rem Installs the program on Windows: open it with a double click. What follows the line #OUI-PS is install.ps1, which
rem PowerShell is given to run.
setlocal
set "OUI_INSTALLER=%~f0"
powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "$text = [IO.File]::ReadAllText($env:OUI_INSTALLER); & ([scriptblock]::Create(($text -split '(?m)^#OUI-PS\r?$', 2)[1])) %*"
set "OUI_STATUS=%ERRORLEVEL%"
echo(%cmdcmdline% | find /i "%~nx0" >nul && pause
exit /b %OUI_STATUS%
#OUI-PS
EOF
    cat "$1"
} | sed 's/\r$//; s/$/\r/' > "$2"
