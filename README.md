# OUI Paint

A raster image editor in the spirit of Paint.NET, built on OUI: layers, selections that can be moved, turned and
scaled, Paint.NET's tools, effects and keys, 8 and 16 bits per channel, indexed images with palettes, OpenRaster,
the system clipboard both ways, and plugins in JavaScript. See [PLAN.md](PLAN.md), the keys in
[doc/shortcuts.md](doc/shortcuts.md) and how to write plugins in [doc/plugins.md](doc/plugins.md).

## Building

OUI comes as a git submodule:

```sh
git clone --recurse-submodules <this repository>
```

Linux, with CMake, Ninja, a C++17 compiler and GoogleTest (`libgtest-dev`):

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
bin/oui_paint
```

Copying and pasting images uses `wl-copy` and `wl-paste` on Wayland, `xclip` on X11.

Windows, cross-built on Linux with llvm-mingw, which the script fetches once:

```sh
tools/build_windows.sh            # dist/oui_paint.exe, needing nothing installed beside it
tools/build_windows.sh --tests    # also dist/oui_paint_tests.exe, to run on Windows or with wine
```

## License

zlib, as OUI. Third-party pieces and their licenses are listed in [THIRD_PARTY.md](THIRD_PARTY.md).
