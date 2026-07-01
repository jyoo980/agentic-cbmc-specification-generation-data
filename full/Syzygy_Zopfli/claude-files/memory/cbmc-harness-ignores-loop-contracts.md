---
name: cbmc-harness-ignores-loop-contracts
description: run-cbmc never passes --apply-loop-contracts, so __CPROVER_loop_invariant is inert; loops are partial-unwound at 5
metadata:
  node_type: memory
  type: project
  originSessionId: full
---

`run-cbmc` (`/app/tools/run_cbmc_and_mutation_testing.py`) drives a fixed
pipeline: `goto-cc` -> `goto-instrument [--add-library]` -> `goto-instrument
--partial-loops --unwind 5` -> `goto-instrument --replace-call-with-contract <callees>
--enforce-contract <fn>` -> `cbmc ... --depth 200`. Constants: `_CBMC_UNWIND = 5`,
`_CBMC_DEPTH = 200`.

**Key finding:** there is NO `--apply-loop-contracts` step. So
`__CPROVER_loop_invariant` / `__CPROVER_decreases` / loop `__CPROVER_assigns` are
SILENTLY IGNORED — they do not abstract the loop, do not become reachable proof
obligations, and do not help kill loop-guard mutants. Loops are instead
partial-unwound at 5 (`--partial-loops` keeps early-exit paths, drops the
unwinding-assertion), so a loop of N>5 iterations only ever executes <=5 and the
code AFTER the loop runs with a truncated result.

Consequence for `CalculateBlockSymbolSizeGivenCounts` (the histogram branch with
three constant-bound loops of 256/29/30): even after forcing the else branch
(`lstart==0 && lend>=ZOPFLI_NUM_LL*3`, avoiding the lz77 is_fresh of the small
branch), the proof is VACUOUS — confirmed by `__CPROVER_ensures(1==0)` still
passing, and still passing even with the `forall` preconditions removed. The
`--depth 200` budget is spent on the 4 is_fresh arrays + the unwound loop bodies
(loop2/loop3 each replace 2 extrabits calls per iter) before the post-loop
`return` is reached. All 21 mutants survive (kill 0/21) — they sit on the loop
guards / the `if` condition, whose effect is only visible in the never-reached
post-loop result. Same depth wall as [[cbmc-calculateblocksymbolsizesmall-depth-vacuity]].

**Spec kept (strongest sound):** force-else; is_fresh on the 4 count/length
arrays sized 288/32; constant-bound forall value bounds (len in [1,16), count in
[1,2^20)); assigns(); ensures return in [316, finite-upper-bound]; plus full loop
invariants + decreases. Verifies (exit 0). The loop contracts are inert here but
are the correct strong spec and discharge fully under a loop-contract-enabled CBMC.
Takeaway: do NOT expect loop contracts to raise kill score under this harness; for
any function whose only mutable operators live in/after a long loop, kill is
capped at 0 by the depth-200 + unwind-5 + no-loop-contracts configuration.
