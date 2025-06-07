#!/bin/bash

. $(dirname "$0")/constants.sh

diff_char() {
    leftmoves="$(find_charfile "$1" "$2" "moves")"
    rightmoves="$(find_charfile "$1" "$3" "moves")"

    if [[ "$leftmoves" == "blah" ]]; then
        return
    fi

    tmpdir=/tmp/diffmoves/"$1"_"$2"_"$3"
    mkdir -p $tmpdir

    leftprocessed=$tmpdir/moves_processed_left
    rightprocessed=$tmpdir/moves_processed_right


    jq_prune_command='walk(if type == "object" then del(.Action, ._ActionName, .ActionID, .id, .BonePlaceKey, .BGPlaceKey, .VoiceKey, .WindKey, .VfxKey, .PlaceFG, .SteerFG, .__PlaceFG, .__SteerFG, ._IsUNIQUE_UNIQUE_PARAM_COPY, ._p01_UNIQUE_UNIQUE_PARAM_COPY, ._p02_UNIQUE_UNIQUE_PARAM_COPY, ._p04_UNIQUE_UNIQUE_PARAM_COPY, ._p05_UNIQUE_UNIQUE_PARAM_COPY, ._p06_UNIQUE_UNIQUE_PARAM_COPY, ._p13_GROUND_CORRECTION, ._p13_GROUND_CORRECTION_OFF, ._p13_IK_LOCK, ._p13_IK_UNLOCK, ._p13_JOINT_LOCK, ._p13_JOINT_MOVE, ._p13_JOINT_UNLOCK, ._p13_UNLOCK_JOINT_MOVE) else . end)'

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
}

if [[ $1 == "all" ]]; then
    for char in "${characters[@]}"; do
        ( diff_char "$char" "$2" "$3" ) &
    done
    wait
else
    diff_char "$1" "$2" "$3"
fi