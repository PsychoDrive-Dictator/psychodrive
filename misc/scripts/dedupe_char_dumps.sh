#!/bin/bash

versions=("19" "20" "21" "22" "23" "24" "25" "26" "30" "31" "32" "33")

dedupe_char_file() {
    sum=blah
    lastfile=blah
    lastfile_processed=blah
    for ver in ${versions[@]}; do
        filename=data/chars/$1/"$1""$ver"_$2.json
        filename_processed=data/chars/$1/"$1""$ver"_$2_processed.json
        if [ ! -f $filename ] || [ -L $filename ]; then
            if [ ! -L $filename ] && [ $lastfile != "blah" ]; then
                ln -s $lastfile $filename
                # ln -s $lastfile_processed $filename_processed
            fi
            continue
        fi
        lastfile="$1""$ver"_$2.json
        lastfile_processed="$1""$ver"_$2_processed.json
        newsum=$(md5sum $filename | cut -d' ' -f1)
        woulddelete=false
        if [ $newsum == $sum ]; then
            woulddelete=true
        fi
        echo $filename $newsum $woulddelete
        if [ $woulddelete == true ]; then
            git rm $filename
            rm $filename
        fi
        sum=$newsum

        # if [ $2 == "moves" ]; then
        #     jq 'walk(if type == "object" then del(._ComboMember, .PhyCoeff, ._p01_CHARA_RESET_CONDITION, ._p02_CHARA_RESET_CONDITION) else . end)' < $filename > $filename_processed
        # fi
    done
}

dedupe_char_dumps() {
    dedupe_char_file "$1" atemi
    dedupe_char_file "$1" assist
    dedupe_char_file "$1" charinfo
    dedupe_char_file "$1" commands
    dedupe_char_file "$1" hit
    dedupe_char_file "$1" moves
    dedupe_char_file "$1" names
    dedupe_char_file "$1" rects
    dedupe_char_file "$1" trigger_groups
    dedupe_char_file "$1" triggers
    dedupe_char_file "$1" charge
}

dedupe_char_dumps aki
dedupe_char_dumps akuma
dedupe_char_dumps blanka
dedupe_char_dumps cammy
dedupe_char_dumps chunli
dedupe_char_dumps deejay
dedupe_char_dumps dhalsim
dedupe_char_dumps dictator
dedupe_char_dumps ed
dedupe_char_dumps elena
dedupe_char_dumps honda
dedupe_char_dumps guile
dedupe_char_dumps jamie
dedupe_char_dumps jp
dedupe_char_dumps juri
dedupe_char_dumps ken
dedupe_char_dumps kimberly
dedupe_char_dumps lily
dedupe_char_dumps luke
dedupe_char_dumps mai
dedupe_char_dumps manon
dedupe_char_dumps marisa
dedupe_char_dumps rashid
dedupe_char_dumps ryu
dedupe_char_dumps terry
dedupe_char_dumps zangief
