#!/bin/sh
# Builds and runs the calculator unit test with the PC's gcc (QMK MSYS has
# one at /mingw64/bin/gcc). Prints each failure and a summary.
here=$(cd "$(dirname "$0")" && pwd)
out="${TMPDIR:-/tmp}/calc_test.exe"
gcc -std=c11 -Wall -Wextra -Werror -o "$out" "$here/calc_test.c" "$here/../calc.c" && "$out"
