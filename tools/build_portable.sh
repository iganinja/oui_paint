#!/usr/bin/env bash
# Builds the program a release carries, oui_paint, for another machine: optimised for speed, as an image editor's
# effects are its work, needing nothing installed beside it, and stripped. Linux's and Windows' are built inside a
# container, with podman or docker, the repository mounted read-only, so this machine and GitHub's build them the same
# way.
#
#   tools/build_portable.sh             Linux: it carries SDL3 and the C++ runtime, and takes the C library and the
#                                       display libraries from the system, so it is built on Ubuntu 22.04 to run on
#                                       that and anything newer.
#   tools/build_portable.sh --windows   Windows: oui_paint.exe, cross-built with llvm-mingw and linked statically.
#
# On a Mac it builds macOS's there, as Apple's tools run in no container: for macOS 11 and newer, the first Apple's own
# processors run, linked against the system's libraries, which every Mac has, unused code dropped (-dead_strip). The
# machine's processor says which: arm64 on Apple's, x86_64 on Intel's.
#
# The Linux one is for the processor the container runs on: x86_64 here, aarch64 on an ARM machine; the Windows one is
# for x86_64. OUI_PAINT_VERSION, as a release's tag gives it, v0.1.0, is the version it shows; unless set, the
# project's. JOBS says how many compilers run at a time in a container, every processor's unless set. Output:
# dist/<system>-<processor>/, as dist/linux-x86_64/, with the program. tools/package_release.sh then makes a release's
# files of them.
set -euo pipefail

windows=0
for argument in "$@"; do
    case "$argument" in
        --windows) windows=1 ;;
        *) echo "Unknown option: $argument" >&2; exit 2 ;;
    esac
done

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="${OUI_PAINT_VERSION:-}"
version="${version#v}"

# What every build of a release shares: -O2, as RelWithDebInfo has it, rather than the -O3 of Release, which makes GCC
# see overlaps that are not there; its debugging information goes with strip. No tests, and the version.
common="-G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOUI_PAINT_BUILD_TESTS=OFF -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON"
[[ -n "$version" ]] && common="$common -DOUI_PAINT_VERSION=$version"

if [[ "$(uname -s)" == Darwin ]]; then
    if [[ $windows -eq 1 ]]; then
        echo "Windows' program is built on Linux" >&2
        exit 2
    fi
    output="$root/dist/macos-$(uname -m)"
    rm -rf "$output"
    mkdir -p "$output"
    work="$(mktemp -d)"
    trap 'rm -rf "$work"' EXIT
    cmake -S "$root" -B "$work/build" $common -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
        -DCMAKE_EXE_LINKER_FLAGS=-Wl,-dead_strip -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$work/bin" >/dev/null
    cmake --build "$work/build" --target oui_paint >/dev/null
    strip -o "$output/oui_paint" "$work/bin/oui_paint"
    # Signed again, ad hoc, with no certificate: a Mac with Apple's processor runs nothing unsigned, and strip changed
    # what the linker signed.
    codesign --force --sign - "$output/oui_paint"
    echo "$output/oui_paint: $(stat -f%z "$output/oui_paint") bytes"
    exit 0
fi

engine="$(command -v podman || command -v docker || true)"
if [[ -z "$engine" ]]; then
    echo "podman or docker is required" >&2
    exit 1
fi

if [[ $windows -eq 1 ]]; then
    output="$root/dist/windows-x86_64"
    program=oui_paint.exe
else
    output="$root/dist/linux-$(uname -m)"
    program=oui_paint
fi
rm -rf "$output"
mkdir -p "$output"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# JOBS compilers at a time, every processor's unless set.
jobs="${JOBS:-$(nproc)}"

# With docker the container writes as root: each opens what it wrote to everyone as it ends, for the trap to clean.
if [[ $windows -eq 1 ]]; then
    # llvm-mingw, a version that does not change under the release, checked against its sum, as tools/build_windows.sh
    # takes it.
    "$engine" run --rm -v "$root:/src:ro,Z" -v "$work:/work:Z" -e "COMMON=$common" \
        -e "CMAKE_BUILD_PARALLEL_LEVEL=$jobs" docker.io/library/ubuntu:22.04 bash -euc '
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -qq
        apt-get install -qq -y --no-install-recommends cmake ninja-build curl ca-certificates xz-utils >/dev/null
        version=20260922
        name=llvm-mingw-$version-ucrt-ubuntu-22.04-x86_64
        curl -fsSL -o /work/llvm-mingw.tar.xz \
            https://github.com/mstorsjo/llvm-mingw/releases/download/$version/$name.tar.xz
        echo "bb7bb7654b33d5aa8712acb837c963b2e0c56352560c76105270a3268c665c21  /work/llvm-mingw.tar.xz" | sha256sum -c -
        tar -xJf /work/llvm-mingw.tar.xz -C /work
        toolchain=/work/$name/bin
        cat > /work/windows.cmake <<EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER $toolchain/x86_64-w64-mingw32-clang)
set(CMAKE_CXX_COMPILER $toolchain/x86_64-w64-mingw32-clang++)
set(CMAKE_RC_COMPILER $toolchain/x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /work/$name/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF
        cmake -S /src -B /work/build $COMMON -DCMAKE_TOOLCHAIN_FILE=/work/windows.cmake \
            -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=/work/bin >/dev/null
        cmake --build /work/build --target oui_paint >/dev/null
        $toolchain/x86_64-w64-mingw32-strip -o /work/oui_paint.exe /work/bin/oui_paint.exe
        chmod -R a+rwX /work
    '
else
    "$engine" run --rm -v "$root:/src:ro,Z" -v "$work:/work:Z" -e "COMMON=$common" \
        -e "CMAKE_BUILD_PARALLEL_LEVEL=$jobs" docker.io/library/ubuntu:22.04 bash -euc '
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -qq
        apt-get install -qq -y --no-install-recommends g++ cmake ninja-build pkg-config ca-certificates \
            libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev libxtst-dev \
            libxkbcommon-dev libwayland-dev libwayland-bin wayland-protocols libegl-dev libgl-dev libdrm-dev \
            libgbm-dev >/dev/null
        # Ubuntu 22.04 has GCC 11, which warns of uninitialised values where there are none.
        cmake -S /src -B /work/build $COMMON -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=/work/bin \
            -DCMAKE_CXX_FLAGS=-Wno-error=maybe-uninitialized >/dev/null
        cmake --build /work/build --target oui_paint >/dev/null
        strip -o /work/oui_paint /work/bin/oui_paint
        chmod -R a+rwX /work
    '
fi

cp "$work/$program" "$output/$program"
echo "$output/$program: $(stat -c%s "$output/$program") bytes"
