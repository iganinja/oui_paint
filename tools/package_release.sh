#!/usr/bin/env bash
# Makes a release's files from what tools/build_portable.sh left in dist/<system>-<processor>/, in dist/release/:
#
#   oui_paint-<system>-<processor>.tar.gz, or .zip for Windows: the program, with LICENSE and THIRD_PARTY_NOTICES.txt,
#       the licences of what it carries;
#   oui_paint-<system>-<processor>, with .exe for Windows: the program alone, as oui_paint-linux-x86_64;
#   oui-paint-linux-<processor>.deb: the Linux program as a Debian package, installed in /usr/bin with sudo dpkg -i,
#       and upgraded by installing a newer one the same way, with its place in the desktop's menu and its icon, from
#       tools/linux_desktop;
#   THIRD_PARTY_NOTICES.txt: every licence any of them carries, for whoever downloads the program alone;
#   SHA256SUMS: the sum of each of those files.
#
# The names carry no version, so a release's files are always at the same address: GitHub's
# releases/latest/download/<name> is the latest one's, which install.sh and install.ps1 download from.
#
# The package's version is OUI_PAINT_VERSION, as GitHub gives it from the tag, v0.1.0; unless set, git describe's, as
# 0.1.0+3.ged0fda3 three commits after it, which Debian orders after 0.1.0.
#
# usage: tools/package_release.sh
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
oui="$root/third_party/olagarro"
release="$root/dist/release"
rm -rf "$release"
mkdir -p "$release"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# One licence, under a heading saying whose it is and why it is here.
notice() {
    printf '%s\n%s\n\n' "$1" "$(printf '%s' "$1" | sed 's/./=/g')"
    cat "$2"
    printf '\n\n'
}

# The licences of what the program of a system carries, as Help > About shows them.
notices() {
    local system="$1"
    printf 'OUI Paint carries code and pictures from these projects, under their licences.\n\n'
    notice "OUI, of olagarro, the interface" "$oui/LICENSE"
    notice "QuickJS-ng, the JavaScript engine plugins run in" "$oui/third_party/quickjs-ng/LICENSE"
    notice "NanoSVG, which draws SVG pictures" "$oui/third_party/nanosvg/LICENSE.txt"
    notice "stb_image, stb_image_write and stb_truetype, which read and write images and read fonts" \
        "$root/third_party/stb/LICENSE"
    notice "LodePNG, which writes PNG" "$root/third_party/lodepng/LICENSE"
    notice "miniz, which zips OpenRaster files" "$root/third_party/miniz/LICENSE"
    notice "SDL3, the desktop window" "$oui/licenses/SDL3.txt"
    notice "Spleen, the cell font" "$oui/oui/fonts/spleen_license.txt"
    notice "Jersey 10, the interface font" "$oui/oui/fonts/jersey10_ofl.txt"
    notice "The Godot editor's icons" "$root/icons/godot/LICENSE.txt"
    if [[ "$system" == windows || "$system" == all ]]; then
        notice "The mingw-w64 runtime (the Windows program)" "$oui/licenses/mingw-w64-runtime.txt"
        notice "winpthreads (the Windows program)" "$oui/licenses/winpthreads.txt"
    fi
}

