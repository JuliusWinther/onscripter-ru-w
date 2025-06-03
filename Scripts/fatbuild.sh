#!/bin/bash

#
# fatbuild.sh
# ONScripter-RU
#
# macOS FAT file generation (embeds multiple architectures).
# Run with "i386/executable" "x86_64/executable" "x86_64h/executable" "target/executable" arguments.
#
# Consult LICENSE file for licensing terms and copyright holders.
#

if (( $# < 4 )); then # Changed from 5
  echo "Usage: x86_64/executable x86_64h/executable target/executable action" # Updated
  exit 1
fi

# executable32="${1}" # Removed
executable64="${1}"  # Was ${2}
executable64h="${2}" # Was ${3}
executabledst="${3}" # Was ${4}
action="${4}"        # Was ${5}

if [ "$action" == "clean" ]; then
	exit 0
fi

# echo "EXE32:  ${executable32}" # Removed
echo "EXE64:  ${executable64}"
echo "EXE64h: ${executable64h}"
echo "DST:    ${executabledst}"

# Updated condition
if [ ! -x "${executable64}" ] || [ ! -x "${executable64h}" ] || [ ! -x "${executabledst}" ]; then
  echo "Missing dependent app for merging!"
  exit 1
fi

rm -rf /tmp/onscripter-ru-exec
mkdir -p /tmp/onscripter-ru-exec || exit 1

# cp "${executable32}" /tmp/onscripter-ru-exec/ons32 # Removed
cp "${executable64}" /tmp/onscripter-ru-exec/ons64
cp "${executable64h}" /tmp/onscripter-ru-exec/ons64h

# Set cpu_subtype to Haswell (until Xcode supports compiling for x86_64h)
echo -n -e "\x08\x00\x00\x00" | dd of="/tmp/onscripter-ru-exec/ons64h" bs=1 seek=8 count=4 conv=notrunc

# Updated lipo command
lipo -create "/tmp/onscripter-ru-exec/ons64" \
			"/tmp/onscripter-ru-exec/ons64h" -output "${executabledst}" || exit 1

rm -rf /tmp/onscripter-ru-exec

exit 0
