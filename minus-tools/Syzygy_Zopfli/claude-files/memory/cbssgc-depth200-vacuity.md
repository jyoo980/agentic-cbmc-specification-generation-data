---
name: cbssgc-depth200-vacuity
description: "CalculateBlockSymbolSizeGivenCounts scores 0 kills under hardcoded cbmc --depth 200; inherent vacuity, don't chase"
metadata: 
  node_type: memory
  type: project
  originSessionId: e9aa002b-24c5-4309-817a-73a12c48dbf8
---

`CalculateBlockSymbolSizeGivenCounts` in /app/Syzygy_Zopfli/c_code/zopfli.c verifies but kills 0/21 mutants under the mandated `--partial-loops --unwind 5 --depth 200` pipeline. This is inherent depth-200 vacuity, not a weak spec — same pattern as the sibling [[cbsss-depth200-vacuity]].

**Why:** The function branches on `lstart + 3*ZOPFLI_NUM_LL > lend` (small-window → CalculateBlockSymbolSizeSmall) vs an else path with three histogram loops. The 21 mutants are 6 guard mutants (line 354) + 15 loop-bound mutants (3 loops). I added complementary in-branch assertions (`lstart+3*NUM_LL > lend` / `<= lend`) that DO kill all 6 guard mutants at unbounded depth (confirmed: assertion FAILS for each). But each full-table `__CPROVER_is_fresh` precondition (ll_counts/d_counts/ll_lengths/d_lengths, 288/32 entries) costs ~100 GOTO steps; the kill only appears at `--depth ~450+`. At depth 200 the branch point isn't reached → assertions vacuously SUCCESS → mutants survive.

Measured the tension directly: 0 is_fresh → branch assertion reachable (mutant killed) but original FAILS (body derefs unsafe within depth 200); 2 is_fresh → branch already past depth 200 (vacuous). No middle ground without depth-tuned padding, which is a forbidden CBMC-arg hardcode. Loop-bound mutants are additionally unkillable under unwind-5 (the differing high index — 256/286/30 — is never reached; `!=` variants are equivalent mutants).

Re-confirmed 2026-06-27 with a cleaner measurement: an `assert(0)` at the top of the else (histogram) body is vacuously SUCCESS at depth 200 but FAILS unbounded (body unreachable). Threshold-probing the precondition cost: full contract → body reachable only ~depth 450+; removing the entire lz77 chain (the then-branch-only is_fresh) drops it to ~265; but the 4 mandatory histogram is_fresh (ll_counts/d_counts/ll_lengths/d_lengths — needed for the else loops' array reads to be memory-safe) ALONE keep the body above ~depth 265. So even deleting every precondition not needed by the else path cannot get the body reachable within depth 200. Guarding the lz77 is_fresh under the small-window condition does NOT help — goto-instrument allocates the fresh object regardless of the implication guard. Hard floor > 200; 0/21 is inherent.

**How to apply:** Keep the strong, faithful contract (full-size table is_fresh, conditional lz77 well-formedness guarding the Small call, `return_value >= ll_lengths[256]`, the two sound branch assertions). Accept 0 kills; do not chase. Helper scripts left in c_code: `kill_test_CBSSGC.sh`, `exp_contract.py`.
