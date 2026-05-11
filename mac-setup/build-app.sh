#!/usr/bin/env bash

set -e

if [ $# -eq 0 ]; then
  echo 'Please provide the CMake install prefix path'
  exit 1
fi

install_prefix="$(cd "$1"; pwd)"

# go to script directory
cd "$(dirname "$0")" || exit

# delete old app, if there
echo "clean old app"
rm -rf ./VertexNote.app

if ! command -v macdeployqt >/dev/null 2>&1; then
  echo "error: macdeployqt was not found on PATH"
  exit 1
fi

echo "create package"

executable="$install_prefix/bin/vertex-note-qt-shell"
[ ! -x "$executable" ] && echo "$executable doesn't exist or is not executable!" && exit 1

mkdir -p VertexNote.app/Contents/MacOS
mkdir -p VertexNote.app/Contents/Resources
cp Info.plist VertexNote.app/Contents/Info.plist
cp "$executable" VertexNote.app/Contents/MacOS/
cp icon/vertex-note.icns VertexNote.app/Contents/Resources/
if [ -d "$install_prefix/share/vertex-note" ]; then
  cp -R "$install_prefix/share/vertex-note" VertexNote.app/Contents/Resources/
fi
if [ -d "$install_prefix/share/poppler" ]; then
  mkdir -p VertexNote.app/Contents/Resources/share
  cp -R "$install_prefix/share/poppler" VertexNote.app/Contents/Resources/share/
fi

macdeployqt VertexNote.app -verbose=1

echo "Create zip"
zip --filesync -r VertexNote.zip VertexNote.app

echo "finished"
