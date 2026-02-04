#!/bin/bash
set -e

mkdir -p "$1"/data/cooked
cd "$MESON_SOURCE_ROOT"
python3 "$MESON_SOURCE_ROOT"/cook_all.py "$1"/data/cooked "$MESON_SOURCE_ROOT"/psychodrive

rsync -avx --exclude='chars' "$MESON_SOURCE_ROOT"/data "$1"/

if [[ $2 == "emscripten" ]]; then
    for binfile in "$1"/data/cooked/*.bin; do
        charver=$(basename "$binfile" .bin)
        /usr/lib/emsdk/upstream/emscripten/tools/file_packager "$1"/psychodrive_char_"$charver".data --js-output="$1"/psychodrive_char_"$charver".js --embed "$binfile"@./data/cooked/"$(basename "$binfile")" &
    done
    wait

    /usr/lib/emsdk/upstream/emscripten/tools/file_packager "$1"/psychodrive_files.data --js-output="$1"/psychodrive_files.js --exclude \*cooked\* --embed "$1"/data@./data
fi

rm -r "$1"/data

latest_path="$(dirname "$1")"/psychodrive-latest
rm -f "$latest_path"
ln -s "$1" "$latest_path"