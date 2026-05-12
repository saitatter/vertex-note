#!/usr/bin/env bash

set -e
set -o pipefail

# Build and package the application
if [ $# -eq 0 ]; then
  echo 'Please provide the CMake install prefix path'
  exit 1
fi

root_dir=$(python3 -c 'import pathlib, sys; print(pathlib.Path(sys.argv[1]).resolve().parent.parent)' "$0")
install_prefix="$(cd "$1"; pwd)"

build_dir="$root_dir"/build
mkdir -p "$build_dir"

pushd "$build_dir"
cmake .. -GNinja -DCMAKE_INSTALL_PREFIX="$install_prefix"
cmake --build .
cmake --install .
popd

bash ./build-app.sh "$install_prefix"
