#!/bin/bash

#
# Example usage:
# rename_cert_files.sh 0b854c3f6515cc03d153cae2e1c3aef22c68b789a223ad48868c913dc8bf3aad coop-snooper-charlie-house
#

# Check if exactly two arguments are provided
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <search_string> <replacement_string>"
    exit 1
fi

search_string="$1"
replacement_string="$2"

# Loop over files that start with the search string
for f in "$search_string"*; do
    # Check if the file exists (in case no files match)
    if [ -e "$f" ]; then
        mv -- "$f" "${replacement_string}${f#"$search_string"}"
    fi
done
