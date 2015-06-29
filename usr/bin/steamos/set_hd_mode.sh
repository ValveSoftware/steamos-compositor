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

GOODMODES=("1920x1080" "1280x720")
GOODRATES=("60.0" "59.9") # CEA modes guarantee or one the other, but not both?

# First, some logging
date
xrandr --verbose

# Improve this later to prioritize HDMI/DP over LVDS; for now, use first output
OUTPUT_NAME=`xrandr | grep ' connected' | head -n1 | cut -f1 -d' '`

ALL_OUTPUT_NAMES=`xrandr | grep ' connected' | cut -f1 -d' '`

# Disable everything but the first output
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