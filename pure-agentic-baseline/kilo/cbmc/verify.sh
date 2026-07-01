#!/usr/bin/env bash
# Verify a single function's CBMC contract using the documented
# goto-cc / goto-instrument --enforce-contract / cbmc flow.
#
# Usage: ./verify.sh <function> [unwind] [extra cbmc args...]
#
# Compiles kilo.c together with stubs.c, instruments the named function so its
# contract is asserted, then model-checks it with the function as entry point.
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../kilo.c"
STUBS="$HERE/stubs.c"
FN="${1:?usage: verify.sh <function> [unwind] [extra cbmc args...]}"
UNWIND="${2:-30}"
shift || true
shift || true
EXTRA="$*"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

if ! goto-cc -std=c99 --function "$FN" "$SRC" "$STUBS" -o "$WORK/in.goto" 2> "$WORK/cc.log"; then
    echo "[$FN] goto-cc FAILED"; cat "$WORK/cc.log"; exit 2
fi

if ! goto-instrument --enforce-contract "$FN" "$WORK/in.goto" "$WORK/chk.goto" \
        > "$WORK/inst.log" 2>&1; then
    echo "[$FN] goto-instrument FAILED"; cat "$WORK/inst.log"; exit 2
fi

# shellcheck disable=SC2086
cbmc "$WORK/chk.goto" --function "$FN" --unwind "$UNWIND" $EXTRA 2>&1 \
    | tee "$WORK/cbmc.log" \
    | grep -iE ": FAILURE|: UNKNOWN|VERIFICATION|\*\* [0-9]" || true

if grep -q "VERIFICATION SUCCESSFUL" "$WORK/cbmc.log"; then
    echo "[$FN] PASS"
    exit 0
else
    echo "[$FN] FAIL"
    exit 1
fi
