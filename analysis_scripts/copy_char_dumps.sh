#!/bin/bash

version=23

copy_char_file() {
    src="MMDK/PlayerData/$1/$1 $3.json"
    target=~/src/psychodrive/data/chars/$2_$4.json
    echo cp "$src" $target
    cp "$src" $target
    dos2unix $target
}

copy_char_dumps() {
    targetcharname=$2$version
    copy_char_file "$1" $targetcharname atemi atemi
    copy_char_file "$1" $targetcharname CharInfo charinfo
    copy_char_file "$1" $targetcharname commands commands
    copy_char_file "$1" $targetcharname HIT_DT hit
    copy_char_file "$1" $targetcharname moves_dict moves
    copy_char_file "$1" $targetcharname Names names
    copy_char_file "$1" $targetcharname rects rects
    copy_char_file "$1" $targetcharname tgroups trigger_groups
    copy_char_file "$1" $targetcharname triggers triggers
}

copy_char_dumps AKI aki
copy_char_dumps Akuma akuma
copy_char_dumps Blanka blanka
copy_char_dumps Cammy cammy
copy_char_dumps Chun-Li chunli
copy_char_dumps 'Dee Jay' deejay
copy_char_dumps Dhalsim dhalsim
copy_char_dumps Ed ed
copy_char_dumps 'E Honda' honda
copy_char_dumps Guile guile
copy_char_dumps Jamie jamie
copy_char_dumps JP jp
copy_char_dumps Juri juri
copy_char_dumps Ken ken
copy_char_dumps Kimberly kimberly
copy_char_dumps Lily lily
copy_char_dumps Luke luke
copy_char_dumps Manon manon
copy_char_dumps Marisa marisa
copy_char_dumps Rashid rashid
copy_char_dumps Ryu ryu
copy_char_dumps Zangief zangief