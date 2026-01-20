#!/usr/bin/env bash
set -Eeuo pipefail

# ----------------------------
# Config (override via env)
# ----------------------------
ROOT="${ROOT:-lattests}"
COMPILER="${COMPILER:-./latc_x86_64}"
RUNTIME_C="${RUNTIME_C:-./lib/runtime.c}"
TMPDIR="${TMPDIR:-/tmp}"

# link flags (some distros need -no-pie)
LINKFLAGS=(${LINKFLAGS:-"-no-pie"})

# ----------------------------
# Helpers
# ----------------------------
green()  { printf "\033[32m%s\033[0m\n" "$*"; }
red()    { printf "\033[31m%s\033[0m\n" "$*"; }
yellow() { printf "\033[33m%s\033[0m\n" "$*"; }

first_lines() { sed -n "1,${2:-30}p" "$1" 2>/dev/null || true; }

PASS=0
FAIL=0
SKIP=0
FAIL_LIST=()

pass() { PASS=$((PASS+1)); }
fail() { FAIL=$((FAIL+1)); FAIL_LIST+=("$1"); }

need() {
  if [[ ! -e "$1" ]]; then
    echo "ERROR: missing: $1" >&2
    exit 2
  fi
}

# compile runtime once into a temp .o
build_runtime() {
  RUNTIME_O="$(mktemp "$TMPDIR/latte_runtime.XXXXXX.o")"
  gcc -c "$RUNTIME_C" -o "$RUNTIME_O"
}

# Runs backend pipeline for ONE .lat
# mode = good | bad
run_one() {
  local lat="$1"
  local mode="$2"

  local base="${lat%.lat}"
  local name
  name="$(basename "$base")"

  local sfile="${base}.s"
  local expected="${base}.output"
  local input="${base}.input"

  local exe out got diff c_log l_log r_err
  exe="$(mktemp "$TMPDIR/latte_exe.${name}.XXXXXX")"
  got="$(mktemp "$TMPDIR/latte_got.${name}.XXXXXX")"
  c_log="$(mktemp "$TMPDIR/latte_compile.${name}.XXXXXX.log")"
  l_log="$(mktemp "$TMPDIR/latte_link.${name}.XXXXXX.log")"
  r_err="$(mktemp "$TMPDIR/latte_run.${name}.XXXXXX.err")"
  diff="$(mktemp "$TMPDIR/latte_diff.${name}.XXXXXX")"

  echo "==> $name"

  rm -f "$sfile"

  # 1) lat -> s
  if "$COMPILER" "$lat" >/dev/null 2>"$c_log"; then
    if [[ "$mode" == "bad" ]]; then
      red "FAIL (expected compile error) $name"
      echo "  compiler stderr (first 30 lines):"
      first_lines "$c_log" 30
      fail "$lat (expected COMPILE_FAIL)"
      cleanup_tmp "$exe" "$got" "$c_log" "$l_log" "$r_err" "$diff"
      echo
      return
    fi
  else
    if [[ "$mode" == "bad" ]]; then
      green "OK (compile failed as expected) $name"
      pass
      cleanup_tmp "$exe" "$got" "$c_log" "$l_log" "$r_err" "$diff"
      echo
      return
    fi
    red "COMPILE_FAIL $name"
    echo "  compiler stderr (first 30 lines):"
    first_lines "$c_log" 30
    fail "$lat (compile failed)"
    cleanup_tmp "$exe" "$got" "$c_log" "$l_log" "$r_err" "$diff"
    echo
    return
  fi

  # sanity: compiler should have produced .s
  if [[ ! -f "$sfile" ]]; then
    red "COMPILE_FAIL (no .s produced) $name"
    echo "  compiler stderr (first 30 lines):"
    first_lines "$c_log" 30
    fail "$lat (no .s produced)"
    cleanup_tmp "$exe" "$got" "$c_log" "$l_log" "$r_err" "$diff"
    echo
    return
  fi

  # 2) link
  if ! gcc "${LINKFLAGS[@]}" "$sfile" "$RUNTIME_O" -o "$exe" >"$l_log" 2>&1; then
    red "LINK_FAIL $name"
    echo "  linker log (first 30 lines):"
    first_lines "$l_log" 30
    fail "$lat (link failed)"
    cleanup_tmp "$exe" "$got" "$c_log" "$l_log" "$r_err" "$diff"
    echo
    return
  fi

  # 3) run
  if [[ -f "$input" ]]; then
    if ! "$exe" <"$input" >"$got" 2>"$r_err"; then
      red "RUN_FAIL $name"
      echo "  stderr (first 30 lines):"
      first_lines "$r_err" 30
      fail "$lat (run failed)"
      cleanup_tmp "$exe" "$got" "$c_log" "$l_log" "$r_err" "$diff"
      echo
      return
    fi
  else
    if ! "$exe" >"$got" 2>"$r_err"; then
      red "RUN_FAIL $name"
      echo "  stderr (first 30 lines):"
      first_lines "$r_err" 30
      fail "$lat (run failed)"
      cleanup_tmp "$exe" "$got" "$c_log" "$l_log" "$r_err" "$diff"
      echo
      return
    fi
  fi

  # normalize CRLF in program output
  tr -d '\r' <"$got" >"${got}.tmp" && mv "${got}.tmp" "$got"

  # 4) compare if expected exists
  if [[ -f "$expected" ]]; then
    if diff -u "$expected" "$got" >"$diff"; then
      green "OK $name"
      pass
    else
      red "FAIL $name"
      echo "----- diff (first 80 lines) -----"
      first_lines "$diff" 80
      fail "$lat (output differs)"
    fi
  else
    yellow "NOOUTPUT $name (no .output file)"
    SKIP=$((SKIP+1))
  fi

  cleanup_tmp "$exe" "$got" "$c_log" "$l_log" "$r_err" "$diff"
  echo
}

cleanup_tmp() {
  rm -f "$@"
}

# ----------------------------
# Main
# ----------------------------
need "$COMPILER"
need "$ROOT"
need "$RUNTIME_C"

if [[ ! -x "$COMPILER" ]]; then
  echo "ERROR: compiler not executable: $COMPILER" >&2
  exit 2
fi

build_runtime

echo "Using compiler: $COMPILER"
echo "Tests root:     $ROOT"
echo

shopt -s nullglob

# good
for f in "$ROOT/good"/*.lat "$ROOT/good"/**/*.lat; do
  [[ -f "$f" ]] || continue
  run_one "$f" good
done

# bad (expect compile fail)
for f in "$ROOT/bad"/*.lat "$ROOT/bad"/**/*.lat; do
  [[ -f "$f" ]] || continue
  run_one "$f" bad
done

# extensions (heuristic like in your frontend script)
if [[ -d "$ROOT/extensions" ]]; then
  while IFS= read -r -d '' f; do
    case "$f" in
      */bad/*) run_one "$f" bad ;;
      *)       run_one "$f" good ;;
    esac
  done < <(find "$ROOT/extensions" -type f -name "*.lat" -print0 | sort -z)
fi

rm -f "$RUNTIME_O"

echo "========================"
echo "PASS: $PASS"
echo "FAIL: $FAIL"
echo "NOOUTPUT: $SKIP"
if (( FAIL != 0 )); then
  echo
  echo "Failed tests:"
  for x in "${FAIL_LIST[@]}"; do
    echo " - $x"
  done
  exit 1
fi
echo "ALL BACKEND TESTS OK"
