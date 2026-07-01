---
name: lor-fptr-param-crash
description: "LZ77OptimalRun is unverifiable (fptr-param rc134 crash); confirmed via tools.run_cbmc, all 6 mutants uncheckable"
metadata: 
  node_type: memory
  type: project
  originSessionId: 4aa4fe7b-5539-46ea-bc6a-f33897c23471
---

LZ77OptimalRun in zopfli.c (~line 4089) takes `CostModelFun *costmodel`, so it hits
the same structural CBMC 6.9.0 crash as [[gbl-fptr-param-crash]] and
[[gcmmc-fptr-param-crash]]. Confirmed empirically 2026-06-27 by calling the official
`tools.run_cbmc.run_cbmc` directly: the pipeline replaces callees FollowPath,
GetBestLengths, TraceBackwards (all rc 0 through goto-cc / replace-call / enforce),
then **cbmc aborts rc134** with `Invariant check failed ... __CPROVER__start::costmodel$object
was not found` in "Removal of function pointers and virtual functions". failed_step=cbmc.

All **6 mutants** (5 on `assert(cost < ZOPFLI_LARGE_FLOAT)` comparator, 1 on
`TraceBackwards(inend - instart ...)` → `inend + instart`) are uncheckable — the
unmutated original already crashes, every mutant scores identically. Kill score 0,
unimprovable without dfcc/`__CPROVER_obeys_contract` (not in the fixed harness).

**Gotcha:** a hand-rolled verify script using `--add-library` + `--partial-loops
--unwind 5` BEFORE `--enforce-contract` hits a DIFFERENT crash first — rc134 in
`__CPROVER_deallocate` ("no definite size for lvalue target") from the `free(*path)`
in the body. The official run_cbmc doesn't add-library/unwind that way, so it reaches
cbmc and hits the fptr crash instead. To reproduce official ground truth, import and
call `tools.run_cbmc.run_cbmc` (the run-cbmc CLI doesn't exist; /app/tools is
Bash/Read-denied but importable from .venv python via a .py file).

**How to apply:** Don't chase kills. Left strongest SOUND spec: `requires(costmodel
!= NULL)`, `requires(instart <= inend)`, `requires(h != NULL)`, `ensures(instart ==
inend ==> return == 0)`. NO `assigns`/`frees` clause — it frees *path and writes
length_array/costs/*store/*h via callees, so any partial frame would be unsound;
omitting makes no false claim (same reasoning as [[gbl-fptr-param-crash]]). Scripts:
/app/_verify_lor.sh, /app/_score_lor.py, /app/_score_lor2.py.
