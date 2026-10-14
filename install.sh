#!/bin/sh
# Installs OUI Paint on Linux, x86-64 or 64-bit ARM, and on macOS, from the latest release: one file, with nothing to
# install beside it and no root needed. Or downloads the whole package, the program with its licences.
#
#   curl -fsSL https://raw.githubusercontent.com/iganinja/oui_paint/master/install.sh | sh
#
# With no options it shows a menu of what there is and asks. Options choose without asking, after "sh -s --":
#
#   --package   the whole package, the program with LICENSE and THIRD_PARTY_NOTICES.txt, unpacked into the folder it
#               is run from rather than installed
#   --apt       the program from the project's APT repository, on Debian, Ubuntu and the like, so apt upgrade keeps it
#               up to date, with its place in the desktop's menu: it adds the repository and its key, with sudo, and
#               installs the package oui-paint in /usr/bin
#
#   curl -fsSL https://raw.githubusercontent.com/iganinja/oui_paint/master/install.sh | sh -s -- --apt
#
# Every download is checked against the release's SHA256SUMS before it is used. OUI_INSTALL_DIR says where the program
# goes, ~/.local/bin unless set; OUI_DOWNLOAD_URL where it comes from, the latest release unless set; OUI_APT_URL the
# APT repository, https://iganinja.github.io/oui_paint unless set.
set -eu

source=${OUI_DOWNLOAD_URL:-https://github.com/iganinja/oui_paint/releases/latest/download}
target=${OUI_INSTALL_DIR:-$HOME/.local/bin}
apt_source=${OUI_APT_URL:-https://iganinja.github.io/oui_paint}

fail() {
    echo "$*" >&2
    exit 1
}

case "$(uname -s)" in
    Linux) system=linux ;;
    Darwin) system=macos ;;
    *) fail "This installer is for Linux and macOS. On Windows, install.ps1 does the same: see the README." ;;
esac
case "$(uname -m)" in
    x86_64 | amd64) processor=x86_64 ;;
    # 64-bit ARM, as Linux and macOS each name it.
    aarch64 | arm64) if [ "$system" = macos ]; then processor=arm64; else processor=aarch64; fi ;;
    # A Raspberry Pi or another ARM board with a 32-bit system says armv7l or armv6l, even on a 64-bit processor.
    armv*) fail "The releases have OUI Paint for 64-bit ARM, and this system is 32-bit ($(uname -m)): a 64-bit one runs it." ;;
    *) fail "The releases have OUI Paint for x86_64 and 64-bit ARM (aarch64) only, not for $(uname -m)." ;;
esac

# What to get: the program, the whole package, or the program through apt.
package=0
apt=0
asked=0
for argument in "$@"; do
    case "$argument" in
        --package) package=1 ;;
        --apt) apt=1 ;;
        *) fail "Unknown option: $argument. It takes --package and --apt." ;;
    esac
    asked=1
done

if [ $asked -eq 0 ]; then
    # Through a pipe the script is what comes in: the answer comes from the terminal itself.
    # In a subshell, as a failed redirection of ":" would end the whole script.
    if ! (true < /dev/tty) 2>/dev/null; then
        fail "There is no terminal to ask in: say what to install, as \"sh -s -- --apt\", or run it from one."
    fi
    cat > /dev/tty <<'EOF'
OUI Paint: what would you like?

   1) OUI Paint, installed in ~/.local/bin, one file
   2) The whole package, with its licences, unpacked into this folder
   3) OUI Paint through apt, which keeps it up to date and puts it in the desktop's menu
      (Debian, Ubuntu and the like; asks for sudo)

   0) Nothing

EOF
    while :; do
        printf 'Your choice [1]: ' > /dev/tty
        read -r choice < /dev/tty || choice=0
        case "${choice:-1}" in
            1) ;;
            2) package=1 ;;
            3) apt=1 ;;
            0) echo "Nothing installed."; exit 0 ;;
            *) echo "That is no choice: a number from 0 to 3." > /dev/tty; continue ;;
        esac
        break
    done
fi

