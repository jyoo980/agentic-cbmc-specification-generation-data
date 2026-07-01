---
name: cbmc-calculatetreesize-minselect-unkillable
description: Why CalculateTreeSize in zopfli.c caps at 3/12 kills (min-select + loop-count survivors are unobservable under EncodeTree contract replacement)
metadata: 
  node_type: memory
  type: project
  originSessionId: baef7430-7c79-47e3-b0e4-f1526ba87a95
---

`CalculateTreeSize(ll_lengths, d_lengths)` in `Syzygy_Zopfli/c_code/zopfli.c` loops 8× calling `EncodeTree(...,i&1,i&2,i&4,0,0,0)` and keeps the minimum size.

Spec that works (sound, NON-vacuous): mirror EncodeTree's preconditions — `is_fresh(ll_lengths, ZOPFLI_NUM_LL*sizeof)`, `is_fresh(d_lengths, ZOPFLI_NUM_D*sizeof)`, both forall entries `<= 15` — plus `assigns()` and `ensures(return_value >= 26)`. Verifies at depth 200, kills 3/12 (proves body reached — NOT the is_fresh/malloc vacuity wall; only 2 pointer params here).

The 9 survivors (line 1133 min-select `if (result==0 || size < result)` relational/logical mutants; line 1128 loop bound `<8`) are tooling-forced unkillable: `EncodeTree` has its OWN contract, so each of the 8 calls is contract-REPLACED by its ensures `return_value >= 26`, i.e. a fresh nondeterministic value. The return is then min of 8 unconstrained nondeterministic values — no postcondition over the return can observe which one the selection logic picks. Loop-count mutants are also dead: `<8`->`<=8` just re-runs the i=0 (all-flags-false) combo, `<8`->`!=8` is semantically identical. Killing min-select would need EncodeTree's contract to return a value DETERMINISTICALLY tied to use_16/17/18 — not expressible simply, and the C code / EncodeTree spec must not change. Left the strong sound spec in place. See [[cbmc-depth200-isfresh-vacuity]].
