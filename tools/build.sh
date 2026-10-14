#!/usr/bin/env bash
# Configures, builds, and tests OUI Paint on Linux and macOS: optimised, with debugging information, in build/, as
# bin/oui_paint is made here, so what is measured or tried is as fast as what a release carries.
#
# usage: tools/build.sh [--no-tests] [--clean]
#   --no-tests  Skip ctest.
#   --clean     Delete the build directory first, for a cold build.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$root/build"
run_tests=1
clean=0

for argument in "$@"; do
    case "$argument" in
        --no-tests) run_tests=0 ;;
        --clean) clean=1 ;;
        *) echo "Unknown option: $argument" >&2; exit 2 ;;
    esac
done

if [[ $clean -eq 1 ]]; then
    rm -rf "$build_dir"
fi

if [[ "$(uname -s)" == Darwin ]]; then
    jobs="$(sysctl -n hw.ncpu)"
else
    jobs="$(nproc)"
fi
cmake -S "$root" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$build_dir"
if [[ $run_tests -eq 1 ]]; then
    ctest --test-dir "$build_dir" --output-on-failure -j "$jobs" --timeout 300
fi