# On Linux it takes the C library and the display libraries from the system: glibc 2.35 or newer.
if [ "$system" = linux ]; then
    glibc=$(getconf GNU_LIBC_VERSION 2>/dev/null | awk '{print $2}') || glibc=""
    if [ -z "$glibc" ]; then
        fail "OUI Paint needs glibc, which this system has not."
    fi
    major=${glibc%%.*}
    minor=${glibc#*.}
    minor=${minor%%.*}
    if [ "$major" -lt 2 ] || { [ "$major" -eq 2 ] && [ "$minor" -lt 35 ]; }; then
        fail "OUI Paint needs glibc 2.35 or newer, and this system has $glibc."
    fi
fi

if command -v curl >/dev/null 2>&1; then
    fetch() { curl -fsSL "$1" -o "$2"; }
elif command -v wget >/dev/null 2>&1; then
    fetch() { wget -q "$1" -O "$2"; }
else
    fail "Neither curl nor wget is here to download with."
fi
if command -v sha256sum >/dev/null 2>&1; then
    sum() { sha256sum "$1" | awk '{print $1}'; }
elif command -v shasum >/dev/null 2>&1; then
    sum() { shasum -a 256 "$1" | awk '{print $1}'; }
else
    fail "Neither sha256sum nor shasum is here to check the download with."
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# The repository's key and its line among apt's sources, once; apt installs the package, and upgrades it from then on.
if [ $apt -eq 1 ]; then
    [ "$system" = linux ] || fail "apt is Linux's: on macOS, install the program itself, without --apt."
    command -v apt-get >/dev/null 2>&1 || fail "There is no apt here: install the program itself, without --apt."
    if [ "$(id -u)" -eq 0 ]; then
        as_root() { "$@"; }
    elif command -v sudo >/dev/null 2>&1; then
        as_root() { sudo "$@"; }
    else
        fail "Adding the APT repository takes root, and there is no sudo here."
    fi
    fetch "$apt_source/oui_paint.gpg" "$work/oui_paint.gpg" || fail "The repository's key could not be downloaded from $apt_source."
    echo "Adding the APT repository $apt_source, with sudo..."
    as_root mkdir -p /etc/apt/keyrings
    as_root install -m 0644 "$work/oui_paint.gpg" /etc/apt/keyrings/oui_paint.gpg
    echo "deb [signed-by=/etc/apt/keyrings/oui_paint.gpg] $apt_source stable main" > "$work/oui_paint.list"
    as_root install -m 0644 "$work/oui_paint.list" /etc/apt/sources.list.d/oui_paint.list
    # Through a pipe the script is what comes in: apt must not read the rest of it as answers.
    as_root apt-get update < /dev/null
    as_root apt-get install -y oui-paint < /dev/null
    echo "Installed oui-paint: apt upgrade keeps it up to date."
    exit 0
fi

fetch "$source/SHA256SUMS" "$work/SHA256SUMS" || fail "The release's SHA256SUMS could not be downloaded from $source."

# A file of the release, downloaded and checked against its sum.
download() {
    echo "Downloading $1..."
    fetch "$source/$1" "$work/$1" || fail "$1 could not be downloaded from $source."
    expected=$(awk -v name="$1" '$2 == name {print $1}' "$work/SHA256SUMS")
    [ -n "$expected" ] || fail "The release's SHA256SUMS does not list $1: nothing done."
    [ "$(sum "$work/$1")" = "$expected" ] || fail "$1 is not what the release says it is: nothing done."
}

name="oui_paint-$system-$processor"
if [ $package -eq 1 ]; then
    [ -e "$name" ] && fail "$name is here already: nothing done."
    download "$name.tar.gz"
    tar -xzf "$work/$name.tar.gz"
    echo "Unpacked into $(pwd)/$name: the program, LICENSE and THIRD_PARTY_NOTICES.txt."
    exit 0
fi

download "$name"
mkdir -p "$target"
chmod +x "$work/$name"
mv "$work/$name" "$target/oui_paint"
echo "Installed $target/oui_paint"
case ":$PATH:" in
    *":$target:"*) echo "Run it: oui_paint" ;;
    *) echo "$target is not in your PATH: add it, or run $target/oui_paint." ;;
esac
