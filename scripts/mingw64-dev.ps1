param(
    [ValidateSet("configure", "build", "test", "vertex-tests", "run", "all")]
    [string] $Task = "all"
)

$ErrorActionPreference = "Stop"

$bash = "C:\msys64\usr\bin\bash.exe"
if (-not (Test-Path -LiteralPath $bash)) {
    throw "MSYS2 was not found at $bash. Install MSYS2 first, then run this script again."
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$escapedRepoRoot = $repoRoot.Replace("'", "'\''")
$repoUnix = (& $bash -lc "cygpath -u '$escapedRepoRoot'").Trim()

function Invoke-Mingw64 {
    param([Parameter(Mandatory = $true)][string] $Command)

    $prefix = "export MSYSTEM=MINGW64; export PATH=/mingw64/bin:/usr/bin:`$PATH; export XDG_DATA_DIRS=/mingw64/share:/usr/local/share:/usr/share; cd '$repoUnix'"
    & $bash -lc "$prefix && $Command"
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

function Configure-VertexNote {
    Invoke-Mingw64 "cmake -S . -B build/mingw64 -G Ninja -DENABLE_GTEST=ON -DDOWNLOAD_GTEST=ON"
}

function Build-VertexNote {
    Invoke-Mingw64 "cmake --build build/mingw64"
}

function Build-Tests {
    Invoke-Mingw64 "cmake --build build/mingw64 --target test-units"
}

switch ($Task) {
    "configure" {
        Configure-VertexNote
    }
    "build" {
        Build-VertexNote
    }
    "test" {
        Build-Tests
        Invoke-Mingw64 "./build/mingw64/test/test-units.exe"
    }
    "vertex-tests" {
        Build-Tests
        Invoke-Mingw64 "./build/mingw64/test/test-units.exe --gtest_filter='VertexNote*'"
    }
    "run" {
        Build-VertexNote
        Invoke-Mingw64 "./build/mingw64/src/vertex-note.exe"
    }
    "all" {
        Configure-VertexNote
        Build-VertexNote
        Build-Tests
        Invoke-Mingw64 "./build/mingw64/test/test-units.exe"
    }
}
