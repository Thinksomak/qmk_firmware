#!/bin/bash
INPUT=$(cat)
INPUT="${INPUT//\\\\//}"
if echo "$INPUT" | grep -q -E 'macropad/keymaps/Thinksomak/config\.h|remove_sensitive_lines\.py'; then
  echo "Blocked: this file holds credentials and must not be read, edited, printed or committed." >&2
  exit 2
fi
exit 0
