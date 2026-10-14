# Installs OUI Paint on Windows from the latest release: one .exe, with nothing to install beside it and no
# administrator needed. Or downloads the whole package, the program with its licences.
#
#   irm https://raw.githubusercontent.com/iganinja/oui_paint/master/install.ps1 | iex
#
# With no options it shows a menu of what there is and asks. An option chooses without asking; through irm it goes
# after the script made a script block:
#
#   -Package   the whole package, the program with LICENSE and THIRD_PARTY_NOTICES.txt, unpacked into the folder it
#              is run from rather than installed
#   -Install   the program, without asking
#
#   & ([scriptblock]::Create((irm https://raw.githubusercontent.com/iganinja/oui_paint/master/install.ps1))) -Install
#
# The program goes in %LOCALAPPDATA%\Programs\oui_paint, which is added to the user's PATH when it is not there, and
# gets a shortcut in the Start menu. Every download is checked against the release's SHA256SUMS before it is used.
# OUI_INSTALL_DIR says where the program goes instead; OUI_DOWNLOAD_URL where it comes from, the latest release unless
# set. Works in Windows PowerShell 5.1 and in PowerShell 7.
param(
    [switch]$Install,
    [switch]$Package
)

$ErrorActionPreference = "Stop"
# Invoke-WebRequest's progress bar slows a download down several times over in Windows PowerShell.
$ProgressPreference = "SilentlyContinue"
# Windows PowerShell 5.1 may not offer TLS 1.2 unless told, and GitHub takes nothing older.
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12

# A failure is thrown, to be shown at the end: exit would close the whole window of a script run through iex.
function Fail([string]$message) {
    throw $message
}

function Install-OuiPaint {
    if ($env:OS -ne "Windows_NT") {
        Fail "This installer is for Windows. On Linux and macOS, install.sh does the same: see the README."
    }
    # Windows on ARM runs the x86_64 program through its emulation.
    if ($env:PROCESSOR_ARCHITECTURE -ne "AMD64" -and $env:PROCESSOR_ARCHITECTURE -ne "ARM64") {
        Fail "The releases have OUI Paint for x86_64 only, and this is $env:PROCESSOR_ARCHITECTURE."
    }

    $source = $env:OUI_DOWNLOAD_URL
    if (-not $source) { $source = "https://github.com/iganinja/oui_paint/releases/latest/download" }
    $target = $env:OUI_INSTALL_DIR
    if (-not $target) { $target = Join-Path $env:LOCALAPPDATA "Programs\oui_paint" }

    $wholePackage = [bool]$Package
    if (-not ($Install -or $Package)) {
        Write-Host @"
OUI Paint: what would you like?

   1) OUI Paint, installed in $target, one .exe
   2) The whole package, with its licences, unpacked into this folder

   0) Nothing

"@
        # In a switch, break and continue act on the switch: whether the choice was one says when the asking ends.
        $chosen = $false
        while (-not $chosen) {
            $choice = Read-Host "Your choice [1]"
            if (-not $choice) { $choice = "1" }
            $chosen = $true
            switch ($choice.Trim()) {
                "1" { }
                "2" { $wholePackage = $true }
                "0" { Write-Host "Nothing installed."; return }
                default { Write-Host "That is no choice: a number from 0 to 2."; $chosen = $false }
            }
        }
    }

    $work = Join-Path ([IO.Path]::GetTempPath()) ("oui_paint-" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $work | Out-Null
    try {
        try {
            Invoke-WebRequest -UseBasicParsing -Uri "$source/SHA256SUMS" -OutFile (Join-Path $work "SHA256SUMS")
        } catch {
            Fail "The release's SHA256SUMS could not be downloaded from ${source}: $($_.Exception.Message)"
        }
        $sums = @{}
        foreach ($line in Get-Content (Join-Path $work "SHA256SUMS")) {
            $parts = $line -split "\s+", 2
            if ($parts.Count -eq 2) { $sums[$parts[1].TrimStart("*")] = $parts[0].ToLower() }
        }

        # A file of the release, downloaded and checked against its sum.
        function Get-ReleaseFile([string]$name) {
            Write-Host "Downloading $name..."
            $path = Join-Path $work $name
            try {
                Invoke-WebRequest -UseBasicParsing -Uri "$source/$name" -OutFile $path
            } catch {
                Fail "$name could not be downloaded from ${source}: $($_.Exception.Message)"
            }
            if (-not $sums.ContainsKey($name)) { Fail "The release's SHA256SUMS does not list ${name}: nothing done." }
            if ((Get-FileHash -Algorithm SHA256 -Path $path).Hash.ToLower() -ne $sums[$name]) {
                Fail "$name is not what the release says it is: nothing done."
            }
            return $path
        }

        if ($wholePackage) {
            $name = "oui_paint-windows-x86_64"
            $folder = Join-Path (Get-Location) $name
            if (Test-Path $folder) { Fail "$name is here already: nothing done." }
            $zip = Get-ReleaseFile "$name.zip"
            Expand-Archive -Path $zip -DestinationPath (Get-Location)
            Write-Host "Unpacked into ${folder}: the program, LICENSE and THIRD_PARTY_NOTICES.txt."
            return
        }

        $downloaded = Get-ReleaseFile "oui_paint-windows-x86_64.exe"
        New-Item -ItemType Directory -Force -Path $target | Out-Null
        $program = Join-Path $target "oui_paint.exe"
        Move-Item -Force -Path $downloaded -Destination $program
        Write-Host "Installed $program"

        # A shortcut in the user's Start menu, to start it as any other program.
        $menu = Join-Path ([Environment]::GetFolderPath("Programs")) "OUI Paint.lnk"
        $shortcut = (New-Object -ComObject WScript.Shell).CreateShortcut($menu)
        $shortcut.TargetPath = $program
        $shortcut.Save()
        Write-Host "OUI Paint is in the Start menu."

        # The folder in the user's PATH, for new terminals, and in this one's.
        $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
        $folders = @()
        if ($userPath) { $folders = $userPath -split ";" | Where-Object { $_ } }
        if ($folders -notcontains $target) {
            [Environment]::SetEnvironmentVariable("Path", (($folders + $target) -join ";"), "User")
            Write-Host "$target is now in your PATH: terminals opened from now on find oui_paint."
        }
        if (($env:Path -split ";") -notcontains $target) { $env:Path = "$env:Path;$target" }
    } finally {
        Remove-Item -Recurse -Force -Path $work -ErrorAction SilentlyContinue
    }
}

try {
    Install-OuiPaint
} catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    # Run as a file it ends with a code saying so; through iex, exit would close the window.
    if ($PSCommandPath) { exit 1 }
}
