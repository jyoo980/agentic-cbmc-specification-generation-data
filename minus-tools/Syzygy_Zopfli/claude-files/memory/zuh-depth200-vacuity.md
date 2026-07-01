---
name: zuh-depth200-vacuity
description: "ZopfliUpdateHash (zopfli.c) — verifies w/ exact-value contract but 0/40 kills @depth200; 8 nested-array is_fresh push whole body past frontier, w_ok lever fails on nested fields, don't chase"
metadata: 
  node_type: memory
  type: project
  originSessionId: cc2d7fe2-ad10-4fe9-b1d4-f91ed952b390
---

`ZopfliUpdateHash(array, pos, end, h)` — rolling-hash + chain + run-length update over a `ZopfliHash` struct-of-arrays. Already has a strong concrete-regime contract (pos==8, end==pos+4, val==0, array bytes==7, head/head2[7]==5 etc.) with exact-value `ensures` for all 9 outputs (val, val2, hashval[hpos], prev[hpos], head[7], same[hpos], hashval2[hpos], prev2[hpos], head2[7]). Verifies with `goto-instrument --partial-loops --unwind 5` + `--replace-call-with-contract UpdateHashValue --enforce-contract` + `cbmc --depth 200` (script /app/_verify_zuh.sh).

**Kill score: 0/40 @depth200** (40 avocado mutants on lines 3544/3546/3555/3557/3559/3560/3566/3568). Pure [[avocado-depth200-vacuity]]: the 8 mandatory nested-array `is_fresh` preconditions (head,head2,hashval,hashval2,prev,prev2,same + array) consume ~1000+ depth in the enforce prologue, pushing the entire body and every postcondition assertion past the hardcoded --depth 200 frontier. Measured: an early-body branch mutant (L3546, affects prev[hpos]) is killable only at depth ~1200; even at depth 600 only 2/40 die (the two array-OOB reads at L3544: `array[pos+ZMM+1]`→array[12] OOB and `array[pos-ZMM-1]`→array[4] unconstrained — caught at the access, before postconditions).

**Levers that FAIL (don't retry):**
- `is_fresh`→`w_ok`/`r_ok` + `__CPROVER_separate(...)` on the nested field pointers: w_ok on a fresh struct's nondet field pointers reports `pointer_primitives ... W_OK ... FAILURE` (not assumed). Same nested-field failure recorded for [[zslld-depth200-vacuity]]. is_fresh is mandatory for nested array fields.
- Dropping `--replace-call-with-contract UpdateHashValue` (inline the 1-line callee): still 0/40, and even the 2 OOB mutants now survive @200 — the is_fresh prologue, not the callee contract, is the bottleneck.

Strong spec, inherent vacuity at the scoring depth. Same regime/verdict as [[zslld-depth200-vacuity]], [[silmc-depth200-vacuity]], [[zlz77gh-depth200-vacuity]]. Don't chase. Kill script: /app/kill_zuh.sh (DEPTH env var; /app/kill_zuh_noreplace.sh is the inline variant).
