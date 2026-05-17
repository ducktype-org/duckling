#!/bin/bash

# Specify the directory to start in (default is current directory)
TARGET_DIR="${1:-.}"

# Check if directory exists
if [ ! -d "$TARGET_DIR" ]; then
  echo "Error: $TARGET_DIR is not a directory."
  exit 1
fi

# Use find to locate files/dirs containing a dash
# -depth ensures we rename files before their parent directories
find "$TARGET_DIR" -depth -name "*-*" | while read -r file; do
    # Get the directory and the filename separately
    dirname=$(dirname "$file")
    basename=$(basename "$file")

    # Substitute dashes for underscores in the filename
    new_name="${basename//-/_}"

    # Perform the rename
    mv -n "$file" "$dirname/$new_name"

    echo "Renamed: $basename -> $new_name"
done

echo "Done!"
