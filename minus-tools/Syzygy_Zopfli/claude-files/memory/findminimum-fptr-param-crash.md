---
name: findminimum-fptr-param-crash
description: "FindMinimum is unverifiable (fptr-param crash); fails at goto-instrument not cbmc, all 52 mutants uncheckable"
metadata: 
  node_type: memory
  type: project
  originSessionId: 9f932c7b-18e5-4d58-b897-346362c551b4
---

FindMinimum(FindMinimumFun f, void *context, size_t start, size_t end, double
*smallest) in zopfli.c (~line 5235) takes a function-pointer parameter, so it
joins the fptr-param unverifiable family with [[gcmmc-fptr-param-crash]],
[[gbl-fptr-param-crash]], [[lor-fptr-param-crash]]. Confirmed empirically
2026-06-27 via tools.run_cbmc.run_cbmc.

**Difference from the others:** here the crash is at the **goto-instrument**
(enforce-contract) step, NOT cbmc, returncode 6 / failed_step=goto-instrument.
FindMinimumFun = `double(size_t, void*)`; CBMC's function-pointer-removal pass
resolves the `f(i,context)` call to SplitCost — the ONLY function with that
signature — then inlines SplitCost's whole call tree (SplitCost → EstimateCost →
ZopfliCalculateBlockSizeAutoType → ... → ZopfliLengthLimitedCodeLengths →
self-recursive BoundaryPM). Enforce-contract inlining aborts with `Recursive call
to 'BoundaryPM' during inlining` + `Numeric exception : 0`. Crashes identically
with OR without a contract (verified both).

The run_cbmc recursion-inlining retry does NOT help: its trigger
`has_recursion_inlining_error_message` looks for `Recursive call to 'FindMinimum'`
but the message names BoundaryPM, and FindMinimum isn't self-recursive so the
retry's `include_self` adds nothing. get_in_file_callees_for('FindMinimum')==[]
(f is the only callee, a nondet external — never in --replace-call-with-contract),
so SplitCost is never abstracted away.

**52 mutants** (most on the simple-scan branch `v<best`/`i<end`/`end-start<1024`
and the recursive branch comparators `best>lastbest`/`vp[i]<best`/`end-start<=NUM`/
loop bounds/`besti==0`/`besti==NUM-1`). All uncheckable — original crashes, every
mutant scores identically. Kill score 0, unimprovable without dfcc/obeys_contract
(not in the fixed harness).

**How to apply:** Don't chase kills. Left strongest SOUND spec: requires(f!=NULL),
requires(start<end), requires(is_fresh(smallest,...)), assigns(*smallest),
ensures(return in [old(start), old(end))). Added an explanatory NOTE comment in
source. Script: /app/_score_findmin.py.
