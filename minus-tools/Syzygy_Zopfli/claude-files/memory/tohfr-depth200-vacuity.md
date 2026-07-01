---
name: tohfr-depth200-vacuity
description: TryOptimizeHuffmanForRle scores 0/9 kills — all mutants are past the depth-200 horizon
metadata: 
  node_type: memory
  type: project
  originSessionId: f1700b1b-fa09-438c-9186-d0d3d54e5672
---

`TryOptimizeHuffmanForRle` (zopfli.c) verifies with a sound contract
(full is_fresh/<=15/lstart<=lend/lz77 preconditions forwarded from its callees,
`__CPROVER_assigns(object_whole(ll_lengths), object_whole(d_lengths))`,
`__CPROVER_ensures(__CPROVER_return_value >= 26)`) but scores **0/9 kills**.

**Why:** another instance of [[avocado-depth200-vacuity]]. All 9 mutants sit on the
final `if (treesize2+datasize2 < treesize+datasize)` / the two `return` lines. The
function inlines two `CalculateTreeSize` (each 8× `EncodeTree`), two
`CalculateBlockSymbolSizeGivenCounts`, two `OptimizeHuffmanForRle`, and two
`ZopfliCalculateBitLengths`→`ZopfliLengthLimitedCodeLengths` — the callee
contracts use `__CPROVER_is_fresh`, whose `--replace-call-with-contract`
instrumentation needs `malloc`/`__CPROVER_replace_requires_is_fresh` bodies that
aren't present, so goto-instrument silently leaves every callee inlined (identical
to the canonical [[calculatetreesize-depth200-vacuity]] flow). That inlined body
exhausts `--depth 200` long before the comparison/returns, so the postcondition
site is unreachable. Verified directly: an impossible `ensures(return_value >=
9999999)` still reports VERIFICATION SUCCESSFUL → vacuous.

**How to apply:** don't chase these 9 kills; the harness's hardcoded `--depth 200`
makes them inherently unkillable. Kill harness: /app/kill_tohfr.sh.
