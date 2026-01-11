#!/usr/bin/env bash
set -Eeuo pipefail

trap 'rc=$?; echo "ERROR at line $LINENO: $BASH_COMMAND (exit $rc)" >&2; exit $rc' ERR

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GOOD_DIR="${ROOT_DIR}/lattests/good"
COMPILER="${ROOT_DIR}/latc_x86_64"
RUNTIME_C="${ROOT_DIR}/lib/runtime.c"
RUNTIME_O="${ROOT_DIR}/lib/runtime.o"

green() { printf "\033[32m%s\033[0m\n" "$*"; }
red()   { printf "\033[31m%s\033[0m\n" "$*"; }
yellow(){ printf "\033[33m%s\033[0m\n" "$*"; }

if [[ ! -x "${COMPILER}" ]]; then
  echo "ERROR: missing compiler: ${COMPILER}" >&2
  echo "Run: make" >&2
  exit 2
fi

if [[ ! -d "${GOOD_DIR}" ]]; then
  echo "ERROR: missing tests dir: ${GOOD_DIR}" >&2
  exit 2
fi

# build runtime once
gcc -c "${RUNTIME_C}" -o "${RUNTIME_O}"

shopt -s nullglob
tests=( "${GOOD_DIR}"/*.lat )
shopt -u nullglob

echo "Running tests from: ${GOOD_DIR}"
echo "Found ${#tests[@]} .lat files"
echo

if (( ${#tests[@]} == 0 )); then
  echo "ERROR: no .lat files found" >&2
  exit 2
fi

total=0
ok=0
fail=0
noout=0

for lat in "${tests[@]}"; do
  ((total+=1))

  base="${lat%.lat}"
  name="$(basename "${base}")"
  sfile="${base}.s"
  exe="${base}"
  expected="${base}.output"
  got="${base}.got"
  diff_file="${base}.diff"

  compile_log="${base}.compile.log"
  link_log="${base}.link.log"
  run_err="${base}.run.err"

  echo "==> ${name}"

  rm -f "${sfile}" "${exe}" "${got}" "${diff_file}" "${compile_log}" "${link_log}" "${run_err}"

  # 1) lat -> s
  if ! "${COMPILER}" "${lat}" > /dev/null 2> "${compile_log}"; then
    red "COMPILE_FAIL ${name}"
    echo "  log: ${compile_log}"
    ((fail+=1))
    echo
    continue
  fi

  # 2) link s + runtime
  if ! gcc -no-pie "${sfile}" "${RUNTIME_O}" -o "${exe}" > "${link_log}" 2>&1; then
    red "LINK_FAIL ${name}"
    echo "  log: ${link_log}"
    ((fail+=1))
    echo
    continue
  fi

  # 3) run
  if ! "${exe}" > "${got}" 2> "${run_err}"; then
    red "RUN_FAIL ${name}"
    echo "  stderr: ${run_err}"
    ((fail+=1))
    echo
    continue
  fi

  # 4) compare if expected exists
  if [[ -f "${expected}" ]]; then
    tr -d '\r' < "${got}" > "${got}.tmp" && mv "${got}.tmp" "${got}"

    if diff -u "${expected}" "${got}" > "${diff_file}"; then
      green "OK ${name}"
      ((ok+=1))
      rm -f "${compile_log}" "${link_log}" "${run_err}" "${diff_file}"
    else
      red "FAIL ${name}"
      echo "----- expected -----"
      cat "${expected}"
      echo "----- got -----"
      cat "${got}"
      echo "----- diff -----"
      cat "${diff_file}"
      ((fail+=1))
    fi
  else
    yellow "NOOUTPUT ${name} (saved: ${name}.got)"
    ((noout+=1))
    rm -f "${compile_log}" "${link_log}" "${run_err}"
  fi

  echo
done

echo "Summary:"
echo "  total:     ${total}"
echo "  ok:        ${ok}"
echo "  fail:      ${fail}"
echo "  no output: ${noout}"

exit $(( fail > 0 ))
