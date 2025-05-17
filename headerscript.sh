#!/usr/bin/env bash
set -euo pipefail

# Path to your new header block (no trailing blank line)
HEADER_FILE=./header.txt
HEADER_CONTENT=$(<"$HEADER_FILE")

find . -type f \( -name '*.c' -o -name '*.h' \) | while IFS= read -r src; do
  # Skip if SPDX tag already present
  if grep -q 'SPDX-License-Identifier: Apache-2.0' "$src"; then
    echo "Skipping (already updated): $src"
    continue
  fi

  tmp=$(mktemp)
  # Remove the very first C comment (from /* ... */) and any whitespace/newlines after it
  perl -0777 -pe 's{\A\s*/\*.*?\*/\s*}{}s' "$src" > "$tmp"

  # Prepend the new header
  {
    printf '%s\n\n' "$HEADER_CONTENT"
    cat "$tmp"
  } > "$src"

  rm "$tmp"
  echo "Updated header: $src"
done
