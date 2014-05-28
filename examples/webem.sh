#!/bin/sh

if [ -z $1 ] || [ $1 = "-h" ]; then
	echo "usage: webem.sh [OUTPUT_FILE]"
	exit 1
fi

get_coords() {
	X=$1
	Y=$2
	W=$3
	H=$4
}; get_coords `xregion`

ffmpeg -f x11grab -r 30 -s ${W}x$H -i $DISPLAY.0+$X,$Y -vcodec libvpx -preset ultrafast -crf 12 -b:v 500K -threads 0 "$*"
