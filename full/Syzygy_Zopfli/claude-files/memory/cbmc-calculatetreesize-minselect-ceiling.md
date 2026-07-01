---
name: cbmc-calculatetreesize-minselect-ceiling
description: CalculateTreeSize verifies soundly but kill score capped at 0.25 by EncodeTree contract abstraction
metadata: 
  node_type: memory
  type: project
  originSessionId: de02238d-32e4-47c6-9805-36fc44b8fa62
---

CalculateTreeSize (zopfli.c:1410) verifies soundly with a contract mirroring
EncodeTree's size-only preconditions: `is_fresh(ll_lengths, ZOPFLI_NUM_LL*sizeof(unsigned))`,
`is_fresh(d_lengths, ZOPFLI_NUM_D*sizeof(unsigned))`, `assigns()`, and
`ensures(return_value >= 14 + 4*3)`.

**Kill score ceiling 0.25 (3/12).** The 9 survivors are all the min-selection
mutants on `if (result == 0 || size < result)` (< -> <=,>,>=,==,!=; ==->!=;
||->&&) and the loop-bound mutants on `i < 8` (<=, !=). They cannot be killed:
when verifying CalculateTreeSize, CBMC replaces the 8 EncodeTree calls with its
contract, which returns an arbitrary independent value >= 26. min/max/equality
selection all yield a value >= 26 — the only assertable fact. `i<8 -> i!=8` is
semantically equivalent (both iterate 0..7). Killing these would require an exact
characterization of EncodeTree's return as a function of use_16/use_17/use_18,
which the real code does not provide. Same contract-abstraction ceiling family as
[[cbmc-harness-ignores-loop-contracts]] and the depth-vacuity notes.

Build: needs `-I /app/Syzygy_Zopfli/c_code -I /app/Syzygy_Zopfli/stubs` (file
includes `<x86_64-linux-gnu/bits/types/FILE.h>` from the stubs dir; absolute -I
per [[cbmc-mutation-include-must-be-absolute]]).
