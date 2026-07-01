#!/usr/bin/env bash
# Verify a single function's CBMC contract via DFCC contract enforcement.
# Usage: ./verify.sh <FunctionName> [extra cbmc args...]
# Requires a harness named harness_<FunctionName> in harness.c.
set -u
cd "$(dirname "$0")"
FN="$1"
shift || true

INC="-I cbmc_include"
UNWIND="${UNWIND:-6}"

# Compile source + stubs, with main() invoking the chosen harness.
goto-cc $INC -DHARNESS="harness_${FN}" -o harness.gb harness.c cbmc_stubs.c 2>cc.log \
  || { echo "GOTO-CC FAILED"; cat cc.log; exit 1; }

# Remove stdin/driver bodies that pull in getchar (unsupported 'input' by DFCC).
goto-instrument \
  --remove-function-body read_stdin_to_bytes \
  --remove-function-body zopfli_unused_main \
  --remove-function-body run_all_tests \
  --remove-function-body single_test \
  harness.gb harness_pruned.gb 2>gp.log || { echo "PRUNE FAILED"; cat gp.log; exit 1; }

# Instrument: enforce the target's contract, entry point main.
goto-instrument \
  --dfcc main \
  --enforce-contract "$FN" \
  harness_pruned.gb harness_inst.gb 2>gi.log || { echo "GOTO-INSTRUMENT FAILED"; tail -8 gi.log; exit 1; }

# Default to a strong check set. Functions that rely on intentional unsigned
# wraparound or narrowing can override via CHECKS env (e.g. drop a check).
CHECKS="${CHECKS:---bounds-check --pointer-check --div-by-zero-check --pointer-overflow-check --conversion-check --signed-overflow-check --unsigned-overflow-check --float-overflow-check --nan-check}"

cbmc harness_inst.gb \
  $CHECKS \
  --unwind "$UNWIND" "$@" 2>&1 | tail -45
