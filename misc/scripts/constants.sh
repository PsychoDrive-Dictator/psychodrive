#!/bin/bash

versions=("19" "20" "21" "22" "23" "24" "25" "26" "30" "31" "32" "33" "34" "35" "36" "37" "38")

characters=(
    "ryu"
    "luke"
    "kimberly"
    "chunli"
    "manon"
    "zangief"
    "jp"
    "dhalsim"
    "cammy"
    "ken"
    "deejay"
    "lily"
    "aki"
    "rashid"
    "blanka"
    "juri"
    "marisa"
    "guile"
    "ed"
    "honda"
    "jamie"
    "akuma"
    "dictator"
    "terry"
    "mai"
    "elena"
    "sagat"
    "viper"
)

find_charfile() {
    char="$1"
    target_version="$2"
    file="$3"

    lastgoodfile=blah

    for ver in ${versions[@]}; do
        filename=data/chars/$1/"$1""$ver"_$3.json
        if [ -f $filename ]; then
            lastgoodfile="$filename"
        fi
        if [[ "$ver" == "$target_version" ]]; then
            break
        fi
    done
    echo "$lastgoodfile"
}
