#!/bin/bash

# This script attempts to set a known-good mode on a good output

function contains() {
    local n=$#
    local value=${!n}
    for ((i=1;i < $#;i++)) {
        if [ "${!i}" == "${value}" ]; then
            echo "y"
            return 0
        fi
    }
    echo "n"
    return 1
}

# This function echoes the first element from first argument array, matching a
# prefix in the order given by second argument array.
function first_by_prefix_order() {
    local values=${!1}
    local prefix_order=${!2}
    for prefix in ${prefix_order[@]} ; do
        for val in ${values[@]} ; do
            if [[ $val =~ ^$prefix ]] ; then echo $val ; return ; fi
        done
    done
}

GOODMODES=("1920x1080" "1280x720")
GOODRATES=("60.0" "59.9") # CEA modes guarantee or one the other, but not both?

# First, some logging
date
xrandr --verbose

# List connected outputs
ALL_OUTPUT_NAMES=$(xrandr | grep ' connected' | cut -f1 -d' ')
# Default to first connected output
OUTPUT_NAME=$(echo $ALL_OUTPUT_NAMES | cut -f1 -d' ')

# If any is connected, give priority to HDMI then DP
OUTPUT_PRIORITY="HDMI DP"
PREFERRED_OUTPUT=$(first_by_prefix_order ALL_OUTPUT_NAMES[@] OUTPUT_PRIORITY[@])
if [[ -n "$PREFERRED_OUTPUT" ]] ; then
    OUTPUT_NAME=$PREFERRED_OUTPUT
fi

# Disable everything but the selected output
for i in $ALL_OUTPUT_NAMES; do
	if [ "$i" != "$OUTPUT_NAME" ]; then
		xrandr --output "$i" --off
	fi
done


CURRENT_MODELINE=`xrandr | grep \* | tr -s ' ' | head -n1`

CURRENT_MODE=`echo "$CURRENT_MODELINE" | cut -d' ' -f2`
CURRENT_RATE=`echo "$CURRENT_MODELINE" | tr ' ' '\n' | grep \* | tr -d \* | tr -d +`

# If the current mode is already deemed good, we're good, exit
if [ $(contains "${GOODMODES[@]}" "$CURRENT_MODE") == "y" ]; then
	if [ $(contains "${GOODRATES[@]}" "$CURRENT_RATE") == "y" ]; then
	exit 0
	fi
fi

# Otherwise try to set combinations of good modes/rates until it works
for goodmode in "${GOODMODES[@]}"; do
	for goodrate in "${GOODRATES[@]}"; do
		xrandr --output "$OUTPUT_NAME" --mode "$goodmode" --refresh "$goodrate"
		# If good return, we're done
		if [[ $? -eq 0 ]]; then
			exit 0
		fi
	done
done

exit 1