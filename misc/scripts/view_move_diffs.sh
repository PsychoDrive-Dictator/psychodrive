leftdifffiles=(left*)

if [[ $1 == "print" ]]; then
    for leftdifffile in "${leftdifffiles[@]}"; do
        move="${leftdifffile/left/}"
        rightfile="${leftdifffile/left/right}"
        echo "$move"
        diff -u "$leftdifffile" "$rightfile"
    done
    exit 0
else
    diffmoves=('quit')
    for leftdifffile in "${leftdifffiles[@]}"; do
        diffmoves+=("${leftdifffile/left/}")
    done

    while true; do
        choice=$(printf "%s\n" "${diffmoves[@]}" | fzf)
        echo "Your choice is $choice"
        if [[ $choice == 'quit' ]]; then
            exit 0
        fi
        code --diff left"$choice" right"$choice"
    done
    exit 0
fi