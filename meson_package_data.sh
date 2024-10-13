#!/bin/bash
set -e

rsync -avx "$MESON_SOURCE_ROOT"/data "$1"/
for chardir in $(find "$1"/data/chars -maxdepth 1 -type d); do
    char=$(basename "$chardir")
    zip -j "$chardir"/"$char".zip "$chardir"/*.json
    rm "$chardir"/*.json
done

if [[ $2 == "emscripten" ]]; then
    /usr/lib/emsdk/upstream/emscripten/tools/file_packager "$1"/psychodrive_files.data --js-output="$1"/psychodrive_files.js --preload "$1"/data@./data
    rm -r "$1"/data
fi