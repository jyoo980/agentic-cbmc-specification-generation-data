#!/usr/bin/env bash
# Verify a function's CBMC contract using the DFCC (dynamic frame condition
# checking) flow, which -- unlike the deprecated path used by verify.sh --
# supports `frees` clauses and loop contracts.
#
# Usage: ./verify_dfcc.sh <function> [unwind] [extra cbmc args...]
#
# Requires a harness named h_<function> in harness.c.  Callees that have their
# own contracts can be abstracted by listing them (space separated) in the
# REPLACE environment variable, e.g. REPLACE="editorFreeRow editorUpdateRow".
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
HARNESS="$HERE/harness.c"
STUBS="$HERE/stubs.c"
FN="${1:?usage: verify_dfcc.sh <function> [unwind] [extra cbmc args...]}"
UNWIND="${2:-30}"
shift || true
shift || true
EXTRA="$*"

REPL_ARGS=""
for c in ${REPLACE:-}; do REPL_ARGS="$REPL_ARGS --replace-call-with-contract $c"; done

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

if ! goto-cc -std=c99 --function "h_$FN" "$HARNESS" "$STUBS" -o "$WORK/in.goto" 2> "$WORK/cc.log"; then
    echo "[$FN] goto-cc FAILED"; cat "$WORK/cc.log"; exit 2
fi

# shellcheck disable=SC2086
if ! goto-instrument --dfcc "h_$FN" --enforce-contract "$FN" $REPL_ARGS \
        --apply-loop-contracts "$WORK/in.goto" "$WORK/chk.goto" \
        > "$WORK/inst.log" 2>&1; then
    echo "[$FN] goto-instrument FAILED"; tail -20 "$WORK/inst.log"; exit 2
fi

# shellcheck disable=SC2086
cbmc "$WORK/chk.goto" --unwind "$UNWIND" $EXTRA 2>&1 \
    | tee "$WORK/cbmc.log" \
    | grep -iE ": FAILURE|: UNKNOWN|VERIFICATION|\*\* [0-9]" || true

if grep -q "VERIFICATION SUCCESSFUL" "$WORK/cbmc.log"; then
    echo "[$FN] PASS"; exit 0
else
    echo "[$FN] FAIL"; exit 1
fi
