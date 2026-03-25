#!/bin/bash
set -e

cd "$MESON_SOURCE_ROOT"

cookpath=
if [[ $2 == "emscripten" ]]; then
    cookpath="$1"/
else
    cookpath="$1"/data/cooked
fi
mkdir -p "$cookpath"
python3 "$MESON_SOURCE_ROOT"/misc/scripts/cook_all_chars.py "$cookpath"/ "$MESON_SOURCE_ROOT"/psychodrive

rsync -avx --exclude='chars' --exclude='cooked' "$MESON_SOURCE_ROOT"/data "$1"/

if [[ $2 == "emscripten" ]]; then
    /usr/lib/emsdk/upstream/emscripten/tools/file_packager "$1"/psychodrive_files.data --js-output="$1"/psychodrive_files.js --exclude \*cooked\* --preload "$1"/data@./data
    rm -r "$1"/data

    version=$(basename "$1")
    version=${version#psychodrive-}
    sed -i -e "s/version=/version=$version/g" "$1"/psychodrive.html

    latest_path="$(dirname "$1")"/psychodrive-latest
    rm -f "$latest_path"
    ln -s "$1" "$latest_path"
fi


