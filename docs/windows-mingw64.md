# Windows Local Development with MSYS2 MinGW64

VertexNote currently builds cleanly on Windows through MSYS2 MinGW64. This is the
recommended local development path for the Qt shell.

## Install MSYS2

Install MSYS2 with winget:

```powershell
winget install --id MSYS2.MSYS2 --source winget --accept-source-agreements --accept-package-agreements --disable-interactivity
```

Update the MSYS2 base system:

```powershell
& C:\msys64\usr\bin\bash.exe -lc "pacman -Syuu --noconfirm"
& C:\msys64\usr\bin\bash.exe -lc "pacman -Syu --noconfirm"
```

Install the MinGW64 toolchain and runtime dependencies:

```powershell
& C:\msys64\usr\bin\bash.exe -lc "export MSYSTEM=MINGW64; export PATH=/mingw64/bin:/usr/bin:`$PATH; pacman -S --needed --noconfirm base-devel mingw-w64-x86_64-gcc mingw-w64-x86_64-make mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-pkgconf mingw-w64-x86_64-gettext-tools mingw-w64-x86_64-poppler mingw-w64-x86_64-libxml2 mingw-w64-x86_64-libzip mingw-w64-x86_64-lua mingw-w64-x86_64-portaudio mingw-w64-x86_64-libsndfile mingw-w64-x86_64-qpdf mingw-w64-x86_64-qt6-base mingw-w64-x86_64-qt6-printsupport git"
```

GoogleTest from MSYS2 can be incompatible with the current C++20 `std::u8string`
test usage, so the local CMake configuration downloads and builds GoogleTest in the
build tree with `-DDOWNLOAD_GTEST=ON`.

## Daily Commands

From PowerShell at the repository root:

```powershell
.\scripts\mingw64-dev.ps1 configure
.\scripts\mingw64-dev.ps1 build
.\scripts\mingw64-dev.ps1 test
.\scripts\mingw64-dev.ps1 run
```

Those default commands target the Qt shell and the Qt-backed build tree.

The default task is `all`, which configures, builds, builds unit tests, and runs the
full unit suite on the Qt shell path:

```powershell
.\scripts\mingw64-dev.ps1
```

## Manual Equivalent

```powershell
& C:\msys64\usr\bin\bash.exe -lc "export MSYSTEM=MINGW64; export PATH=/mingw64/bin:/usr/bin:`$PATH; export XDG_DATA_DIRS=/mingw64/share:/usr/local/share:/usr/share; cd /c/Users/andrvoicu/Desktop/repos/vertex-note && cmake -S . -B build/mingw64-qt -G Ninja -DENABLE_GTEST=ON -DDOWNLOAD_GTEST=ON -DENABLE_QT_SHELL=ON -DENABLE_LEGACY_GTK_SHELL=OFF -DCMAKE_PREFIX_PATH=/mingw64"
& C:\msys64\usr\bin\bash.exe -lc "export MSYSTEM=MINGW64; export PATH=/mingw64/bin:/usr/bin:`$PATH; export XDG_DATA_DIRS=/mingw64/share:/usr/local/share:/usr/share; cd /c/Users/andrvoicu/Desktop/repos/vertex-note && cmake --build build/mingw64-qt --target vertexnote-qt-shell"
& C:\msys64\usr\bin\bash.exe -lc "export MSYSTEM=MINGW64; export PATH=/mingw64/bin:/usr/bin:`$PATH; export XDG_DATA_DIRS=/mingw64/share:/usr/local/share:/usr/share; cd /c/Users/andrvoicu/Desktop/repos/vertex-note && cmake --build build/mingw64-qt --target test-units && ./build/mingw64-qt/test/test-units.exe"
```

## Notes

- The Qt shell executable is `build/mingw64-qt/vertex-note-qt-shell.exe`.
- MinGW64 can print a locale warning on Windows during tests. The current unit suite
  still passes with that warning.
- Keep MinGW64 and MSVC build directories separate. Use `build/mingw64-qt` for this path.
