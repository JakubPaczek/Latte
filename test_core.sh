#!/usr/bin/env bash
set -euo pipefail

for f in lattests/good/core0*.lat; do
    echo "== $f =="
    ./latc_x86_64 "$f" >/dev/null

    exe="${f%.lat}"
    out_expected="${f%.lat}.output"
    in_file="${f%.lat}.input"

    if [[ "$(uname -s)" == MINGW* || "$(uname -s)" == MSYS* || "$(uname -s)" == CYGWIN* ]]; then
    exe_run="${exe}.exe"
    else
    exe_run="${exe}"
    fi

    if [[ -f "$in_file" ]]; then
    "$exe_run" < "$in_file" > /tmp/latte_out
    else
    "$exe_run" > /tmp/latte_out
    fi


    diff -u "$out_expected" /tmp/latte_out
done

echo "ALL core0* OK"
