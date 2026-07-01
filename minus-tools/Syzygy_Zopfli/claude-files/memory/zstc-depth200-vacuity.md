---
name: zstc-depth200-vacuity
description: "ZopfliSublenToCache maxes 10/38 kills; loop pushes post-loop asserts past depth 200 + unwind-5 unreachable branches, don't chase"
metadata: 
  node_type: memory
  type: project
  originSessionId: 4e331a9f-eb50-4b02-9139-e2b1aa138e5e
---

ZopfliSublenToCache (zopfli.c) verifies with a clean general contract:
is_fresh(lmc) + pos<=1024 + length<=1024 + is_fresh(lmc->sublen, CACHE_LEN*pos*3+24-byte window)
+ is_fresh(sublen, (length+2)) + assigns(object_from(&lmc->sublen[CACHE_LEN*pos*3])).
Callee ZopfliMaxCachedSublen is replace-call-with-contract. Scores **10/38**.

**Why the 28 survivors are a hard ceiling under the canonical pipeline**
(goto-cc; --partial-loops --unwind 5; --replace-call-with-contract ZopfliMaxCachedSublen
--enforce-contract; cbmc --depth 200):

- **Depth-200 vacuity** (same as [[avocado-depth200-vacuity]]): the 5×-unwound loop pushes
  ALL post-loop code past depth 200. Confirmed: the final `assert(bestlength==ZopfliMaxCachedSublen)`
  mutant survives@200/240 but is KILLED@260/600. Post-loop comparison mutants (bestlength==length,
  the `if(j<CACHE_LENGTH)` branch) survive even @240. Gap to close is ~60 GOTO steps — too large
  to recover by trimming is_fresh prologue.
- **r_ok trick fails here** (unlike [[zlz77gbr-rok-beats-vacuity]]): swapping is_fresh→r_ok for the
  `sublen` param gives 0/38 (no backing object in non-DFCC enforce → reads vacuous). lmc->sublen is a
  havoc'd field so w_ok fails outright ("pointer invalid"). All three buffers NEED is_fresh.
- **Unwind-5 unreachable/equivalent branches**: the `else` branch (j>=CACHE_LENGTH=8) needs >8 loop
  iterations, impossible under unwind 5 → its asserts (`bestlength<=length` ×5 mutants) are
  inherently unkillable. `j>CL`/`j==CL` mutants at the break and post-loop branch are equivalent
  under unwind 5 (j never reaches 8).
- **In-loop OOB writes not caught**: negative-index mutants `cache[j*3-1]`, `cache[j*3-2]` land
  inside the large backing object (just outside the `assigns` frame). Non-DFCC enforce checks assigns
  only object-granular, not byte-precise, so they survive. Making the cache base object-offset-0
  (pos==0) would catch them but failed to verify and badly weakens the spec — not worth +2.

10/38 = loop guard (5) + length guard (3) + in-loop condition mutants (2). Don't chase the rest.
Scripts: /app/_verify_zstc.sh, /app/kill_zstc.py.

**Re-confirmed 2026-06-27 (10/38, VERIFICATION SUCCESSFUL).** Tested the one lever the
memory hadn't explicitly closed: **loop contracts are INERT under the avocado pipeline.**
The scoring pipeline is hardcoded in /app/tools/run_cbmc.py (`_CBMC_UNWIND=5`, `_CBMC_DEPTH=200`):
`goto-instrument --partial-loops --unwind 5` → `--enforce-contract` (+ `--replace-call-with-contract`
for callees) → `cbmc --depth 200`. It NEVER passes `--apply-loop-contracts`, so any
`__CPROVER_loop_invariant` in the source is silently ignored — proved by inserting
`__CPROVER_loop_invariant(0 == 1)` (false) into the ZSTC loop: still VERIFICATION SUCCESSFUL.
=> the [[zic-dfcc-loopcontracts-12of16]] "dfcc + loop contracts" trick does NOT apply to the
fixed avocado scorer and cannot rescue any depth-200-vacuity function here. Don't add loop
contracts to chase post-loop-assert mutants — they do nothing.
