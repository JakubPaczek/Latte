#!/usr/bin/env bash
set -u

# ----------------------------
# Config
# ----------------------------
COMPILER="${COMPILER:-./latc_x86_64}"
ROOT="${ROOT:-lattests}"
TMPDIR="${TMPDIR:-/tmp}"

# On Windows-like shells produce .exe
is_windows() {
  case "$(uname -s 2>/dev/null || echo "")" in
    MINGW*|MSYS*|CYGWIN*) return 0 ;;
    *) return 1 ;;
  esac
}

exe_path_for_lat() {
  local f="$1"
  local exe="${f%.lat}"
  if is_windows; then
    echo "${exe}.exe"
  else
    echo "${exe}"
  fi
}

# Keep stderr/stdout separate and enforce OK/ERROR convention
compile_ok() {
  local f="$1"
  local errfile="$2"
  local outfile="$3"

  # We want: first line of stderr must be OK
  # Latc prints OK/ERROR to stderr, so discard stdout, capture stderr.
  if "$COMPILER" "$f" >"$outfile" 2>"$errfile"; then
    return 0
  else
    return 1
  fi
}

first_line() {
  # prints first line or empty
  sed -n '1p' "$1" 2>/dev/null || true
}

run_and_capture() {
  local exe="$1"
  local in_file="$2"   # can be empty
  local out_file="$3"

  if [[ -n "$in_file" && -f "$in_file" ]]; then
    "$exe" < "$in_file" > "$out_file"
  else
    "$exe" > "$out_file"
  fi
}

# ----------------------------
# Test helpers
# ----------------------------
PASS=0
FAIL=0
FAIL_LIST=()

report_fail() {
  local msg="$1"
  echo "  [FAIL] $msg"
  FAIL=$((FAIL+1))
  FAIL_LIST+=("$msg")
}

report_pass() {
  PASS=$((PASS+1))
}

# test good: compile must succeed + OK + run output must match
test_good() {
  local f="$1"
  local base="${f%.lat}"
  local expected="${base}.output"
  local input="${base}.input"
  local exe
  exe="$(exe_path_for_lat "$f")"

  local errfile outjunk runout
  errfile="$(mktemp "$TMPDIR/latte_err.XXXXXX")"
  outjunk="$(mktemp "$TMPDIR/latte_outjunk.XXXXXX")"
  runout="$(mktemp "$TMPDIR/latte_run.XXXXXX")"

  echo "== GOOD $f =="

  if ! compile_ok "$f" "$errfile" "$outjunk"; then
    echo "  Compiler exited non-zero."
    echo "  stderr:"
    sed -n '1,80p' "$errfile" || true
    report_fail "$f (compile failed)"
    rm -f "$errfile" "$outjunk" "$runout"
    return
  fi

  local fl
  fl="$(first_line "$errfile")"
  if [[ "$fl" != "OK" ]]; then
    echo "  Expected first stderr line: OK"
    echo "  Got: ${fl}"
    sed -n '1,20p' "$errfile" || true
    report_fail "$f (missing OK)"
    rm -f "$errfile" "$outjunk" "$runout"
    return
  fi

  if [[ ! -x "$exe" ]]; then
    echo "  Missing executable: $exe"
    report_fail "$f (no exe)"
    rm -f "$errfile" "$outjunk" "$runout"
    return
  fi

  run_and_capture "$exe" "$input" "$runout"

  if [[ ! -f "$expected" ]]; then
    echo "  Missing expected output file: $expected"
    report_fail "$f (missing .output)"
    rm -f "$errfile" "$outjunk" "$runout"
    return
  fi

  if ! diff -u "$expected" "$runout"; then
    report_fail "$f (output mismatch)"
    rm -f "$errfile" "$outjunk" "$runout"
    return
  fi

  echo "  [PASS]"
  report_pass
  rm -f "$errfile" "$outjunk" "$runout"
}

# test bad: compile must fail OR at least print ERROR in first stderr line.
# Some implementations return non-zero; some return 0 but print ERROR (should be non-zero ideally).
test_bad() {
  local f="$1"
  local errfile outjunk
  errfile="$(mktemp "$TMPDIR/latte_err.XXXXXX")"
  outjunk="$(mktemp "$TMPDIR/latte_outjunk.XXXXXX")"

  echo "== BAD  $f =="

  if "$COMPILER" "$f" >"$outjunk" 2>"$errfile"; then
    # Compiler returned 0 — still must print ERROR (shouldn't happen in strict spec)
    local fl
    fl="$(first_line "$errfile")"
    if [[ "$fl" != "ERROR" ]]; then
      echo "  Expected compilation to be rejected (ERROR)."
      echo "  First stderr line: ${fl}"
      sed -n '1,20p' "$errfile" || true
      report_fail "$f (unexpected accept)"
      rm -f "$errfile" "$outjunk"
      return
    fi
  else
    # Non-zero exit, must have ERROR line
    local fl
    fl="$(first_line "$errfile")"
    if [[ "$fl" != "ERROR" ]]; then
      echo "  Compiler exited non-zero but did not print ERROR as first stderr line."
      echo "  First stderr line: ${fl}"
      sed -n '1,20p' "$errfile" || true
      report_fail "$f (missing ERROR)"
      rm -f "$errfile" "$outjunk"
      return
    fi
  fi

  echo "  [PASS]"
  report_pass
  rm -f "$errfile" "$outjunk"
}

# Heuristic: classify by path segment /good/ or /bad/
classify_and_test() {
  local f="$1"
  case "$f" in
    */bad/*)  test_bad "$f" ;;
    */good/*) test_good "$f" ;;
    *)
      # default: treat as good if it has .output, else treat as bad if path contains "bad"
      if [[ -f "${f%.lat}.output" ]]; then
        test_good "$f"
      else
        # fallback: try bad if name starts with bad
        if [[ "$(basename "$f")" == bad* ]]; then
          test_bad "$f"
        else
          # safest: run as good (will fail loudly if missing .output)
          test_good "$f"
        fi
      fi
      ;;
  esac
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

# 1) core good
shopt -s nullglob
for f in "$ROOT/good"/core0*.lat; do
  classify_and_test "$f"
done

# 2) bad0*
for f in "$ROOT/bad"/bad0*.lat; do
  classify_and_test "$f"
done

# 3) extensions: all .lat under lattests/extensions
if [[ -d "$ROOT/extensions" ]]; then
  while IFS= read -r -d '' f; do
    classify_and_test "$f"
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

echo "ALL TESTS OK"