found=0
for built in "$root"/dist/linux-* "$root"/dist/macos-* "$root"/dist/windows-*; do
    [[ -d "$built" ]] || continue
    found=1
    name="$(basename "$built")"
    system="${name%%-*}"
    package="$work/oui_paint-$name"
    mkdir -p "$package"
    cp "$built"/* "$package/"
    # A program that went through GitHub's artifacts comes back without the bit that lets it run.
    chmod +x "$package"/oui_paint*
    cp "$root/LICENSE" "$package/LICENSE"
    notices "$system" > "$package/THIRD_PARTY_NOTICES.txt"
    if [[ "$system" == windows ]]; then
        (cd "$work" && zip -q -r -X "$release/oui_paint-$name.zip" "oui_paint-$name")
        cp "$built/oui_paint.exe" "$release/oui_paint-$name.exe"
    else
        tar -C "$work" --owner=0 --group=0 --numeric-owner -czf "$release/oui_paint-$name.tar.gz" "oui_paint-$name"
        cp "$built/oui_paint" "$release/oui_paint-$name"
        chmod +x "$release/oui_paint-$name"
    fi
done
if [[ $found -eq 0 ]]; then
    echo "Nothing built in dist/: run tools/build_portable.sh first." >&2
    exit 1
fi

# The Linux program as a Debian package: in /usr/bin, in the desktop's menu, the licences as Debian keeps them.
version="${OUI_PAINT_VERSION:-$(git -C "$root" describe --tags 2>/dev/null || echo 0.0.0)}"
version="${version#v}"
version="$(printf '%s' "$version" | sed -E 's/-([0-9]+)-(g[0-9a-f]+)$/+\1.\2/')"
package=oui-paint
for built in "$root"/dist/linux-*; do
    [[ -d "$built" ]] || continue
    name="$(basename "$built")"
    processor="${name#linux-}"
    case "$processor" in
        x86_64) architecture=amd64 ;;
        aarch64) architecture=arm64 ;;
        *) echo "No Debian architecture for $processor." >&2; exit 1 ;;
    esac
    tree="$work/deb-$name"
    mkdir -p "$tree/DEBIAN" "$tree/usr/bin" "$tree/usr/share/doc/$package" "$tree/usr/share/applications" \
        "$tree/usr/share/icons/hicolor/128x128/apps"
    install -m 0755 "$built/oui_paint" "$tree/usr/bin/oui_paint"
    install -m 0644 "$root/tools/linux_desktop/oui_paint.desktop" "$tree/usr/share/applications/oui_paint.desktop"
    install -m 0644 "$root/tools/linux_desktop/oui_paint.png" "$tree/usr/share/icons/hicolor/128x128/apps/oui_paint.png"
    { cat "$root/LICENSE"; printf '\n\n'; notices linux; } > "$tree/usr/share/doc/$package/copyright"
    # The changelog Debian asks every package for: which version, from where, and when.
    {
        printf '%s (%s) unstable; urgency=medium\n\n' "$package" "$version"
        printf '  * Built from https://github.com/iganinja/oui_paint, version %s.\n\n' "$version"
        printf ' -- iganinja <iganinja@users.noreply.github.com>  %s\n' "$(LC_ALL=C date -u -R)"
    } | gzip -9 -n > "$tree/usr/share/doc/$package/changelog.gz"
    # What it takes on disk once installed, in KiB, which apt tells before installing it.
    size="$(du -sk --apparent-size "$tree/usr" | cut -f1)"
    {
        printf 'Package: %s\nVersion: %s\nArchitecture: %s\n' "$package" "$version" "$architecture"
        printf 'Maintainer: iganinja <iganinja@users.noreply.github.com>\n'
        # Built on Ubuntu 22.04: its C library or a newer one; the display libraries SDL3 opens as it starts.
        printf 'Depends: libc6 (>= 2.35)\n'
        printf 'Installed-Size: %s\n' "$size"
        printf 'Section: graphics\nPriority: optional\nHomepage: https://github.com/iganinja/oui_paint\n'
        printf 'Description: Image editor in the spirit of Paint.NET\n'
        printf ' OUI Paint edits images with layers, selections that move, turn and scale,\n'
        printf ' 8 and 16 bits a channel, indexed images with palettes, and effects written\n'
        printf ' as plugins. It is made with OUI, of olagarro.\n'
    } > "$tree/DEBIAN/control"
    # Writable by root alone, whatever the umask of the machine that builds it.
    chmod -R u=rwX,go=rX "$tree"
    dpkg-deb --build --root-owner-group -Zxz "$tree" "$release/$package-$name.deb" >/dev/null
done

notices all > "$release/THIRD_PARTY_NOTICES.txt"
(cd "$release" && sha256sum -- * > SHA256SUMS)
ls -l "$release"
