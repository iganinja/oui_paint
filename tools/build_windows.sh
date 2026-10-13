#!/usr/bin/env bash
# Builds oui_paint.exe for Windows on Linux, cross-compiled with llvm-mingw: optimised for size, every runtime linked
# in, stripped, so it needs nothing installed beside it. The toolchain, a version that does not change, checked
# against its sum, is kept in ~/.cache/oui_paint between builds. Output: dist/oui_paint.exe.
#
#   tools/build_windows.sh            the program
#   tools/build_windows.sh --tests    its tests as well, dist/oui_paint_tests.exe with the DLLs it needs beside it,
#                                     to run on Windows or with wine
set -euo pipefail

tests=OFF
for argument in "$@"; do
    case "$argument" in
        --tests) tests=ON ;;
        *) echo "Unknown option: $argument" >&2; exit 2 ;;
    esac
done

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cache="${XDG_CACHE_HOME:-$HOME/.cache}/oui_paint"
version=20260922
name=llvm-mingw-$version-ucrt-ubuntu-22.04-x86_64
toolchain="$cache/$name"
mkdir -p "$cache"
if [[ ! -x "$toolchain/bin/x86_64-w64-mingw32-clang++" ]]; then
    curl -fsSL -o "$cache/llvm-mingw.tar.xz" \
        "https://github.com/mstorsjo/llvm-mingw/releases/download/$version/$name.tar.xz"
    echo "bb7bb7654b33d5aa8712acb837c963b2e0c56352560c76105270a3268c665c21  $cache/llvm-mingw.tar.xz" | sha256sum -c -
    tar -xJf "$cache/llvm-mingw.tar.xz" -C "$cache"
    rm "$cache/llvm-mingw.tar.xz"
fi

roots="$toolchain/x86_64-w64-mingw32"
if [[ $tests == ON && ! -f "$cache/gtest/lib/libgtest.a" ]]; then
    # GoogleTest for Windows, from the system's sources.
    cat > "$cache/gtest.cmake" <<TOOLCHAIN
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER $toolchain/bin/x86_64-w64-mingw32-clang)
set(CMAKE_CXX_COMPILER $toolchain/bin/x86_64-w64-mingw32-clang++)
TOOLCHAIN
    cmake -S /usr/src/googletest -B "$cache/gtest-build" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$cache/gtest.cmake" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$cache/gtest" >/dev/null
    cmake --build "$cache/gtest-build" >/dev/null
    cmake --install "$cache/gtest-build" >/dev/null
fi
[[ $tests == ON ]] && roots="$roots $cache/gtest"

cat > "$cache/windows.cmake" <<TOOLCHAIN
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER $toolchain/bin/x86_64-w64-mingw32-clang)
set(CMAKE_CXX_COMPILER $toolchain/bin/x86_64-w64-mingw32-clang++)
set(CMAKE_RC_COMPILER $toolchain/bin/x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH $roots)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
TOOLCHAIN

build="$cache/build-windows"
cmake -S "$root" -B "$build" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$cache/windows.cmake" -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DOUI_PAINT_BUILD_TESTS=$tests \
    -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST >/dev/null
targets=oui_paint
[[ $tests == ON ]] && targets="$targets oui_paint_tests"
cmake --build "$build" --target $targets ${JOBS:+-j "$JOBS"}

mkdir -p "$root/dist"
"$toolchain/bin/x86_64-w64-mingw32-strip" -o "$root/dist/oui_paint.exe" "$root/bin/oui_paint.exe"
if [[ $tests == ON ]]; then
    cp "$build/oui_paint_tests.exe" "$root/dist/"
    cp "$toolchain/x86_64-w64-mingw32/bin/"{libc++.dll,libunwind.dll,libwinpthread-1.dll} "$root/dist/"
fi
ls -l "$root/dist/oui_paint.exe"
