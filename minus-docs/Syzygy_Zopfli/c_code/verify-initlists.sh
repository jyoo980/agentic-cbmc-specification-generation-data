#!/bin/bash
#
# Manually run CBMC on each InitLists mutant file, using the same manual directions provided to agents.
# 
# The loop invariant introduced by the agent for this treatment breaks mutant generation (the agent's
# conversation trace makes a note of this and moves on).
#

set -u

FUNCTION=InitLists
CALLEES=(InitNode)

MUTANTS=(
    zopfli__InitLists_mutant_0.c
    zopfli__InitLists_mutant_1.c
    zopfli__InitLists_mutant_2.c
    zopfli__InitLists_mutant_3.c
    zopfli__InitLists_mutant_4.c
)

# Build the --replace-call-with-contract flags for every callee.
REPLACE_FLAGS=()
for callee in "${CALLEES[@]}"; do
    REPLACE_FLAGS+=(--replace-call-with-contract "${callee}")
done

for mutant in "${MUTANTS[@]}"; do
    echo "=============================================================="
    echo "Verifying ${FUNCTION} in ${mutant}"
    echo "=============================================================="

    headers="stubs"
    base="${mutant%.c}"
    goto="${base}.goto"
    checking="checking-${base}-contracts.goto"

    goto-cc -I /app/experimental-data/minus-docs/Syzygy_Zopfli/stubs -o "${goto}" "${mutant}" --function "${FUNCTION}" > /dev/null 2>&1 \
    && goto-instrument --partial-loops --unwind 5 "${goto}" "${goto}" > /dev/null 2>&1 \
    && goto-instrument "${REPLACE_FLAGS[@]}" --enforce-contract "${FUNCTION}" "${goto}" "${checking}" > /dev/null 2>&1 \
    && cbmc "${checking}" --function "${FUNCTION}" --depth 200 > /dev/null 2>&1

    status=$?
    if [ "${status}" -eq 0 ]; then
        echo "RESULT: ${mutant} -> CBMC SUCCEEDED (mutant survived)"
    else
        echo "RESULT: ${mutant} -> CBMC FAILED with status ${status} (mutant killed)"
    fi
    echo
done
