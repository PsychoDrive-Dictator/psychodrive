#!/bin/bash

versions=("19" "20" "21" "22" "23" "24" "25" "26" "30" "31")

tmpdir=/tmp/diffmoves/"$1"_"$2"_"$3"
mkdir -p $tmpdir

leftmoves=data/chars/$1/"$1""$2"_moves.json
rightmoves=data/chars/$1/"$1""$3"_moves.json

leftprocessed=$tmpdir/moves_processed_left
rightprocessed=$tmpdir/moves_processed_right


jq_prune_command='walk(if type == "object" then del(.ActionID, .id, .BGPlaceKey, .VoiceKey, .WindKey, .VfxKey, .PlaceFG, .SteerFG, .__PlaceFG, .__SteerFG, ._IsUNIQUE_UNIQUE_PARAM_COPY, ._p01_UNIQUE_UNIQUE_PARAM_COPY, ._p02_UNIQUE_UNIQUE_PARAM_COPY, ._p04_UNIQUE_UNIQUE_PARAM_COPY, ._p05_UNIQUE_UNIQUE_PARAM_COPY, ._p06_UNIQUE_UNIQUE_PARAM_COPY) else . end)'

jq "$jq_prune_command" < $leftmoves > $leftprocessed
jq "$jq_prune_command" < $rightmoves > $rightprocessed

keys_string=$(jq -r 'keys[]' < $rightprocessed)

readarray -t keys <<<"$keys_string"

for key in "${keys[@]}"; do
    leftkeyfile=$tmpdir/left"$key"
    rightkeyfile=$tmpdir/right"$key"
    jq ".\"$key\"" < $leftprocessed > "$leftkeyfile"
    jq ".\"$key\"" < $rightprocessed > "$rightkeyfile"
    if cmp --silent -- "$leftkeyfile" "$rightkeyfile"; then
        rm "$leftkeyfile" "$rightkeyfile"
    fi
done
