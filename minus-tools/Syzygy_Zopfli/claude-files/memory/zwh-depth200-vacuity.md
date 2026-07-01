---
name: zwh-depth200-vacuity
description: "ZopfliWarmupHash verifies but 0/8 kills @depth200; max 6/8 only at high depth, 2 equivalent mutants"
metadata: 
  node_type: memory
  type: project
  originSessionId: d134a65b-0117-4554-bf9c-d7db282b2d15
---

ZopfliWarmupHash (zopfli.c) verifies SUCCESSFUL @--depth 200 with full closed-form
spec (is_fresh(h)+val∈[0,HASH_MASK]+pos<end+is_fresh(array,end); two conditional-regime
exact-value ensures using the UpdateHashValue rolling-hash closed form, plus range ensures).
Flow: goto-cc → --replace-call-with-contract UpdateHashValue --enforce-contract → cbmc --depth 200.

0/8 kills @depth200 — inherent depth-200 vacuity. Even ONE UpdateHashValue
contract-replacement expansion (carrying its is_fresh(h,sizeof(*h)) precondition,
asserted at the call site) costs ~260+ steps before any postcondition is reachable
(kill threshold measured at ~280; 3/8 @300, 6/8 @2000). Can't weaken UpdateHashValue's
contract (own verified spec, grader replaces it identically). Same wall as [[zuh-depth200-vacuity]].

Max kill ceiling = 6/8 (only @depth≥~2000). The 2 survivors are EQUIVALENT mutants,
don't chase: (a) `pos+1 != end` — under precondition pos<end ⟹ pos+1<=end, so
`!= end` ⟺ `< end`; (b) `array[pos-0]` == `array[pos+0]`.

r_ok(array,end) lever: lowers threshold a bit (3/8 @300 vs is_fresh needing higher)
but still >200, and is_fresh is sounder (guarantees array⊥h for clean postcond), so
kept is_fresh. Scripts: /app/_verify_zwh.sh, /app/kill_zwh.sh.
