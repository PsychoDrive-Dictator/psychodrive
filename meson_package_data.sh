#!/bin/bash
set -e

cd "$MESON_SOURCE_ROOT"
python3 "$MESON_SOURCE_ROOT"/misc/scripts/cook_all_chars.py "$1"/ "$MESON_SOURCE_ROOT"/psychodrive

rsync -avx --exclude='chars' --exclude='cooked' "$MESON_SOURCE_ROOT"/data "$1"/

if [[ $2 == "emscripten" ]]; then
    /usr/lib/emsdk/upstream/emscripten/tools/file_packager "$1"/psychodrive_files.data --js-output="$1"/psychodrive_files.js --exclude \*cooked\* --preload "$1"/data@./data
fi

rm -r "$1"/data

latest_path="$(dirname "$1")"/psychodrive-latest
rm -f "$latest_path"
ln -s "$1" "$latest_path"