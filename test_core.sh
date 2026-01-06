#!/usr/bin/env bash
set -euo pipefail

for f in lattests/good/core0*.lat; do
  echo "== $f =="
  ./latc_x86 "$f" >/dev/null

  exe="${f%.lat}"
  out_expected="${f%.lat}.output"
  in_file="${f%.lat}.input"

  if [[ -f "$in_file" ]]; then
    "$exe" < "$in_file" > /tmp/latte_out
  else
    "$exe" > /tmp/latte_out
  fi

  diff -u "$out_expected" /tmp/latte_out
done

echo "ALL core0* OK"
