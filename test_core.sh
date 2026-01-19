#!/usr/bin/env bash
set -u

# ----------------------------
# Config
# ----------------------------
COMPILER="${COMPILER:-./latc}"   # frontend-only
ROOT="${ROOT:-lattests}"
TMPDIR="${TMPDIR:-/tmp}"

first_line() {
  sed -n '1p' "$1" 2>/dev/null || true
}

PASS=0
FAIL=0
FAIL_LIST=()

pass() { PASS=$((PASS+1)); }
fail() {
  local msg="$1"
  echo "  [FAIL] $msg"
  FAIL=$((FAIL+1))
  FAIL_LIST+=("$msg")
}

run_expect() {
  local f="$1"
  local expect="$2" # OK or ERROR

  local errfile outjunk
  errfile="$(mktemp "$TMPDIR/latte_err.XXXXXX")"
  outjunk="$(mktemp "$TMPDIR/latte_outjunk.XXXXXX")"

  echo "== $expect $f =="

  # We only care about first stderr line.
  # Frontend may exit non-zero on ERROR; that's fine.
  "$COMPILER" "$f" >"$outjunk" 2>"$errfile" || true

  local fl
  fl="$(first_line "$errfile")"

  if [[ "$fl" != "$expect" ]]; then
    echo "  Expected first stderr line: $expect"
    echo "  Got: ${fl}"
    echo "  stderr (first 30 lines):"
    sed -n '1,30p' "$errfile" || true
    fail "$f (expected $expect)"
  else
    echo "  [PASS]"
    pass
  fi

  rm -f "$errfile" "$outjunk"
}

# ----------------------------
# Main
# ----------------------------
if [[ ! -x "$COMPILER" ]]; then
  echo "ERROR: compiler not found or not executable: $COMPILER" >&2
  exit 2
fi

echo "Using compiler: $COMPILER"
echo

shopt -s nullglob

# good: core
for f in "$ROOT/good"/core0*.lat; do
  run_expect "$f" "OK"
done

# bad: bad0*
for f in "$ROOT/bad"/bad0*.lat; do
  run_expect "$f" "ERROR"
done

# extensions: heuristics by path segment
if [[ -d "$ROOT/extensions" ]]; then
  while IFS= read -r -d '' f; do
    case "$f" in
      */bad/*)  run_expect "$f" "ERROR" ;;
      *)        run_expect "$f" "OK" ;;
    esac
  done < <(find "$ROOT/extensions" -type f -name "*.lat" -print0 | sort -z)
fi

echo
echo "========================"
echo "PASS: $PASS"
echo "FAIL: $FAIL"
if [[ $FAIL -ne 0 ]]; then
  echo
  echo "Failed tests:"
  for x in "${FAIL_LIST[@]}"; do
    echo " - $x"
  done
  exit 1
fi

echo "ALL FRONTEND TESTS OK"
