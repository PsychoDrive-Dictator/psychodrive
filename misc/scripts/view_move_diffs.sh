leftdifffiles=(left*)

diffmoves=()

diffmoves+=('quit')

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